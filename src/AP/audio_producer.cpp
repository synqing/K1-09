#include "audio_producer.h"
#include <math.h>   // for lrintf
#include <string.h>
#if AUDIO_DIAG_FLUX
#include <Arduino.h>
#endif
#include "debug/debug_flags.h"
#include "window_lut.h"
#include "goertzel_backend.h"
#include "levels.h"
#include "chroma.h"
#include "flux.h"
#include "audio_params.h"
#include "tempo_lane.h"
#include "bands.h"

// ========= static storage (no heap) =========
volatile uint32_t audio_frame_epoch = 0;

static AudioFrame g_public;   // published frame (read by VP)
static AudioFrame g_staging;  // written locally per tick

// EMA state for smooth_spectral
static int32_t smooth_state_q16[freq_bins];

using K1Lightwave::Audio::Bands4_64;

#if AUDIO_DIAG_FLUX
namespace {
constexpr uint32_t kFluxDiagPeriodMs = (AUDIO_DIAG_FLUX_PERIOD_MS == 0u) ? 1u : AUDIO_DIAG_FLUX_PERIOD_MS;
constexpr size_t   kFluxDiagBuckets  = 6u;
constexpr float    kFluxDiagMaxLin   = 1.5f;

uint32_t flux_hist[kFluxDiagBuckets] = {};
uint32_t flux_samples = 0;
float    flux_sum = 0.0f;
float    flux_peak = 0.0f;
uint32_t flux_last_emit_ms = 0;
bool     flux_diag_active = false;
}

static void flux_diag_record(q16 flux_q16) {
  const bool enabled = debug_flags::enabled(debug_flags::kGroupTempoFlux);
  if (!enabled) {
    if (flux_diag_active) {
      flux_sum = 0.0f;
      flux_peak = 0.0f;
      flux_samples = 0u;
      for (size_t i = 0; i < kFluxDiagBuckets; ++i) {
        flux_hist[i] = 0u;
      }
    }
    flux_diag_active = false;
    return;
  }
  flux_diag_active = true;

  const float flux_lin = (float)flux_q16 / 65536.0f;
  flux_sum += flux_lin;
  ++flux_samples;
  if (flux_lin > flux_peak) {
    flux_peak = flux_lin;
  }

  float normalised = flux_lin / kFluxDiagMaxLin;
  if (normalised < 0.0f) normalised = 0.0f;
  if (normalised > 0.999f) normalised = 0.999f;  // keep last bucket for max
  const size_t bucket = (size_t)(normalised * (float)kFluxDiagBuckets);
  if (bucket < kFluxDiagBuckets) {
    ++flux_hist[bucket];
  }

  const uint32_t now = millis();
  if ((now - flux_last_emit_ms) < kFluxDiagPeriodMs) {
    return;
  }
  flux_last_emit_ms = now;

  const float avg = (flux_samples > 0u) ? (flux_sum / (float)flux_samples) : 0.0f;
  Serial.printf("[flux] avg=%.3f peak=%.3f samples=%lu buckets:",
                avg,
                flux_peak,
                static_cast<unsigned long>(flux_samples));
  for (size_t i = 0; i < kFluxDiagBuckets; ++i) {
    const float lo = (kFluxDiagMaxLin * (float)i) / (float)kFluxDiagBuckets;
    const float hi = (kFluxDiagMaxLin * (float)(i + 1u)) / (float)kFluxDiagBuckets;
    Serial.printf(" [%.2f-%.2f]=%lu",
                  lo,
                  hi,
                  static_cast<unsigned long>(flux_hist[i]));
  }
  Serial.println();

  flux_sum = 0.0f;
  flux_peak = 0.0f;
  flux_samples = 0;
  for (size_t i = 0; i < kFluxDiagBuckets; ++i) {
    flux_hist[i] = 0u;
  }
}
#endif

// Band splits (exclusive end indices) from the 4-band scheme.
static constexpr uint16_t band_low_start       = Bands4_64::kBandStart[0];
static constexpr uint16_t band_low_end         = Bands4_64::kBandEnd[0];
static constexpr uint16_t band_low_mid_start   = Bands4_64::kBandStart[1];
static constexpr uint16_t band_low_mid_end     = Bands4_64::kBandEnd[1];
static constexpr uint16_t band_presence_start  = Bands4_64::kBandStart[2];
static constexpr uint16_t band_presence_end    = Bands4_64::kBandEnd[2];
static constexpr uint16_t band_high_start      = Bands4_64::kBandStart[3];
static constexpr uint16_t band_high_end        = Bands4_64::kBandEnd[3];

// Snapshot pointer
const AudioFrame* acquire_spectral_frame() { return &g_public; }

bool audio_pipeline_init() {
  init_gaussian_window(0.4f);
  flux::init();
  chroma_map::init();
  tempo_lane::tempo_init();
  for (uint32_t k=0;k<freq_bins;++k) smooth_state_q16[k] = 0;
  audio_params::init();  // loads/persists smoothing alpha via NVS

  memset(&g_public,  0, sizeof(g_public));
  memset(&g_staging, 0, sizeof(g_staging));
  audio_frame_epoch = 0;
  return goertzel_backend::init();
}

void audio_pipeline_tick(const int32_t* q24_chunk, uint32_t t_ms) {
  // Remove per-chunk DC offset once so all downstream consumers see zero-mean audio.
  int64_t sum = 0;
  for (uint32_t i = 0; i < chunk_size; ++i) {
    sum += q24_chunk[i];
  }
  const int32_t mean = static_cast<int32_t>(sum / static_cast<int64_t>(chunk_size));

  int32_t centered_q24[chunk_size];
  for (uint32_t i = 0; i < chunk_size; ++i) {
    centered_q24[i] = q24_chunk[i] - mean;
  }

  // 1) Waveform snapshot (Q15)
  for (uint32_t i=0;i<chunk_size;++i) {
    float f = (float)centered_q24[i] / 8388607.0f;
    int32_t q15 = (int32_t)lrintf(f * 32767.0f);
    if (q15 > 32767) {
      q15 = 32767;
    } else if (q15 < -32768) {
      q15 = -32768;
    }
    g_staging.waveform[i] = (int16_t)q15;
  }

  // 2) Levels (linear)
  g_staging.vu_peak = levels::peak_q16_from_q24(centered_q24, chunk_size);
  g_staging.vu_rms  = levels::rms_q16_from_q24 (centered_q24, chunk_size);

  // 3) Frequency bins -> raw_spectral (linear)
  goertzel_backend::compute_bins(centered_q24, g_staging.raw_spectral);

  // 4) EMA smoothing (runtime-tunable alpha, saturated, non-negative)
  const uint32_t a = audio_params::get_smoothing_alpha_q16();
  for (uint32_t k=0;k<freq_bins;++k) {
    int32_t x = g_staging.raw_spectral[k];
    int32_t y = smooth_state_q16[k];
    int32_t dy = x - y;
    int32_t inc = (int32_t)(((int64_t)a * (int64_t)dy) >> 16);
    int32_t ynew = y + inc;
    if (ynew < 0) ynew = 0;
    if (ynew > 0x7FFFFFFF) ynew = 0x7FFFFFFF;
    smooth_state_q16[k] = ynew;
    g_staging.smooth_spectral[k] = ynew;
  }

  // 5) Chroma accumulation
  chroma_map::accumulate(g_staging.raw_spectral, g_staging.chroma);

  // 6) Band summaries (saturating sums)
  auto saturate_sum = [](int64_t value) -> int32_t {
    if (value > 0x7FFFFFFFLL) return 0x7FFFFFFF;
    if (value < static_cast<int64_t>(-0x80000000LL)) return static_cast<int32_t>(-0x80000000LL);
    return static_cast<int32_t>(value);
  };

  int64_t band_low_sum = 0;
  for (uint32_t k = band_low_start; k < band_low_end; ++k) band_low_sum += g_staging.raw_spectral[k];

  int64_t band_low_mid_sum = 0;
  for (uint32_t k = band_low_mid_start; k < band_low_mid_end; ++k) band_low_mid_sum += g_staging.raw_spectral[k];

  int64_t band_presence_sum = 0;
  for (uint32_t k = band_presence_start; k < band_presence_end; ++k) band_presence_sum += g_staging.raw_spectral[k];

  int64_t band_high_sum = 0;
  for (uint32_t k = band_high_start; k < band_high_end; ++k) band_high_sum += g_staging.raw_spectral[k];

  g_staging.band_low       = saturate_sum(band_low_sum);
  g_staging.band_low_mid   = saturate_sum(band_low_mid_sum);
  g_staging.band_presence  = saturate_sum(band_presence_sum);
  g_staging.band_high      = saturate_sum(band_high_sum);

  // 7) Spectral flux (linear) + clamp to sane ceiling before tempo
  g_staging.flux = flux::compute(g_staging.raw_spectral);
  static const q16 FLUX_Q16_CEIL = (q16)(1.25 * 65536.0); // 1.25 linear units
  if (g_staging.flux > FLUX_Q16_CEIL) g_staging.flux = FLUX_Q16_CEIL;
  if (g_staging.flux < 0)             g_staging.flux = 0;

#if AUDIO_DIAG_FLUX
  flux_diag_record(g_staging.flux);
#endif

  // 8) Tempo lane
  tempo_lane::tempo_ingest(centered_q24);
  tempo_lane::tempo_update(g_staging.tempo_bpm,
                           g_staging.beat_phase,
                           g_staging.beat_strength,
                           g_staging.beat_flag,
                           g_staging.tempo_confidence,
                           g_staging.tempo_silence);
  g_staging.tempo_ready = tempo_lane::tempo_ready() ? 1 : 0;

  // 9) Publish atomically: memcpy once → epoch++
  g_staging.t_ms = t_ms;
  uint32_t next_epoch = audio_frame_epoch + 1;
  g_staging.audio_frame_epoch = next_epoch;
  memcpy((void*)&g_public, (const void*)&g_staging, sizeof(AudioFrame));
  audio_frame_epoch = next_epoch;


#if AUDIO_PROFILE
  uint32_t dt = micros() - t0;
  tick_us_last = dt;
  if (dt > tick_us_max) tick_us_max = dt;
  const uint32_t budget_us = (uint32_t)((uint64_t)chunk_size * 1000000ULL / (uint64_t)audio_sample_rate);
  if (dt > budget_us) {
    ++overrun_cnt;
  }
  if ((audio_frame_epoch & 0x7Fu) == 0u) {
    Serial.printf("AP tick: %u us (max %u) | budget %u | overruns %u\n",
                  tick_us_last, tick_us_max, budget_us, overrun_cnt);
  }
#endif
}


/* ===================== End of audio_producer.cpp ===================== */

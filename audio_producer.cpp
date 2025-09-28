#include "audio_producer.h"
#include <math.h>
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

#include "mel_filterbank.h"   // NEW: perceptual post
#include "beat_gpio.h"        // NEW: optional beat pulse
#include "ap_profile.h"       // NEW: optional micro profiler

#if AUDIO_PROFILE
#include "esp_timer.h"
#endif

// ====== Optional: set to a valid GPIO to pulse on beat (scope/LA). =======
#ifndef BEAT_GPIO_PIN
#define BEAT_GPIO_PIN -1   // set e.g. 5 to enable
#endif

// ========= static storage (no heap) =========
volatile uint32_t audio_frame_epoch = 0;

static AudioFrame g_public;   // published frame (read by VP)
static AudioFrame g_staging;  // written locally per tick

// EMA state for smooth_spectral (legacy view; now produced by melproc)
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
      flux_sum = 0.0f; flux_peak = 0.0f; flux_samples = 0u;
      for (size_t i = 0; i < kFluxDiagBuckets; ++i) flux_hist[i] = 0u;
    }
    flux_diag_active = false; return;
  }
  flux_diag_active = true;
  const float flux_lin = (float)flux_q16 / 65536.0f;
  flux_sum += flux_lin; ++flux_samples;
  if (flux_lin > flux_peak) flux_peak = flux_lin;

  float normalised = flux_lin / kFluxDiagMaxLin;
  if (normalised < 0.0f) normalised = 0.0f;
  if (normalised > 0.999f) normalised = 0.999f;
  const size_t bucket = (size_t)(normalised * (float)kFluxDiagBuckets);
  if (bucket < kFluxDiagBuckets) ++flux_hist[bucket];

  const uint32_t now = millis();
  if ((now - flux_last_emit_ms) < kFluxDiagPeriodMs) return;
  flux_last_emit_ms = now;

  const float avg = (flux_samples > 0u) ? (flux_sum / (float)flux_samples) : 0.0f;
  Serial.printf("[flux] avg=%.3f peak=%.3f samples=%lu\n",
                avg, flux_peak, (unsigned long)flux_samples);

  flux_sum = 0.0f; flux_peak = 0.0f; flux_samples = 0;
  for (size_t i = 0; i < kFluxDiagBuckets; ++i) flux_hist[i] = 0u;
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
  // ===== PROFILE (optional) =====
  AP_PROF_INIT(750); // emit every 750 ms if AP_PROFILE_ENABLE=1

  // ===== WINDOW INIT: Hann SOLO LUT =====
  init_hann_window();

  // ===== Backends =====
  if (!goertzel_backend::init()) return false;
  flux::init();
  chroma_map::init();
  tempo_lane::tempo_init();
  melproc::melproc_init();                  // NEW: perceptual post

  if (BEAT_GPIO_PIN >= 0) {
    beat_gpio::beat_gpio_init((gpio_num_t)BEAT_GPIO_PIN); // optional
  }

  for (uint32_t k=0;k<freq_bins;++k) smooth_state_q16[k] = 0;

  audio_params::init();  // loads/persists smoothing alpha via NVS (kept for compatibility)

  memset(&g_public,  0, sizeof(g_public));
  memset(&g_staging, 0, sizeof(g_staging));
  audio_frame_epoch = 0;
  return true;
}

void audio_pipeline_tick(const int32_t* q24_chunk, uint32_t t_ms) {
  AP_PROF_BEGIN(Tick);

  // Remove per-chunk DC offset once so all downstream see zero-mean.
  int64_t sum = 0;
  for (uint32_t i = 0; i < chunk_size; ++i) sum += q24_chunk[i];
  const int32_t mean = (int32_t)(sum / (int64_t)chunk_size);

  int32_t centered_q24[chunk_size];
  for (uint32_t i = 0; i < chunk_size; ++i) centered_q24[i] = q24_chunk[i] - mean;

  // 1) Waveform snapshot (Q15) — for scope/visuals
  for (uint32_t i=0;i<chunk_size;++i) {
    float f = (float)centered_q24[i] / 8388607.0f;
    int32_t q15 = (int32_t)lrintf(f * 32767.0f);
    if (q15 > 32767) q15 = 32767; else if (q15 < -32768) q15 = -32768;
    g_staging.waveform[i] = (int16_t)q15;
  }

  // 2) Levels (linear)
  g_staging.vu_peak = levels::peak_q16_from_q24(centered_q24, chunk_size);
  g_staging.vu_rms  = levels::rms_q16_from_q24 (centered_q24, chunk_size);

  // 3) Frequency bins -> raw_spectral (Q16.16, linear)
  AP_PROF_BEGIN(Goertzel);
  goertzel_backend::compute_bins(centered_q24, g_staging.raw_spectral);
  AP_PROF_END(Goertzel);

  // 4) Perceptual post (A-weight -> floor -> soft-knee -> AR smoothing)
  //    Output is still 64 bins, Q16.16 linear. We store into smooth_spectral for backward compat.
  AP_PROF_BEGIN(Percept);
  melproc::melproc_process64(g_staging.raw_spectral, g_staging.smooth_spectral);
  AP_PROF_END(Percept);

  // 5) Chroma accumulation (you can choose raw or processed; we keep raw for stability)
  AP_PROF_BEGIN(Chroma);
  chroma_map::accumulate(g_staging.raw_spectral, g_staging.chroma);
  AP_PROF_END(Chroma);

  // 6) Band summaries (use processed bins for nicer dynamics)
  auto saturate_sum = [](int64_t value) -> int32_t {
    if (value > 0x7FFFFFFFLL) return 0x7FFFFFFF;
    if (value < (int64_t)-0x80000000LL) return (int32_t)-0x80000000LL;
    return (int32_t)value;
  };

  int64_t band_low_sum = 0;
  for (uint32_t k = band_low_start; k < band_low_end; ++k) band_low_sum += g_staging.smooth_spectral[k];

  int64_t band_low_mid_sum = 0;
  for (uint32_t k = band_low_mid_start; k < band_low_mid_end; ++k) band_low_mid_sum += g_staging.smooth_spectral[k];

  int64_t band_presence_sum = 0;
  for (uint32_t k = band_presence_start; k < band_presence_end; ++k) band_presence_sum += g_staging.smooth_spectral[k];

  int64_t band_high_sum = 0;
  for (uint32_t k = band_high_start; k < band_high_end; ++k) band_high_sum += g_staging.smooth_spectral[k];

  g_staging.band_low       = saturate_sum(band_low_sum);
  g_staging.band_low_mid   = saturate_sum(band_low_mid_sum);
  g_staging.band_presence  = saturate_sum(band_presence_sum);
  g_staging.band_high      = saturate_sum(band_high_sum);

  // 7) Spectral flux (linear) + clamp (still from raw; OK to swap to processed if desired)
  AP_PROF_BEGIN(Flux);
  g_staging.flux = flux::compute(g_staging.raw_spectral);
  static const q16 FLUX_Q16_CEIL = (q16)(1.25 * 65536.0);
  if (g_staging.flux > FLUX_Q16_CEIL) g_staging.flux = FLUX_Q16_CEIL;
  if (g_staging.flux < 0)             g_staging.flux = 0;
  AP_PROF_END(Flux);

#if AUDIO_DIAG_FLUX
  flux_diag_record(g_staging.flux);
#endif

  // 8) Tempo lane (BEAT-R1 already integrated)
  AP_PROF_BEGIN(Tempo);
  tempo_lane::tempo_ingest(centered_q24);
  tempo_lane::tempo_update(g_staging.tempo_bpm,
                           g_staging.beat_phase,
                           g_staging.beat_strength,
                           g_staging.beat_flag,
                           g_staging.tempo_confidence,
                           g_staging.tempo_silence);
  g_staging.tempo_ready = tempo_lane::tempo_ready() ? 1 : 0;
  AP_PROF_END(Tempo);

  // 9) Optional: GPIO pulse on beat for scope/LA
#if (BEAT_GPIO_PIN >= 0)
  if (g_staging.beat_flag) beat_gpio::beat_gpio_pulse();
#endif

  // 10) Publish atomically
  g_staging.t_ms = t_ms;
  uint32_t next_epoch = audio_frame_epoch + 1;
  g_staging.audio_frame_epoch = next_epoch;
  memcpy((void*)&g_public, (const void*)&g_staging, sizeof(AudioFrame));
  audio_frame_epoch = next_epoch;

  AP_PROF_END(Tick);
  AP_PROF_TICK();
}

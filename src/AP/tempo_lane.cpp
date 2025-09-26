#include "tempo_lane.h"

#include <math.h>
#include <string.h>

#include "debug/debug_flags.h"

#include <esp_dsp.h>

#if AUDIO_DIAG_TEMPO
#include <Arduino.h>
#endif

namespace tempo_lane {

static constexpr double Q16F = 65536.0;
static constexpr float  EPSILON = 1e-6f;
static constexpr int    kFFTSize = 512;
static constexpr int    kHopSize = chunk_size;           // expected 128
static constexpr float  kSampleRate = (float)audio_sample_rate;
static constexpr float  kFrameRate  = kSampleRate / (float)kHopSize;  // ~125 Hz
static constexpr int    kFFTComplex = kFFTSize * 2;
static constexpr int    kFFTBins    = kFFTSize / 2 + 1;

static constexpr float  kTempogramAlpha = 0.15f;         // tempogram EMA alpha
static constexpr int    kMedianWindowMax = 64;            // accommodates per-band windows
static constexpr uint16_t kBandMedianWindows[4] = {6u, 5u, 3u, 3u};
static constexpr float  kNoveltyBandWeights[4] = {0.28f, 0.32f, 0.24f, 0.16f};
static constexpr float  kPLL_Kappa = 0.18f;
static constexpr float  kPLL_MaxNudge = 0.10f;           // ±10% of period
static constexpr float  kBeatWindowFraction = 0.12f;
static constexpr float  kConfidenceAlpha = 0.20f;
static constexpr float  kConfidenceOn  = 0.60f;
static constexpr float  kConfidenceOff = 0.42f;
static constexpr float  kSilenceFloor = 1e-4f;
static constexpr float  kMaxPeriodSlewPerSec = 0.04f;
static constexpr float  kPhaseWeightLow = 0.60f;
static constexpr float  kPhaseWeightHM  = 0.40f;
static constexpr float  kSwitchUpMinDelta   = 0.04f;
static constexpr float  kSwitchDownMinDelta = 0.08f;
static constexpr float  kHMPhaseAdvForGearUp = 0.03f;
static constexpr float  kStickyBiasAfter1s   = 0.03f;

static constexpr int    kTempogramSeconds = 6;
static constexpr int    kTempogramFrames  = kTempogramSeconds * (int)kFrameRate; // ≈750
static constexpr int    kPhaseEvalFrames  = 2 * (int)kFrameRate;  // 2 s window
static constexpr int    kEnergyEvalFrames = 1 * (int)kFrameRate;  // 1 s window

static constexpr float  kMinBPM = 80.0f;
static constexpr float  kMaxBPM = 180.0f;
static constexpr int    kMinPeriodFrames = (int)roundf((60.0f * kFrameRate) / kMaxBPM); // ≈42
static constexpr int    kMaxPeriodFrames = (int)roundf((60.0f * kFrameRate) / kMinBPM); // ≈94
static constexpr int    kLagCount = kMaxPeriodFrames - kMinPeriodFrames + 1;
static_assert(kLagCount > 0, "invalid lag range");

struct BandState {
  float history[kMedianWindowMax];
  uint16_t head = 0;
  uint16_t count = 0;
  uint16_t window = 1;
};

struct TempoState {
  float fft_window[kFFTSize];
  float sample_ring[kFFTSize];
  uint32_t sample_head = 0;
  uint32_t samples_seen = 0;

  float fft_input[kFFTComplex];
  float prev_bin_mag[kFFTBins];

  BandState bands[4];
  float low_band_flux = 0.0f;

  float novelty_mix_hist[kTempogramFrames];
  float novelty_low_hist[kTempogramFrames];
  float novelty_hm_hist[kTempogramFrames];
  uint16_t novelty_head = 0;
  bool     novelty_full = false;

  float acf_values[kLagCount];
  float acf_hm_values[kLagCount];

  float confidence = 0.0f;
  bool  beat_enabled = false;
  float period_frames = 0.0f;
  float phase_frames  = 0.0f;
  uint8_t current_lane = 1;            // 0: half, 1: one, 2: one-half, 3: double
  uint32_t lane_hold_frames = 0;

  float silence_level = 1.0f;
  bool  silence = true;

  uint32_t frame_counter = 0;
#if AUDIO_DIAG_TEMPO
  uint32_t diag_counter = 0;
#endif
};

static TempoState g_state;

#if AUDIO_DIAG_TEMPO
static constexpr uint32_t kTempoDiagPeriod = (AUDIO_DIAG_TEMPO_PERIOD == 0u) ? 1u : AUDIO_DIAG_TEMPO_PERIOD;
#endif

struct BandBins {
  uint16_t start_bin;
  uint16_t end_bin;
};
static BandBins g_band_bins[4];

// ----------------------------------------------------------------------------
// Utilities
// ----------------------------------------------------------------------------

static inline q16 f_to_q16(float x) {
  if (x <= 0.0f) return 0;
  if (x >= 0.99998474f) return 65535;
  return (q16)(x * (float)Q16F + 0.5f);
}

static inline q16 f_to_q16_bpm(float bpm) {
  if (bpm < 0.0f) bpm = 0.0f;
  if (bpm > 400.0f) bpm = 400.0f;
  return (q16)(bpm * (float)Q16F + 0.5f);
}

static inline float period_from_bpm(float bpm) {
  if (bpm <= 0.0f) bpm = kMinBPM;
  return (60.0f * kFrameRate) / bpm;
}

static inline float bpm_from_period(float period_frames) {
  if (period_frames <= EPSILON) period_frames = (float)kMaxPeriodFrames;
  return (60.0f * kFrameRate) / period_frames;
}

static void build_window() {
  for (int n = 0; n < kFFTSize; ++n) {
    float ratio = (float)n / (float)(kFFTSize - 1);
    g_state.fft_window[n] = 0.5f * (1.0f - cosf(2.0f * (float)M_PI * ratio));
  }
}

static void compute_band_bins() {
  const float bin_hz = kSampleRate / (float)kFFTSize;
  const float edges_hz[5] = {40.0f, 160.0f, 640.0f, 2500.0f, 6000.0f};
  for (int b = 0; b < 4; ++b) {
    float f_start = edges_hz[b];
    float f_end   = edges_hz[b + 1];
    if (f_end > kSampleRate * 0.5f) f_end = kSampleRate * 0.5f;
    uint16_t start = (uint16_t)floorf(f_start / bin_hz + 0.5f);
    uint16_t end   = (uint16_t)floorf(f_end / bin_hz + 0.5f);
    if (start < 1) start = 1;            // skip DC
    if (end >= kFFTBins) end = kFFTBins - 1;
    if (end < start) end = start;
    g_band_bins[b].start_bin = start;
    g_band_bins[b].end_bin   = end;
  }
}

static void reset_state() {
  g_state = TempoState{};
  for (int b = 0; b < 4; ++b) {
    uint16_t window = kBandMedianWindows[b];
    if (window == 0u) window = 1u;
    if (window > kMedianWindowMax) window = kMedianWindowMax;
    g_state.bands[b].window = window;
    g_state.bands[b].head = 0;
    g_state.bands[b].count = 0;
  }
  build_window();
  compute_band_bins();
  dsps_fft2r_init_fc32(nullptr, kFFTSize);
}

static uint32_t available_frames() {
  if (g_state.novelty_full) return kTempogramFrames;
  return g_state.novelty_head;
}

static void push_novelty(float mix_value, float low_value, float hm_value) {
  g_state.novelty_mix_hist[g_state.novelty_head] = mix_value;
  g_state.novelty_low_hist[g_state.novelty_head] = low_value;
  g_state.novelty_hm_hist[g_state.novelty_head]  = hm_value;
  g_state.novelty_head = (uint16_t)((g_state.novelty_head + 1u) % kTempogramFrames);
  if (!g_state.novelty_full && g_state.novelty_head == 0u) {
    g_state.novelty_full = true;
  }
}

static float get_recent(const float* hist, uint32_t offset) {
  uint32_t avail = available_frames();
  if (offset >= avail) return 0.0f;
  int32_t idx = (int32_t)g_state.novelty_head - 1 - (int32_t)offset;
  if (idx < 0) idx += kTempogramFrames;
  return hist[idx];
}

static float median_band(const BandState& band) {
  uint16_t window = band.window;
  if (window == 0u) window = 1u;
  uint16_t count = band.count;
  if (count > window) count = window;
  if (count == 0) return 0.0f;
  float temp[kMedianWindowMax];
  uint16_t idx = band.head;
  for (uint16_t i = 0; i < count; ++i) {
    if (idx == 0) idx = window;
    temp[i] = band.history[idx - 1];
    idx--;
  }
  for (uint16_t i = 0; i < count; ++i) {
    for (uint16_t j = i + 1; j < count; ++j) {
      if (temp[j] < temp[i]) {
        float t = temp[i]; temp[i] = temp[j]; temp[j] = t;
      }
    }
  }
  if (count & 1u) {
    return temp[count / 2];
  }
  return 0.5f * (temp[count / 2 - 1] + temp[count / 2]);
}

// ----------------------------------------------------------------------------
// Tempogram
// ----------------------------------------------------------------------------

struct PeakInfo {
  int lag;
  float height;
};

static void compute_acf_for(const float* hist, float* acf_out) {
  uint32_t avail = available_frames();
  if (avail < (uint32_t)kMaxPeriodFrames + 2u) {
    memset(acf_out, 0, sizeof(float) * kLagCount);
    return;
  }

  float max_val = EPSILON;
  for (int lag = 0; lag < kLagCount; ++lag) {
    int period = kMinPeriodFrames + lag;
    uint32_t samples = avail - period;
    if ((int)samples <= 0) {
      acf_out[lag] = 0.0f;
      continue;
    }
    float sum = 0.0f;
    for (uint32_t n = 0; n < samples; ++n) {
      float a = get_recent(hist, n);
      float b = get_recent(hist, n + (uint32_t)period);
      sum += a * b;
    }
    float norm = sum / (float)samples;
    acf_out[lag] = (1.0f - kTempogramAlpha) * acf_out[lag] + kTempogramAlpha * norm;
    if (acf_out[lag] > max_val) max_val = acf_out[lag];
  }
  if (max_val > EPSILON) {
    float inv = 1.0f / max_val;
    for (int lag = 0; lag < kLagCount; ++lag) {
      acf_out[lag] *= inv;
    }
  }
}

static void update_tempogram() {
  compute_acf_for(g_state.novelty_mix_hist, g_state.acf_values);
  compute_acf_for(g_state.novelty_hm_hist,  g_state.acf_hm_values);
}

static void find_top_peaks(const float* acf, PeakInfo* peaks, int count) {
  for (int i = 0; i < count; ++i) {
    peaks[i].lag = -1;
    peaks[i].height = 0.0f;
  }
  for (int lag = 1; lag < kLagCount - 1; ++lag) {
    float h = acf[lag];
    if (h <= 0.0f) continue;
    if (h < acf[lag - 1] || h < acf[lag + 1]) continue;
    int insert = -1;
    float min_height = h;
    for (int i = 0; i < count; ++i) {
      if (peaks[i].lag == -1) { insert = i; break; }
      if (peaks[i].height < min_height) { min_height = peaks[i].height; insert = i; }
    }
    if (insert >= 0) {
      peaks[insert].lag = lag;
      peaks[insert].height = h;
    }
  }
  for (int i = 0; i < count; ++i) {
    for (int j = i + 1; j < count; ++j) {
      if (peaks[j].height > peaks[i].height) {
        PeakInfo tmp = peaks[i];
        peaks[i] = peaks[j];
        peaks[j] = tmp;
      }
    }
  }
}

static float acf_height_for_period(const float* acf, float period_frames) {
  int lag = (int)roundf(period_frames) - kMinPeriodFrames;
  if (lag < 0) lag = 0;
  if (lag >= kLagCount) lag = kLagCount - 1;
  return acf[lag];
}

static float compute_prominence(float primary_height, float secondary_height) {
  if (primary_height <= EPSILON) return 0.0f;
  float diff = primary_height - secondary_height;
  if (diff <= 0.0f) return 0.0f;
  return fminf(1.0f, diff / (primary_height + EPSILON));
}

static float compute_phase_score(const float* hist, float period_frames) {
  uint32_t avail = available_frames();
  if (avail == 0) return 0.0f;
  uint32_t window = (uint32_t)kPhaseEvalFrames;
  if (window > avail) window = avail;
  if (window == 0) return 0.0f;
  float threshold = kBeatWindowFraction * period_frames;
  float acc = 0.0f;
  int hits = 0;
  for (uint32_t i = 0; i < window; ++i) {
    float phase = fmodf(g_state.phase_frames + (float)i, period_frames);
    float distance = fminf(phase, period_frames - phase);
    if (distance <= threshold) {
      acc += get_recent(hist, i);
      ++hits;
    }
  }
  if (hits == 0) return 0.0f;
  return fminf(1.0f, acc / (float)hits);
}

static float limit_period_slew(float desired_period) {
  if (g_state.period_frames <= 0.0f) return desired_period;
  float max_delta_per_frame = g_state.period_frames * kMaxPeriodSlewPerSec * (1.0f / kFrameRate);
  float delta = desired_period - g_state.period_frames;
  if (delta > max_delta_per_frame) delta = max_delta_per_frame;
  if (delta < -max_delta_per_frame) delta = -max_delta_per_frame;
  return g_state.period_frames + delta;
}

static bool update_phase_pll() {
  if (g_state.period_frames <= EPSILON) return false;
  bool wrapped = false;
  g_state.phase_frames += (float)kHopSize;
  while (g_state.phase_frames >= g_state.period_frames) {
    g_state.phase_frames -= g_state.period_frames;
    wrapped = true;
  }
  float phase_error = g_state.phase_frames;
  if (phase_error > g_state.period_frames * 0.5f) {
    phase_error -= g_state.period_frames;
  }
  if (fabsf(phase_error) <= kBeatWindowFraction * g_state.period_frames) {
    float correction = kPLL_Kappa * phase_error;
    float limit = kPLL_MaxNudge * g_state.period_frames;
    if (correction > limit) correction = limit;
    if (correction < -limit) correction = -limit;
    g_state.phase_frames -= correction;
    if (g_state.phase_frames < 0.0f) g_state.phase_frames += g_state.period_frames;
  }
  return wrapped;
}

static void update_silence_gate() {
  uint32_t eval_frames = (uint32_t)kEnergyEvalFrames;
  uint32_t avail = available_frames();
  if (avail < eval_frames) {
    g_state.silence = true;
    g_state.silence_level = 1.0f;
    return;
  }
  float sum = 0.0f;
  for (uint32_t i = 0; i < eval_frames; ++i) {
    sum += get_recent(g_state.novelty_mix_hist, i);
  }
  float mean = sum / (float)eval_frames;
  g_state.silence_level = fminf(1.0f, fmaxf(0.0f, 1.0f - mean * 5.0f));
  g_state.silence = (mean < kSilenceFloor);
}

// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

bool tempo_init() {
  reset_state();
  return true;
}

void tempo_ingest(const int32_t* q24_chunk) {
  for (int i = 0; i < kHopSize; ++i) {
    float sample = (float)q24_chunk[i] / 8388607.0f;
    g_state.sample_ring[g_state.sample_head] = sample;
    g_state.sample_head = (g_state.sample_head + 1u) % kFFTSize;
    ++g_state.samples_seen;
  }

  if (g_state.samples_seen < (uint32_t)kFFTSize) {
    return;
  }

  uint32_t start = (kFFTSize + g_state.sample_head - kFFTSize) % kFFTSize;
  uint32_t idx = start;
  for (int n = 0; n < kFFTSize; ++n) {
    float windowed = g_state.sample_ring[idx] * g_state.fft_window[n];
    g_state.fft_input[2 * n]     = windowed;
    g_state.fft_input[2 * n + 1] = 0.0f;
    idx = (idx + 1u) % kFFTSize;
  }

  dsps_fft2r_fc32(g_state.fft_input, kFFTSize);
  dsps_bit_rev2r_fc32(g_state.fft_input, kFFTSize);
  dsps_cplx2reC_fc32(g_state.fft_input, kFFTSize);

  float band_flux_raw[4] = {0};
  for (int bin = 1; bin < kFFTBins; ++bin) {
    float real = g_state.fft_input[2 * bin];
    float imag = g_state.fft_input[2 * bin + 1];
    float mag = sqrtf(real * real + imag * imag);
    float diff = mag - g_state.prev_bin_mag[bin];
    g_state.prev_bin_mag[bin] = mag;
    if (diff <= 0.0f) continue;
    for (int band = 0; band < 4; ++band) {
      if (bin >= g_band_bins[band].start_bin && bin <= g_band_bins[band].end_bin) {
        band_flux_raw[band] += diff;
        break;
      }
    }
  }

  float band_values[4];
  for (int b = 0; b < 4; ++b) {
    BandState& bs = g_state.bands[b];
    float flux = band_flux_raw[b];
    if (flux < 0.0f) flux = 0.0f;
    if (bs.count < bs.window) ++bs.count;
    bs.history[bs.head] = flux;
    bs.head = (uint16_t)((bs.head + 1u) % bs.window);
    float median = median_band(bs);
    float novelty = flux - median;
    if (novelty < 0.0f) novelty = 0.0f;
    band_values[b] = novelty;
  }

  g_state.low_band_flux = band_values[0];
  float novelty_mix = 0.0f;
  for (int b = 0; b < 4; ++b) {
    novelty_mix += band_values[b] * kNoveltyBandWeights[b];
  }
  push_novelty(novelty_mix, g_state.low_band_flux, band_values[2]);
  update_tempogram();
  update_silence_gate();
  ++g_state.frame_counter;
}

struct LaneCandidate {
  const char* name;
  float period;
  float bpm;
  float prominence;
  float phase_low;
  float phase_hm;
  float phase_blend;
  float score;
};

void tempo_update(q16& out_tempo_bpm_q16,
                  q16& out_beat_phase_q16,
                  q16& out_beat_strength_q16,
                  uint8_t& out_beat_flag,
                  q16& out_tempo_confidence_q16,
                  q16& out_silence_q16) {
  out_beat_flag = 0;
  out_tempo_bpm_q16 = f_to_q16_bpm(0.0f);
  out_beat_phase_q16 = 0;
  out_beat_strength_q16 = 0;
  out_tempo_confidence_q16 = f_to_q16(g_state.confidence);
  out_silence_q16 = f_to_q16(g_state.silence_level);

  if (!g_state.novelty_full) {
    return;
  }

  update_tempogram();

  PeakInfo peaks_mix[4];
  find_top_peaks(g_state.acf_values, peaks_mix, 4);
  if (peaks_mix[0].lag < 0) {
    return;
  }
  PeakInfo peaks_hm[4];
  find_top_peaks(g_state.acf_hm_values, peaks_hm, 4);

  float base_period = g_state.period_frames > 0.0f
                          ? g_state.period_frames
                          : (float)(kMinPeriodFrames + peaks_mix[0].lag);
  if (base_period < (float)kMinPeriodFrames) base_period = (float)kMinPeriodFrames;
  if (base_period > (float)kMaxPeriodFrames) base_period = (float)kMaxPeriodFrames;

  if (peaks_hm[0].lag >= 0) {
    float mix_height = peaks_mix[0].height;
    float hm_height  = peaks_hm[0].height;
    if (hm_height >= 0.90f * mix_height) {
      float hm_period = (float)(kMinPeriodFrames + peaks_hm[0].lag);
      if (hm_period < (float)kMinPeriodFrames) hm_period = (float)kMinPeriodFrames;
      if (hm_period > (float)kMaxPeriodFrames) hm_period = (float)kMaxPeriodFrames;
      base_period = hm_period;
    }
  }

  static constexpr int kLaneCount = 4;
  static constexpr float kPeriodMultipliers[kLaneCount] = {2.0f, 1.0f, (2.0f / 3.0f), 0.5f};
  static constexpr const char* kLaneNames[kLaneCount] = {"0.5x", "1x", "1.5x", "2x"};

  LaneCandidate lanes[kLaneCount];
  float secondary_height = (peaks_mix[1].lag >= 0) ? peaks_mix[1].height : 0.0f;
  for (int i = 0; i < kLaneCount; ++i) {
    float candidate_period = base_period * kPeriodMultipliers[i];
    if (candidate_period < (float)kMinPeriodFrames) candidate_period = (float)kMinPeriodFrames;
    if (candidate_period > (float)kMaxPeriodFrames) candidate_period = (float)kMaxPeriodFrames;
    float height_mix = acf_height_for_period(g_state.acf_values, candidate_period);
    float prominence = compute_prominence(height_mix, secondary_height);
    float phase_low = compute_phase_score(g_state.novelty_low_hist, candidate_period);
    float phase_hm  = compute_phase_score(g_state.novelty_hm_hist, candidate_period);
    float phase_blend = kPhaseWeightLow * phase_low + kPhaseWeightHM * phase_hm;
    float score = 0.6f * prominence + 0.4f * phase_blend;
    lanes[i] = LaneCandidate{
        kLaneNames[i],
        candidate_period,
        bpm_from_period(candidate_period),
        prominence,
        phase_low,
        phase_hm,
        phase_blend,
        score};
  }

  uint8_t prev_lane = g_state.current_lane;
  if (prev_lane >= kLaneCount) prev_lane = 1;
  float lane_hold_seconds = (float)g_state.lane_hold_frames / kFrameRate;
  float current_score = lanes[prev_lane].score;
  float biased_current = current_score;
  if (lane_hold_seconds > 1.0f) {
    biased_current += kStickyBiasAfter1s;
  }
  float current_bpm = lanes[prev_lane].bpm;
  float current_phase_hm = lanes[prev_lane].phase_hm;

  uint8_t chosen_lane = prev_lane;
  float chosen_score = lanes[prev_lane].score;

  for (int i = 0; i < kLaneCount; ++i) {
    if (i == prev_lane) continue;
    const LaneCandidate& cand = lanes[i];
    bool faster = cand.bpm > current_bpm + 0.5f;
    bool slower = cand.bpm + 0.5f < current_bpm;
    if (faster) {
      if ((cand.score - biased_current) >= kSwitchUpMinDelta &&
          (cand.phase_hm - current_phase_hm) >= kHMPhaseAdvForGearUp &&
          cand.score > chosen_score) {
        chosen_lane = (uint8_t)i;
        chosen_score = cand.score;
      }
    } else if (slower) {
      if ((cand.score - biased_current) >= kSwitchDownMinDelta &&
          cand.score > chosen_score) {
        chosen_lane = (uint8_t)i;
        chosen_score = cand.score;
      }
    } else {
      if (cand.score > chosen_score) {
        chosen_lane = (uint8_t)i;
        chosen_score = cand.score;
      }
    }
  }

  if (chosen_lane == prev_lane) {
    g_state.lane_hold_frames += 1u;
  } else {
    g_state.lane_hold_frames = 0u;
  }
  g_state.current_lane = chosen_lane;

  float desired_period = lanes[chosen_lane].period;
  float limited_period = limit_period_slew(desired_period);
  g_state.period_frames = limited_period;
  lanes[chosen_lane].period = limited_period;
  lanes[chosen_lane].bpm = bpm_from_period(limited_period);

  bool wrapped = update_phase_pll();

  float bpm = lanes[chosen_lane].bpm;
  float strength = fminf(1.0f, fmaxf(0.0f, lanes[chosen_lane].score));

  g_state.confidence += kConfidenceAlpha * (strength - g_state.confidence);
  if (g_state.confidence >= kConfidenceOn) {
    g_state.beat_enabled = true;
  } else if (g_state.confidence <= kConfidenceOff) {
    g_state.beat_enabled = false;
  }

  float phase_norm = 0.0f;
  if (g_state.period_frames > EPSILON) {
    phase_norm = fmodf(g_state.phase_frames / g_state.period_frames, 1.0f);
    if (phase_norm < 0.0f) phase_norm += 1.0f;
  }

  out_tempo_bpm_q16 = f_to_q16_bpm(bpm);
  out_beat_phase_q16 = f_to_q16(phase_norm);
  out_beat_strength_q16 = f_to_q16(strength);
  out_tempo_confidence_q16 = f_to_q16(g_state.confidence);
  out_silence_q16 = f_to_q16(g_state.silence_level);

  if (!g_state.silence && g_state.beat_enabled && wrapped) {
    if (g_state.low_band_flux > kSilenceFloor) {
      out_beat_flag = 1u;
    }
  }

#if AUDIO_DIAG_TEMPO
  if (debug_flags::enabled(debug_flags::kGroupTempoFlux)) {
    Serial.printf("[tempo] bpm=%.1f strength=%.2f conf=%.2f silence=%.2f ready=%d beat=%u\n",
                  bpm,
                  strength,
                  g_state.confidence,
                  g_state.silence_level,
                  g_state.novelty_full ? 1 : 0,
                  (unsigned)out_beat_flag);

    if ((g_state.diag_counter++ % kTempoDiagPeriod) == 0u) {
      Serial.printf("[tempo] bpm=%.1f strength=%.2f conf=%.2f silence=%.2f ready=%d beat=%u\n",
                    bpm,
                    strength,
                    g_state.confidence,
                    g_state.silence_level,
                    g_state.novelty_full ? 1 : 0,
                    (unsigned)out_beat_flag);
      Serial.printf("[acf] mix:{");
      for (int i = 0; i < 4; ++i) {
        if (peaks_mix[i].lag < 0) continue;
        float peak_period = (float)(kMinPeriodFrames + peaks_mix[i].lag);
        float peak_bpm = bpm_from_period(peak_period);
        Serial.printf("lag=%d bpm=%.1f height=%.3f%s",
                      kMinPeriodFrames + peaks_mix[i].lag,
                      peak_bpm,
                      peaks_mix[i].height,
                      (i == 3) ? "" : " ");
      }
      Serial.printf("} hm:{");
      for (int i = 0; i < 2; ++i) {
        if (peaks_hm[i].lag < 0) continue;
        float peak_period = (float)(kMinPeriodFrames + peaks_hm[i].lag);
        float peak_bpm = bpm_from_period(peak_period);
        Serial.printf("lag=%d bpm=%.1f height=%.3f%s",
                      kMinPeriodFrames + peaks_hm[i].lag,
                      peak_bpm,
                      peaks_hm[i].height,
                      (i == 1) ? "" : " ");
      }
      Serial.println("}");
    }

    Serial.printf("[cand] ");
    for (int i = 0; i < kLaneCount; ++i) {
      const LaneCandidate& cand = lanes[i];
      Serial.printf("%s s=%.3f pL=%.2f pHM=%.2f%s",
                    cand.name,
                    cand.score,
                    cand.phase_low,
                    cand.phase_hm,
                    (i == kLaneCount - 1) ? "" : " | ");
    }
    Serial.printf(" | pick=%s\n", lanes[chosen_lane].name);
    Serial.printf("[tempo] bpm=%.1f strength=%.2f conf=%.2f silence=%.2f ready=%d beat=%u\n",
                  bpm,
                  strength,
                  g_state.confidence,
                  g_state.silence_level,
                  g_state.novelty_full ? 1 : 0,
                  (unsigned)out_beat_flag);

    Serial.printf("[acf] mix:{");
    for (int i = 0; i < 4; ++i) {
      if (peaks_mix[i].lag < 0) continue;
      float peak_period = (float)(kMinPeriodFrames + peaks_mix[i].lag);
      float peak_bpm = bpm_from_period(peak_period);
      Serial.printf("lag=%d bpm=%.1f height=%.3f%s",
                    kMinPeriodFrames + peaks_mix[i].lag,
                    peak_bpm,
                    peaks_mix[i].height,
                    (i == 3) ? "" : " ");
    }
    Serial.printf("} hm:{");
    for (int i = 0; i < 2; ++i) {
      if (peaks_hm[i].lag < 0) continue;
      float peak_period = (float)(kMinPeriodFrames + peaks_hm[i].lag);
      float peak_bpm = bpm_from_period(peak_period);
      Serial.printf("lag=%d bpm=%.1f height=%.3f%s",
                    kMinPeriodFrames + peaks_hm[i].lag,
                    peak_bpm,
                    peaks_hm[i].height,
                    (i == 1) ? "" : " ");
    }
    Serial.println("}");


    uint32_t window = (uint32_t)kEnergyEvalFrames;
    uint32_t avail = available_frames();
    if (window > avail) window = avail;
    if (window > 0) {
      float sum = 0.0f;
      float peak_flux = 0.0f;
      int buckets[6] = {0};
      for (uint32_t i = 0; i < window; ++i) {
        float v = get_recent(g_state.novelty_low_hist, i);
        sum += v;
        if (v > peak_flux) peak_flux = v;
        int bucket = (int)(v / 0.25f);
        if (bucket < 0) bucket = 0;
        if (bucket > 5) bucket = 5;
        buckets[bucket] += 1;
      }
      Serial.printf("[flux] avg=%.3f peak=%.3f samples=%u buckets:[0-0.25]=%d [0.25-0.5]=%d [0.5-0.75]=%d [0.75-1.0]=%d [1.0-1.25]=%d [>1.25]=%d\n",
                    sum / (float)window, peak_flux, window,
                    buckets[0], buckets[1], buckets[2], buckets[3], buckets[4], buckets[5]);
    }
  }
#endif
}

bool tempo_ready() { return g_state.novelty_full; }

bool tempo_is_silent() { return g_state.silence; }

q16 tempo_confidence_q16() { return f_to_q16(g_state.confidence); }

q16 tempo_silence_q16() { return f_to_q16(g_state.silence_level); }

} // namespace tempo_lane

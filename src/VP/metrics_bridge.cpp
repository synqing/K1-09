#include "metrics_bridge.h"

#include <algorithm>
#include <cmath>

#include "AP/audio_bus.h"

namespace vp {

namespace {
constexpr float kSpectrumAlpha = 0.25f;
constexpr float kFluxAlpha = 0.20f;

inline float clamp01(float v) {
  return std::clamp(v, 0.0f, 1.0f);
}

inline float q16_to_float_safe(q16 value, float upper = 1.5f) {
  float f = audio_frame_utils::q16_to_float(value);
  if (f < 0.0f) f = 0.0f;
  if (f > upper) f = upper;
  return f;
}

}  // namespace

MetricsBridge::MetricsBridge() {
  smoothed_spectrum_.fill(0.0f);
}

AudioMetrics MetricsBridge::collect(const AudioFrame& frame) {
  AudioMetrics metrics;

  // Waveform statistics (normalize Q15 samples into [-1, 1])
  float max_sample = -1.0f;
  float min_sample = 1.0f;
  float sum_sq = 0.0f;
  for (size_t i = 0; i < chunk_size; ++i) {
    float sample = static_cast<float>(frame.waveform[i]) / 32768.0f;
    if (sample > max_sample) max_sample = sample;
    if (sample < min_sample) min_sample = sample;
    sum_sq += sample * sample;
  }
  if (chunk_size > 0) {
    metrics.waveform_peak = std::clamp(max_sample, -1.0f, 1.0f);
    metrics.waveform_trough = std::clamp(min_sample, -1.0f, 1.0f);
    float rms = std::sqrt(sum_sq / static_cast<float>(chunk_size));
    metrics.waveform_rms = clamp01(rms);
  }

  // Spectrum smoothing
  for (size_t i = 0; i < smoothed_spectrum_.size(); ++i) {
    float raw = q16_to_float_safe(frame.raw_spectral[i]);
    float smooth = smoothed_spectrum_[i] * (1.0f - kSpectrumAlpha) + raw * kSpectrumAlpha;
    smoothed_spectrum_[i] = smooth;
    metrics.spectrum[i] = clamp01(smooth);
  }

  // Chroma (already aggregated in producer)
  for (size_t i = 0; i < metrics.chroma.size(); ++i) {
    metrics.chroma[i] = clamp01(q16_to_float_safe(frame.chroma[i]));
  }

  metrics.band_low       = clamp01(q16_to_float_safe(frame.band_low));
  metrics.band_low_mid   = clamp01(q16_to_float_safe(frame.band_low_mid));
  metrics.band_presence  = clamp01(q16_to_float_safe(frame.band_presence));
  metrics.band_high      = clamp01(q16_to_float_safe(frame.band_high));

  float flux_lin = q16_to_float_safe(frame.flux, 1.25f);
  flux_state_ = flux_state_ * (1.0f - kFluxAlpha) + flux_lin * kFluxAlpha;
  metrics.flux = clamp01(flux_lin / 1.25f);
  metrics.flux_smoothed = clamp01(flux_state_ / 1.25f);

  metrics.tempo_bpm = audio_frame_utils::q16_to_bpm(frame.tempo_bpm);
  metrics.beat_phase = clamp01(audio_frame_utils::q16_to_float(frame.beat_phase));
  metrics.beat_strength = clamp01(audio_frame_utils::q16_to_float(frame.beat_strength));
  metrics.tempo_confidence = clamp01(audio_frame_utils::q16_to_float(frame.tempo_confidence));
  metrics.tempo_silence = clamp01(audio_frame_utils::q16_to_float(frame.tempo_silence));
  metrics.beat_flag = frame.beat_flag != 0;
  metrics.tempo_ready = frame.tempo_ready != 0;

  metrics.vu_peak = clamp01(audio_frame_utils::q16_to_float(frame.vu_peak));
  metrics.vu_rms  = clamp01(audio_frame_utils::q16_to_float(frame.vu_rms));

  return metrics;
}

}  // namespace vp

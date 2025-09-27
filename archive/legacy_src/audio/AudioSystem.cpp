#include "audio/AudioSystem.h"

#include <algorithm>
#include <cmath>

#include "debug/audio_diagnostics.h"

namespace SensoryBridge {
namespace Audio {

namespace {

inline float clamp01(float value) {
  if (value < 0.0f) {
    return 0.0f;
  }
  if (value > 1.0f) {
    return 1.0f;
  }
  return value;
}

}  // namespace

void AudioCapture::initialize() {
  // TODO: Initialize I2S or other capture hardware from scratch.
}

void AudioCapture::capture(uint32_t /*timestamp_ms*/, std::array<int16_t, kSamplesPerFrame>& buffer) {
  // TODO: Fill buffer with samples from hardware.
  buffer.fill(0);
}

void AudioProcessor::initialize() {
  // TODO: Set up processing state (windows, filters, etc.).
}

void AudioProcessor::process(uint32_t /*timestamp_ms*/,
                             const std::array<int16_t, kSamplesPerFrame>& buffer,
                             Metrics& metrics) {
  // TODO: Replace stub with real processing pipeline.
  float max_abs = 0.0f;
  float sum = 0.0f;
  for (int16_t sample : buffer) {
    float s = static_cast<float>(sample) / 32768.0f;
    max_abs = std::max(max_abs, std::fabs(s));
    sum += s * s;
  }
  metrics.waveformPeak = max_abs;
  metrics.vuLevel = std::sqrt(sum / static_cast<float>(buffer.size()));
  metrics.silentScale = 1.0f;
  metrics.silence = metrics.vuLevel < 0.001f;
  metrics.brightness = clamp01(metrics.brightness);
  metrics.saturation = clamp01(metrics.saturation);
  metrics.spectrogram.fill(0.0f);
  metrics.chroma.fill(0.0f);
  if (!metrics.silence) {
    metrics.spectrogram[0] = metrics.vuLevel;
  }
}

AudioSystem& AudioSystem::instance() {
  static AudioSystem system;
  return system;
}

void AudioSystem::initialize() {
  capture_.initialize();
  processor_.initialize();
  metrics_ = Metrics{};
}

void AudioSystem::capture(uint32_t timestamp_ms) {
  last_capture_timestamp_ms_ = timestamp_ms;
  capture_.capture(timestamp_ms, capture_buffer_);
}

void AudioSystem::process(uint32_t timestamp_ms) {
  processor_.process(timestamp_ms, capture_buffer_, metrics_);
  emitDiagnostics(timestamp_ms);
}

void AudioSystem::update(uint32_t timestamp_ms) {
  capture(timestamp_ms);
  process(timestamp_ms);
}

void AudioSystem::emitDiagnostics(uint32_t timestamp_ms) {
  AudioDiagnostics::ChunkStats stats;
  stats.timestamp_ms = last_capture_timestamp_ms_;
  stats.max_raw = metrics_.waveformPeak * 32768.0f;
  stats.max_processed = metrics_.waveformPeak * 32768.0f;
  stats.rms = metrics_.vuLevel;
  stats.vu_level = metrics_.vuLevel;
  stats.silent_scale = metrics_.silentScale;
  stats.silence = metrics_.silence;
  stats.sweet_spot_state = 0;
  stats.agc_floor = 0.0f;
  stats.silence_threshold = 0.0f;
  stats.loud_threshold = 0.0f;

  AudioDiagnostics::onChunkCaptured(stats, capture_buffer_.data(), capture_buffer_.size());
  AudioDiagnostics::onSpectralUpdate(timestamp_ms);
}

}  // namespace Audio
}  // namespace SensoryBridge

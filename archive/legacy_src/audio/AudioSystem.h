#pragma once

#include <array>
#include <cstdint>

#include "constants.h"

namespace SensoryBridge {
namespace Audio {

constexpr size_t kSamplesPerFrame = 256;  // placeholder, will be configurable later

struct Metrics {
  std::array<float, NUM_FREQS> spectrogram{};
  std::array<float, 12> chroma{};
  float vuLevel = 0.0f;
  float silentScale = 1.0f;
  float waveformPeak = 0.0f;
  float brightness = 0.0f;
  float saturation = 0.0f;
  bool silence = false;
};

class AudioCapture {
 public:
  void initialize();
  void capture(uint32_t timestamp_ms, std::array<int16_t, kSamplesPerFrame>& buffer);
};

class AudioProcessor {
 public:
  void initialize();
  void process(uint32_t timestamp_ms,
               const std::array<int16_t, kSamplesPerFrame>& buffer,
               Metrics& metrics);
};

class AudioSystem {
 public:
  static AudioSystem& instance();

  void initialize();
  void capture(uint32_t timestamp_ms);
  void process(uint32_t timestamp_ms);
  void update(uint32_t timestamp_ms);

  const Metrics& metrics() const { return metrics_; }
  Metrics& metrics() { return metrics_; }

 private:
  AudioSystem() = default;

  void emitDiagnostics(uint32_t timestamp_ms);

  AudioCapture capture_{};
  AudioProcessor processor_{};
  Metrics metrics_{};
  std::array<int16_t, kSamplesPerFrame> capture_buffer_{};
  uint32_t last_capture_timestamp_ms_ = 0;
};

}  // namespace Audio
}  // namespace SensoryBridge


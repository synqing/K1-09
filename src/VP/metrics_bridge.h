#pragma once

#include "audio_metrics.h"

namespace vp {

// Collects and smooths audio metrics from AudioFrame snapshots.
class MetricsBridge {
 public:
  MetricsBridge();

  // Convert an AudioFrame into normalised metrics (thread-safe, single call per frame).
  AudioMetrics collect(const AudioFrame& frame);

 private:
  std::array<float, freq_bins> smoothed_spectrum_{};
  float flux_state_ = 0.0f;
};

}  // namespace vp


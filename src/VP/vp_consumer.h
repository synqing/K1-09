#pragma once

#include <stdint.h>
#include "AP/audio_bus.h"

namespace vp_consumer {

struct VPFrame {
  AudioFrame af;
  uint32_t epoch;
  uint32_t t_ms;
};

// Acquire a stable snapshot of the latest AudioFrame.
bool acquire(VPFrame& out);

} // namespace vp_consumer

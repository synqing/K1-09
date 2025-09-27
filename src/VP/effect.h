#pragma once

#include "FastLED.h"

#include "audio_metrics.h"
#include "frame_context.h"
#include "led_driver.h"
#include "tunables.h"

namespace vp {

class Effect {
 public:
  virtual ~Effect() = default;
  virtual const char* name() const = 0;
  virtual void render(const AudioMetrics& metrics,
                      const FrameContext& context,
                      LedFrame& frame,
                      const Tunables& tunables) = 0;
};

}  // namespace vp


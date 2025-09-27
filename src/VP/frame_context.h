#pragma once

#include <stdint.h>

#include "FastLED.h"

namespace vp {

struct FrameContext {
  uint32_t epoch = 0;
  uint32_t timestamp_ms = 0;
  float time_seconds = 0.0f;

  uint16_t strip_length = 160;
  uint16_t center_left = 79;   // inclusive index on each strip
  uint16_t center_right = 80;  // inclusive index (mirrored partner)

  float brightness_scalar = 0.55f;  // 0..1
  float saturation = 1.0f;

  const CRGBPalette16* palette = nullptr;
  float palette_blend = 0.0f;  // 0..1 blend progress
};

}  // namespace vp

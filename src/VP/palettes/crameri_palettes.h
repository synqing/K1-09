#pragma once

#include <FastLED.h>

namespace vp::palette_data {

enum PaletteFlag : uint8_t {
  PAL_WARM        = 1u << 0,
  PAL_COOL        = 1u << 1,
  PAL_HIGH_SAT    = 1u << 2,
  PAL_WHITE_HEAVY = 1u << 3,
  PAL_CALM        = 1u << 4,
  PAL_VIVID       = 1u << 5
};

extern const TProgmemRGBGradientPaletteRef kCrameriPalettes[];
extern const uint8_t kCrameriPaletteCount;
extern const char* const kCrameriPaletteNames[];
extern const uint8_t kCrameriPaletteFlags[];
extern const uint8_t kCrameriPaletteMaxBrightness[];

}  // namespace vp::palette_data


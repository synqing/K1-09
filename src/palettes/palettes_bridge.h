// palettes_bridge.h — unified access to the active LED‑calibrated palette LUTs
#pragma once
#include <FastLED.h>
#include "../globals.h"
#include "palette_luts_api.h"

// Forward declaration of hsv function to avoid circular dependency
CRGB16 hsv(SQ15x16 h, SQ15x16 s, SQ15x16 v);

// Forward declaration of desaturate function for saturation support
CRGB16 desaturate(CRGB16 input_color, SQ15x16 amount);

// Map 0..1 to 0..255
static inline uint8_t byte01(SQ15x16 v) {
  float f = float(v);
  if (f < 0.f) f = 0.f;
  if (f > 1.f) f = 1.f;
  return uint8_t(f * 255.0f + 0.5f);
}

// Sample either HSV or the current LED‑calibrated palette LUT
static inline CRGB16 hsv_or_palette(SQ15x16 hue01, SQ15x16 sat01, SQ15x16 val01) {
  const uint8_t sel = CONFIG.PALETTE_INDEX;  // 0 = HSV, 1+ = palette
  if (sel == 0) return hsv(hue01, sat01, val01);

  // Palette mode: use 256-entry calibrated LUTs built in palette_luts.cpp
  const CRGB16* lut = frame_config.palette_ptr;
  const uint16_t lut_size = frame_config.palette_size;

  // Fallback to HSV if LUT missing
  if (lut == nullptr || lut_size == 0) {
    return hsv(hue01, sat01, val01);
  }

  // When AUTO_COLOR_SHIFT is enabled, keep palette sampling static to avoid color drift
  // Use mid-point index which is safe for most palettes
  uint8_t palPos = CONFIG.AUTO_COLOR_SHIFT ? 128 : byte01(hue01);
  if (palPos >= lut_size) palPos = lut_size - 1;

  CRGB16 result = lut[palPos];

  // Apply value/brightness scaling once (matching HSV mode)
  result.r *= val01;
  result.g *= val01;
  result.b *= val01;

  // Preserve palette chroma: do NOT apply desaturation in palette mode
  // (HSV desaturation is handled inside hsv())
  return result;
}

// Helpers for UI/debug naming
static inline const char* get_current_palette_name() {
  return palette_name_for_index(CONFIG.PALETTE_INDEX);
}

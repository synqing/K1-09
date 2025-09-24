// palette_facade.h — Phase 2 palette helpers (Agent A scaffolding)
#pragma once

#include "palettes_bridge.h"  // hsv_or_palette, palette LUT state via frame_config
#include "../globals.h"       // CONFIG for SATURATION

// NOTE: This header introduces a thin facade over hsv_or_palette() so modes can be
// migrated incrementally without changing palette infrastructure. Adoption is gated
// by the rollout process (Agent 2 handles actual mode migrations).

// Primary palette sample: hue in [0,1], value in [0,1]
static inline CRGB16 pal_primary(SQ15x16 hue01, SQ15x16 val01) {
  return hsv_or_palette(hue01, CONFIG.SATURATION, val01);
}

// Contrast color: 180° hue shift (wraps in [0,1])
static inline CRGB16 pal_contrast(SQ15x16 hue01, SQ15x16 val01) {
  SQ15x16 h = hue01 + SQ15x16(0.5);
  while (h >= SQ15x16(1.0)) h -= SQ15x16(1.0);
  return hsv_or_palette(h, CONFIG.SATURATION, val01);
}

// Accent color: arbitrary hue shift (wraps in [0,1])
static inline CRGB16 pal_accent(SQ15x16 hue01, SQ15x16 shift, SQ15x16 val01) {
  SQ15x16 h = hue01 + shift;
  while (h >= SQ15x16(1.0)) h -= SQ15x16(1.0);
  while (h <  SQ15x16(0.0)) h += SQ15x16(1.0);
  return hsv_or_palette(h, CONFIG.SATURATION, val01);
}


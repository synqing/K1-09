#pragma once

#include <vector>

#include "FastLED.h"

namespace vp {

class PaletteManager {
 public:
  PaletteManager();

  void init();

  void set_index(uint8_t index, bool snap = false);
  uint8_t index() const { return current_index_; }

  // Advance blending, returns current palette reference.
  const CRGBPalette16& update(float blend_speed = 0.01f);

  const CRGBPalette16& current_palette() const { return current_; }

 private:
  std::vector<CRGBPalette16> palettes_;
  CRGBPalette16 current_{};
  CRGBPalette16 target_{};
  float blend_progress_ = 1.0f;
  uint8_t current_index_ = 0;
};

}  // namespace vp


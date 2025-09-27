#pragma once

#include <vector>

#include "FastLED.h"

#include "palette_catalog.h"

namespace vp {

class PaletteManager {
 public:
  PaletteManager();

  void init();

  void set_index(uint8_t index, bool snap = false);
  uint8_t index() const { return current_index_; }
  uint8_t count() const { return static_cast<uint8_t>(palettes_.size()); }
  void next(bool snap = false);
  void prev(bool snap = false);

  // Advance blending, returns current palette reference.
  const CRGBPalette16& update(float blend_speed = 0.01f);

  const CRGBPalette16& current_palette() const { return current_; }
  const char* current_name() const;
  uint8_t current_brightness_cap() const;

 private:
  std::vector<CRGBPalette16> palettes_;
  CRGBPalette16 current_{};
  CRGBPalette16 target_{};
  float blend_progress_ = 1.0f;
  uint8_t current_index_ = 0;
  std::vector<const char*> names_;
  std::vector<uint8_t> brightness_caps_;
};

}  // namespace vp

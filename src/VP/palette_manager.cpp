#include "palette_manager.h"

#include <algorithm>

namespace vp {

PaletteManager::PaletteManager() {
  init();
}

void PaletteManager::init() {
  palettes_.clear();
  palettes_.reserve(6);

  palettes_.push_back(CRGBPalette16(CRGB::Black));                    // solid off
  palettes_.push_back(CRGBPalette16(CloudColors_p));                  // cool blues
  palettes_.push_back(CRGBPalette16(OceanColors_p));                  // aquatic teal
  palettes_.push_back(CRGBPalette16(ForestColors_p));                 // greens
  palettes_.push_back(CRGBPalette16(LavaColors_p));                   // warm reds
  palettes_.push_back(CRGBPalette16(PartyColors_p));                  // accent neons

  // Default to the first active palette (skip the all-black entry used for explicit blackout).
  current_index_ = palettes_.size() > 1 ? 1u : 0u;
  current_ = palettes_[current_index_];
  target_ = current_;
  blend_progress_ = 1.0f;
}

void PaletteManager::set_index(uint8_t index, bool snap) {
  if (palettes_.empty()) {
    return;
  }
  if (index >= palettes_.size()) {
    index = index % palettes_.size();
  }
  current_index_ = index;
  target_ = palettes_[current_index_];
  if (snap) {
    current_ = target_;
    blend_progress_ = 1.0f;
  } else {
    blend_progress_ = 0.0f;
  }
}

const CRGBPalette16& PaletteManager::update(float blend_speed) {
  blend_speed = std::clamp(blend_speed, 0.001f, 1.0f);
  if (blend_progress_ < 1.0f) {
    // Blend towards target palette in-place.
    nblendPaletteTowardPalette(current_, target_, 8);
    blend_progress_ += blend_speed;
    if (blend_progress_ > 1.0f) {
      blend_progress_ = 1.0f;
      current_ = target_;
    }
  }
  return current_;
}

}  // namespace vp

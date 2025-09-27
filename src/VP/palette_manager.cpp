#include "palette_manager.h"

#include <algorithm>

namespace vp {

PaletteManager::PaletteManager() {
  init();
}

void PaletteManager::init() {
  palettes_.clear();
  names_.clear();
  brightness_caps_.clear();

  const std::size_t total = palette_catalog::count();
  palettes_.reserve(total);
  names_.reserve(total);
  brightness_caps_.reserve(total);

  for (std::size_t i = 0; i < total; ++i) {
    CRGBPalette16 palette;
    palette_catalog::load_palette(i, palette);
    palettes_.push_back(palette);

    auto meta = palette_catalog::metadata(i);
    names_.push_back(meta.name ? meta.name : "");
    brightness_caps_.push_back(meta.max_brightness ? meta.max_brightness : 255u);
  }

  current_index_ = palettes_.empty() ? 0u : 0u;
  current_ = palettes_.empty() ? CRGBPalette16(CRGB::Black) : palettes_[current_index_];
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

void PaletteManager::next(bool snap) {
  if (palettes_.empty()) {
    return;
  }
  uint8_t next_idx = static_cast<uint8_t>((current_index_ + 1u) % palettes_.size());
  set_index(next_idx, snap);
}

void PaletteManager::prev(bool snap) {
  if (palettes_.empty()) {
    return;
  }
  uint8_t prev_idx = static_cast<uint8_t>((current_index_ + palettes_.size() - 1u) % palettes_.size());
  set_index(prev_idx, snap);
}

const char* PaletteManager::current_name() const {
  if (names_.empty() || current_index_ >= names_.size()) {
    return "";
  }
  return names_[current_index_];
}

uint8_t PaletteManager::current_brightness_cap() const {
  if (brightness_caps_.empty() || current_index_ >= brightness_caps_.size()) {
    return 255u;
  }
  return brightness_caps_[current_index_];
}

}  // namespace vp

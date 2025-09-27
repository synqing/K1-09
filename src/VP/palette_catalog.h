#pragma once

#include <FastLED.h>
#include <cstddef>

namespace vp::palette_catalog {

struct Metadata {
  const char* name;
  uint8_t flags;
  uint8_t max_brightness;
};

std::size_t count();
void load_palette(std::size_t index, CRGBPalette16& out);
Metadata metadata(std::size_t index);

}  // namespace vp::palette_catalog


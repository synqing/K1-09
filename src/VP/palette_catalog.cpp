#include "palette_catalog.h"

#include <vector>

#include "palettes/crameri_palettes.h"
#include "palettes/standard_palettes.h"

namespace vp::palette_catalog {
namespace {

struct Entry {
  TProgmemRGBGradientPaletteRef prog;
  const char* name;
  uint8_t flags;
  uint8_t max_brightness;
};

std::vector<Entry> build_catalog() {
  std::vector<Entry> entries;
  entries.reserve(palette_data::kStandardPaletteCount + palette_data::kCrameriPaletteCount);

  for (uint8_t i = 0; i < palette_data::kStandardPaletteCount; ++i) {
    entries.push_back(Entry{
        palette_data::kStandardPalettes[i],
        palette_data::kStandardPaletteNames[i],
        /*flags=*/0u,
        /*max_brightness=*/255u});
  }

  for (uint8_t i = 0; i < palette_data::kCrameriPaletteCount; ++i) {
    const uint8_t brightness = palette_data::kCrameriPaletteMaxBrightness[i];
    entries.push_back(Entry{
        palette_data::kCrameriPalettes[i],
        palette_data::kCrameriPaletteNames[i],
        palette_data::kCrameriPaletteFlags[i],
        brightness ? brightness : 255u});
  }

  return entries;
}

const std::vector<Entry>& catalog() {
  static const std::vector<Entry> kCatalog = build_catalog();
  return kCatalog;
}

}  // namespace

std::size_t count() {
  return catalog().size();
}

void load_palette(std::size_t index, CRGBPalette16& out) {
  const auto& entry = catalog().at(index);
  out = CRGBPalette16(entry.prog);
}

Metadata metadata(std::size_t index) {
  const auto& entry = catalog().at(index);
  return Metadata{entry.name, entry.flags, entry.max_brightness};
}

}  // namespace vp::palette_catalog


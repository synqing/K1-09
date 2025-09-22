// palettes_bridge.h
// Purpose: palette access wrapper with validation & bounds checks.
// Does not modify hsv() or current modes; safe to include now.

#ifndef PALETTES_BRIDGE_H
#define PALETTES_BRIDGE_H

#include "constants.h"
#include "globals.h"
#include "safety_palettes.h"

namespace SensoryBridge {
namespace Palettes {

  // Set the active palette for the next frame (to be called from UI/control task).
  // For now, you can leave this unused; render path only reads frame_config.
  inline void selectPalette(const CRGB16* lut_ptr, uint16_t lut_size) {
    // This function does not write frame_config directly; caller should store
    // into a stable global and cache it during cache_frame_config().
    (void)lut_ptr; (void)lut_size;
  }

  // Safe accessor: returns CRGB16{0,0,0} on any validation failure.
  inline CRGB16 getPaletteColor(uint8_t index) {
    const CRGB16* lut = frame_config.palette_ptr;
    const uint16_t N  = frame_config.palette_size;

    // Validation
    if (!PaletteValidator::validatePROGMEM(lut)) {
      return CRGB16{0,0,0};
    }
    if (!PaletteValidator::validateAlignment(lut)) {
      USBSerial.printf("ERROR: palette_ptr misaligned\n");
      return CRGB16{0,0,0};
    }
    if (!PaletteValidator::validateBounds(index, N)) {
      USBSerial.printf("ERROR: palette index %u out of bounds (size=%u)\n", index, N);
      return CRGB16{0,0,0};
    }

    // Safe read
    CRGB16 c = lut[index];

    // Optional: validate channel ranges (0..1)
    if (!PaletteValidator::validateColorRange(c)) {
      USBSerial.printf("ERROR: palette color out of range at %u\n", index);
      return CRGB16{0,0,0};
    }
    return c;
  }

} // namespace Palettes
} // namespace SensoryBridge

#endif // PALETTES_BRIDGE_H
#ifndef PALETTE_LUTS_API_H
#define PALETTE_LUTS_API_H

#include <stdint.h>
#include <FastLED.h>

// Accessors for precomputed CRGB16 palette lookup tables.
// Index 0 refers to HSV mode and returns nullptr/0 size.

const CRGB16* palette_lut_for_index(uint8_t index);
uint16_t palette_lut_size(uint8_t index);
uint8_t palette_lut_count();
const char* palette_name_for_index(uint8_t index);

#endif // PALETTE_LUTS_API_H

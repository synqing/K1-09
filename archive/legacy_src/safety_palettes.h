// safety_palettes.h
// Purpose: centralised bounds checking + palette validation utilities.

#ifndef SAFETY_PALETTES_H
#define SAFETY_PALETTES_H

#include <Arduino.h>
#include "constants.h"
#include "globals.h"

// ===== LED bounds helpers (0..NATIVE_RESOLUTION-1) =====
inline bool led_index_valid(uint16_t index) {
  return index < NATIVE_RESOLUTION;
}

inline CRGB16 get_led16_or_black(const CRGB16* arr, uint16_t index, const char* ctx) {
  if (!led_index_valid(index)) {
    USBSerial.printf("ERROR: LED index %u out of bounds in %s (N=%u)\n",
                     index, ctx ? ctx : "?", (unsigned)NATIVE_RESOLUTION);
    return CRGB16{0,0,0};
  }
  return arr[index];
}

inline void set_led16_safe(CRGB16* arr, uint16_t index, const CRGB16& c, const char* ctx) {
  if (!led_index_valid(index)) {
    USBSerial.printf("ERROR: LED index %u out of bounds in %s (N=%u)\n",
                     index, ctx ? ctx : "?", (unsigned)NATIVE_RESOLUTION);
    return;
  }
  arr[index] = c;
}

// ===== Palette validation (data-only; no dynamic alloc) =====
class PaletteValidator {
public:
  // 16-bit alignment check (CRGB16 expects naturally aligned SQ15x16)
  static inline bool validateAlignment(const void* ptr) {
    return ((reinterpret_cast<uintptr_t>(ptr) & 0x1) == 0);
  }
  static inline bool validateBounds(uint16_t index, uint16_t size) {
    return index < size;
  }
  // On ESP32, PROGMEM lives in flash mapped to I-cache; we only verify non-null.
  static inline bool validatePROGMEM(const void* ptr) {
    return ptr != nullptr;
  }
  // Expecting channels normalised 0..1 in SQ15x16
  static inline bool validateColorRange(const CRGB16& c) {
    const SQ15x16 lo = 0.0, hi = 1.0;
    return (c.r >= lo && c.r <= hi) &&
           (c.g >= lo && c.g <= hi) &&
           (c.b >= lo && c.b <= hi);
  }
};

#endif // SAFETY_PALETTES_H
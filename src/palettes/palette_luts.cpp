// palette_luts.cpp - Converts FastLED gradient palettes to CRGB16 LUTs
// Uses YOUR actual curated palette collection, not rainbow garbage!

#include <Arduino.h>
#include <FastLED.h>
#include <math.h>
#include <string.h>
#include <algorithm>
#include "../constants.h"
#include "FastLED_Palettes.h"
#include "palette_luts_api.h"

// Define USBSerial for ESP32-S3
#if ARDUINO_USB_CDC_ON_BOOT
  #define USBSerial Serial
#else
  extern HWCDC USBSerial;
#endif

// Complete struct definition to avoid circular dependency with palettes_bridge.h
struct PaletteEntry {
  const CRGB16* lut_ptr;      // Pointer to 256-entry LUT in PROGMEM
  const char* name;            // Human-readable name
  uint8_t encoder_value;       // Which encoder value selects this
  bool requires_saturation;    // Whether to apply saturation control
};

// Convert a FastLED palette into a calibrated CRGB16 LUT suitable for WS2812 strips.
// Applies gamma correction (sRGB -> linear), normalises peak brightness, and clamps to
// avoid blowing out scientific palettes that have high luminance.
static void build_palette_lut(const CRGBPalette256& src_palette, CRGB16 dest[256]) {
  constexpr float gamma_in = 2.2f;      // sRGB gamma assumption for source palettes
  constexpr float target_peak = 0.85f;  // keep headroom so palettes don't wash to white

  float linear_r[256];
  float linear_g[256];
  float linear_b[256];
  float max_component = 0.0f;

  for (int i = 0; i < 256; ++i) {
    CRGB color = ColorFromPalette(src_palette, i, 255, NOBLEND);
    float r = powf(color.r / 255.0f, gamma_in);
    float g = powf(color.g / 255.0f, gamma_in);
    float b = powf(color.b / 255.0f, gamma_in);

    linear_r[i] = r;
    linear_g[i] = g;
    linear_b[i] = b;

    max_component = fmaxf(max_component, fmaxf(r, fmaxf(g, b)));
  }

  if (max_component <= 0.0f) {
    max_component = 1.0f;
  }

  float scale = target_peak / max_component;
  if (scale > 1.0f) {
    scale = 1.0f; // never brighten beyond the palette's original intent
  }

  for (int i = 0; i < 256; ++i) {
    float r = fminf(linear_r[i] * scale, 1.0f);
    float g = fminf(linear_g[i] * scale, 1.0f);
    float b = fminf(linear_b[i] * scale, 1.0f);

    // Clamp to [0,1] range before storing
    if (r < 0.0f) r = 0.0f;
    if (g < 0.0f) g = 0.0f;
    if (b < 0.0f) b = 0.0f;

    dest[i] = CRGB16{ SQ15x16(r), SQ15x16(g), SQ15x16(b) };
  }
}

constexpr uint8_t LED_CALIBRATED_COUNT = 33;

static CRGB16 PALETTE_LUTS[LED_CALIBRATED_COUNT][256];
static PaletteEntry g_palette_registry[LED_CALIBRATED_COUNT + 1];

// Initialize all palette LUTs from gradient definitions
void init_palette_luts() {
  static bool initialized = false;
  if (initialized) {
    return;
  }

  if (gGradientPaletteCount < LED_CALIBRATED_COUNT) {
    USBSerial.println("WARNING: Not all calibrated palettes are available; truncating list.");
  }

  g_palette_registry[0] = { nullptr, "HSV (Off)", 0, true };

  CRGBPalette256 temp_palette;
  const uint8_t palette_limit = std::min<uint8_t>(LED_CALIBRATED_COUNT, gGradientPaletteCount);

  for (uint8_t i = 0; i < palette_limit; ++i) {
    temp_palette = gGradientPalettes[i];
    build_palette_lut(temp_palette, PALETTE_LUTS[i]);
    g_palette_registry[i + 1] = {
      PALETTE_LUTS[i],
      paletteNames[i],
      static_cast<uint8_t>(i + 1),
      true
    };
  }

  // If fewer calibrated palettes than expected, ensure remaining entries are safe
  for (uint8_t i = palette_limit; i < LED_CALIBRATED_COUNT; ++i) {
    memset(PALETTE_LUTS[i], 0, sizeof(PALETTE_LUTS[i]));
    g_palette_registry[i + 1] = {
      PALETTE_LUTS[i],
      "(unused)",
      static_cast<uint8_t>(i + 1),
      true
    };
  }

  initialized = true;
}

static void ensure_palettes_initialised() {
  if (g_palette_registry[1].lut_ptr == nullptr) { // palette zero is always nullptr
    init_palette_luts();
  }
}

const CRGB16* palette_lut_for_index(uint8_t index)
{
  ensure_palettes_initialised();

  if (index >= (LED_CALIBRATED_COUNT + 1)) {
    index = LED_CALIBRATED_COUNT;
  }
  return g_palette_registry[index].lut_ptr;
}

uint16_t palette_lut_size(uint8_t index)
{
  ensure_palettes_initialised();
  return (index == 0) ? 0 : 256;
}

uint8_t palette_lut_count()
{
  return LED_CALIBRATED_COUNT + 1; // +1 for HSV/off entry
}

const char* palette_name_for_index(uint8_t index)
{
  ensure_palettes_initialised();

  if (index >= (LED_CALIBRATED_COUNT + 1)) {
    index = LED_CALIBRATED_COUNT;
  }
  return g_palette_registry[index].name;
}

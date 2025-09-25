#include "render.h"

#include <FastLED.h>

#include "config/features.h"
#ifdef LED_DATA_PIN
#undef LED_DATA_PIN
#endif
#include "config/hardware_config.h"
#include "constants.h"
#include "globals.h"
#include "core/FxEngine.h"
#include "core/PaletteManager.h"
#include "effects/EffectRegistry.h"
#include "utils/FastLEDLUTs.h"
#include "utils/StripMapper.h"
#include "../palettes/FastLED_Palettes.h"
#include "../palettes/Palettes_Crameri.h"
#if FEATURE_PERFORMANCE_MONITOR
#include "hardware/PerformanceMonitor.h"
#endif

extern CRGBPalette16 currentPalette;
extern CRGBPalette16 targetPalette;
extern uint8_t currentPaletteIndex;
#if FEATURE_SERIAL_MENU
#include "utils/SerialMenu.h"
#endif

extern CRGB leds[HardwareConfig::NUM_LEDS];
extern uint8_t angles[HardwareConfig::NUM_LEDS];
extern uint8_t radii[HardwareConfig::NUM_LEDS];
extern CRGBPalette16 currentPalette;
extern uint8_t currentPaletteIndex;
extern uint8_t fadeAmount;
extern uint8_t paletteSpeed;
extern uint8_t brightnessVal;
extern uint16_t fps;

// Instantiate LC globals sourced from the reference runtime.
FxEngine fxEngine;
PaletteManager paletteManager;
#if FEATURE_PERFORMANCE_MONITOR
PerformanceMonitor perfMon;
#endif
StripMapper stripMapper;

namespace lc {
namespace {

bool g_initialized = false;

void initialize_strip_mapping() {
  for (uint16_t i = 0; i < HardwareConfig::NUM_LEDS; ++i) {
    angles[i] = FastLEDLUTs::get_matrix_angle(i);
    radii[i] = FastLEDLUTs::get_matrix_radius(i);
  }
  if (HardwareConfig::NUM_LEDS > 0) {
    Serial.printf("[LC] LUT angle[0]=%u radius[0]=%u\n", angles[0], radii[0]);
  }
}

void refresh_palette_state() {
  ::currentPalette = paletteManager.getCurrentPalette();
  ::targetPalette = paletteManager.getTargetPalette();
  ::currentPaletteIndex = static_cast<uint8_t>(paletteManager.getCombinedIndex());
}

void ensure_initialized() {
  if (g_initialized) {
    return;
  }

  paletteManager.begin();
  paletteManager.setCombinedIndex(currentPaletteIndex, true);
  refresh_palette_state();
  initialize_strip_mapping();
  EffectRegistry::registerAllEffects(fxEngine);
  Serial.println(F("[LC] Effects registered:"));
  Serial.println(fxEngine.getNumEffects());

  g_initialized = true;
}

void copy_to_sb_buffers() {
  const uint16_t lc_led_count = HardwareConfig::NUM_LEDS;
  const uint16_t sb_led_count = NATIVE_RESOLUTION;
  const uint16_t copy_count = (lc_led_count < sb_led_count) ? lc_led_count : sb_led_count;

  for (uint16_t i = 0; i < copy_count; ++i) {
    const CRGB& src = leds[i];
    const float r = src.r / 255.0f;
    const float g = src.g / 255.0f;
    const float b = src.b / 255.0f;
    leds_16[i].r = SQ15x16(r);
    leds_16[i].g = SQ15x16(g);
    leds_16[i].b = SQ15x16(b);
  }

  for (uint16_t i = copy_count; i < sb_led_count; ++i) {
    leds_16[i].r = 0;
    leds_16[i].g = 0;
    leds_16[i].b = 0;
  }

  memcpy(leds_16_prev, leds_16, sizeof(CRGB16) * sb_led_count);
  memcpy(leds_16_fx, leds_16, sizeof(CRGB16) * sb_led_count);
}

}  // namespace

void render_lc_frame(const AudioMetrics& metrics) {
  (void)metrics;
  ensure_initialized();

  paletteManager.updatePaletteBlending();
  refresh_palette_state();
  if (debug_mode) {
    static bool palette_logged = false;
    if (!palette_logged) {
      palette_logged = true;
      const CRGB entry0 = ::currentPalette[0];
      const CRGB entry1 = ::currentPalette[1];
      Serial.printf("[LC] palette idx=%u entry0=(%u,%u,%u) entry1=(%u,%u,%u)\n",
                    ::currentPaletteIndex,
                    entry0.r, entry0.g, entry0.b,
                    entry1.r, entry1.g, entry1.b);
    }
  }
  fxEngine.render();
  if (debug_mode) {
    static uint32_t last_fx_log = 0;
    uint32_t now = millis();
    if (now - last_fx_log > 500) {
      last_fx_log = now;
      Serial.printf("[LC] leds[0]=(%u,%u,%u) leds[1]=(%u,%u,%u)\n",
                     leds[0].r, leds[0].g, leds[0].b,
                     leds[1].r, leds[1].g, leds[1].b);
    }
  }
  copy_to_sb_buffers();
}

}  // namespace lc

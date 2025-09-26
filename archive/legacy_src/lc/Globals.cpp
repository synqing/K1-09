#include <FastLED.h>
#include "config/hardware_config.h"

CRGB leds[HardwareConfig::NUM_LEDS];
uint8_t angles[HardwareConfig::NUM_LEDS];
uint8_t radii[HardwareConfig::NUM_LEDS];

CRGBPalette16 currentPalette;
CRGBPalette16 targetPalette;
uint8_t currentPaletteIndex = 0;

uint8_t brightnessVal = HardwareConfig::DEFAULT_BRIGHTNESS;
uint8_t fadeAmount = 20;
uint8_t paletteSpeed = 10;
uint16_t fps = HardwareConfig::DEFAULT_FPS;

#ifndef PALETTES_H
#define PALETTES_H

#include <FastLED.h>

// From ColorWavesWithPalettes by Mark Kriegsman: https://gist.github.com/kriegsman/8281905786e8b2632aeb

// Gradient Color Palette definitions for 33 different cpt-city color palettes.
//    956 bytes of PROGMEM for all of the palettes together,
//   +618 bytes of PROGMEM for gradient palette code (AVR).
//  1,494 bytes total for all 34 color palettes and associated code.

// Extern declarations for gradient palettes
extern const TProgmemRGBGradientPalette_byte ib_jul01_gp[] FL_PROGMEM;
extern const TProgmemRGBGradientPalette_byte es_vintage_57_gp[] FL_PROGMEM;
extern const TProgmemRGBGradientPalette_byte es_vintage_01_gp[] FL_PROGMEM;
extern const TProgmemRGBGradientPalette_byte es_rivendell_15_gp[] FL_PROGMEM;
extern const TProgmemRGBGradientPalette_byte rgi_15_gp[] FL_PROGMEM;
extern const TProgmemRGBGradientPalette_byte retro2_16_gp[] FL_PROGMEM;
extern const TProgmemRGBGradientPalette_byte Analogous_1_gp[] FL_PROGMEM;
extern const TProgmemRGBGradientPalette_byte es_pinksplash_08_gp[] FL_PROGMEM;
extern const TProgmemRGBGradientPalette_byte es_ocean_breeze_036_gp[] FL_PROGMEM;
extern const TProgmemRGBGradientPalette_byte departure_gp[] FL_PROGMEM;
extern const TProgmemRGBGradientPalette_byte es_landscape_64_gp[] FL_PROGMEM;
extern const TProgmemRGBGradientPalette_byte es_landscape_33_gp[] FL_PROGMEM;
extern const TProgmemRGBGradientPalette_byte rainbowsherbet_gp[] FL_PROGMEM;
extern const TProgmemRGBGradientPalette_byte gr65_hult_gp[] FL_PROGMEM;
extern const TProgmemRGBGradientPalette_byte gr64_hult_gp[] FL_PROGMEM;
extern const TProgmemRGBGradientPalette_byte Sunset_Real_gp[] FL_PROGMEM;
extern const TProgmemRGBGradientPalette_byte Coral_reef_gp[] FL_PROGMEM;
extern const TProgmemRGBGradientPalette_byte es_autumn_19_gp[] FL_PROGMEM;
extern const TProgmemRGBGradientPalette_byte GMT_drywet_gp[] FL_PROGMEM;

// Array of gradient color palettes
extern const TProgmemRGBGradientPaletteRef gGradientPalettes[];

// Count of palettes
extern const uint8_t gGradientPaletteCount;

// Array of palette names for serial menu
extern const char* const paletteNames[];

#endif // PALETTES_H
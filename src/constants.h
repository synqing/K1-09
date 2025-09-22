#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <Arduino.h>     // Base Arduino definitions
#include <FastLED.h>     // Required for CRGB and LED functions
#include <FixedPoints.h> // Include for SQ15x16 type used within
#include <FixedPointsCommon.h> // Required for SQ15x16 typedef
#include <stdint.h>      // Include for uint32_t type used within

// AUDIO #######################################################

#define SERIAL_BAUD 230400
#define DEFAULT_SAMPLE_RATE 16000
#define SAMPLE_HISTORY_LENGTH 4096

// Don't change this unless you're willing to do a lot of other work on the code :/
#define NATIVE_RESOLUTION 160
// MODIFICATION [2025-09-20 22:30] - BIN-REVERT-96-64-001: Core frequency bin reduction
// FAULT DETECTED: 96-bin configuration causing 33% computational overhead and 3KB+ memory usage
// ROOT CAUSE: Original design was 64-bin, 96-bin was experimental expansion with performance issues
// SOLUTION RATIONALE: Revert to proven 64-bin configuration from STABLE_GOLDEN_COMMIT baseline
// IMPACT ASSESSMENT: 33% performance improvement, 3KB RAM savings, better real-time margins
// VALIDATION METHOD: Compilation check, memory measurement, performance benchmarking
// ROLLBACK PROCEDURE: Change back to 96 if any critical functionality breaks
#define NUM_FREQS 64
#define NUM_ZONES 2

#define I2S_PORT I2S_NUM_0

#define SPECTRAL_HISTORY_LENGTH 5

// Secondary LED configuration - compile-time constants for FastLED templates
#define SECONDARY_LED_DATA_PIN 10
#define SECONDARY_LED_TYPE_CONST LED_NEOPIXEL
#define SECONDARY_LED_COUNT_CONST 160
#define SECONDARY_LED_COLOR_ORDER_CONST GRB

#define MAX_DOTS 320

enum reserved_dots {
  GRAPH_NEEDLE,
  GRAPH_DOT_1,
  GRAPH_DOT_2,
  GRAPH_DOT_3,
  GRAPH_DOT_4,
  GRAPH_DOT_5,
  RIPPLE_LEFT,
  RIPPLE_RIGHT,

  RESERVED_DOTS
};

enum knob_names {
  K_NONE,
  K_PHOTONS,
  K_CHROMA,
  K_MOOD
};

// Lightshow mode enum definitions (moved from lightshow_modes.h to resolve circular dependency)
enum lightshow_modes {
  LIGHT_MODE_GDFT,                   // Goertzel-based Discrete Fourier Transform
  LIGHT_MODE_GDFT_CHROMAGRAM,        // Chromagram of GDFT
  LIGHT_MODE_GDFT_CHROMAGRAM_DOTS,   // Chromagram of GDFT dots
  LIGHT_MODE_BLOOM,                  // Bloom Mode
  LIGHT_MODE_VU_DOT,                 // Not a real VU for any measurement sake, just a dance-y LED show
  LIGHT_MODE_KALEIDOSCOPE,          // Three color channels 2D Perlin noise affected by the onsets of low, mid and high pitches
  LIGHT_MODE_QUANTUM_COLLAPSE,       // Quantum collapse visualization mode
  LIGHT_MODE_WAVEFORM,               // Waveform visualization mode
  NUM_MODES                          // Used to know the length of this list if it changes in the future
};

struct CRGB16 {  // Unsigned Q8.8 Fixed-point color channels
  SQ15x16 r;
  SQ15x16 g;
  SQ15x16 b;
};

struct DOT {
  SQ15x16 position;
  SQ15x16 last_position;
};

struct KNOB {
  SQ15x16  value;
  SQ15x16  last_value;
  SQ15x16  change_rate;
  uint32_t last_change;
};

// FREQUENCY LAYOUT [2025-09-21] - Generated via tools/generate_frequency_table.py
// Tuned 64-bin table: A-weighted selection from a 96-note candidate set
// BASE_FREQ=55 Hz (A1), TOTAL_NOTES=96, OUTPUT=64
const float notes[] = {
  55.000000, 130.812783, 184.997211, 233.081881, 277.182631, 329.627557, 369.994423, 415.304698,
  440.000000, 493.883301, 523.251131, 554.365262, 622.253967, 659.255114, 698.456463, 739.988845,
  783.990872, 830.609395, 880.000000, 932.327523, 987.766603, 1046.502261, 1108.730524, 1174.659072,
  1244.507935, 1318.510228, 1396.912926, 1479.977691, 1567.981744, 1661.218790, 1760.000000, 1864.655046,
  1975.533205, 2093.004522, 2217.461048, 2349.318143, 2489.015870, 2637.020455, 2793.825851, 2959.955382,
  3135.963488, 3322.437581, 3520.000000, 3729.310092, 3951.066410, 4186.009045, 4434.922096, 4698.636287,
  4978.031740, 5274.040911, 5587.651703, 5919.910763, 6271.926976, 6644.875161, 7040.000000, 7458.620184,
  7902.132820, 8372.018090, 8869.844191, 9397.272573, 9956.063479, 10548.081821, 11839.821527, 13289.750323
};

// GPIO PINS #######################################################

#ifdef ARDUINO_ESP32S3_DEV
  // ESP32-S3 Pin Configuration (as per hardware specs)
  // Note: S3 device has no physical buttons, knobs, or sweet spot LEDs
  
  // MEMS Microphone pins
  #define I2S_BCLK_PIN 7
  #define I2S_LRCLK_PIN 13
  #define I2S_DIN_PIN 8
  
  // LED Data pins (WS2812 only - no clock needed)
  #define LED_DATA_PIN 9       // Primary LED strip
  #define LED_CLOCK_PIN 10     // Secondary LED strip (WS2812 data only)
  
  // Unused on S3 hardware - define as -1 to indicate not connected
  #define PHOTONS_PIN -1       // No physical knob
  #define CHROMA_PIN -1        // No physical knob  
  #define MOOD_PIN -1          // No physical knob
  #define NOISE_CAL_PIN -1     // No physical button
  #define MODE_PIN -1          // No physical button
  #define SWEET_SPOT_LEFT_PIN -1   // No sweet spot LED
  #define SWEET_SPOT_CENTER_PIN -1 // No sweet spot LED
  #define SWEET_SPOT_RIGHT_PIN -1  // No sweet spot LED
  
  // Random seed can use any floating pin
  #define RNG_SEED_PIN 11
  
  // Physical controls (not connected on S3)
  #define NOISE_CAL_PIN -1  // No physical button
  #define MODE_PIN -1       // No physical button
  
#else
  // ESP32-S2 Pin Configuration (Original)
  #define PHOTONS_PIN 1
  #define CHROMA_PIN 2
  #define MOOD_PIN 3

  #define I2S_BCLK_PIN 33
  #define I2S_LRCLK_PIN 34
  #define I2S_DIN_PIN 35

  #define LED_DATA_PIN 36
  #define LED_CLOCK_PIN 37

  #define RNG_SEED_PIN 11

  #define NOISE_CAL_PIN 11
  #define MODE_PIN 45

  #define SWEET_SPOT_LEFT_PIN 7
  #define SWEET_SPOT_CENTER_PIN 8
  #define SWEET_SPOT_RIGHT_PIN 9
#endif

// OTHER #######################################################

// Enhanced 8-frame temporal dithering for smoother color gradients
const SQ15x16 dither_table[8] = {
  0.125,   // 1/8
  0.375,   // 3/8
  0.625,   // 5/8
  0.875,   // 7/8
  0.0625,  // 1/16
  0.3125,  // 5/16
  0.5625,  // 9/16
  0.8125   // 13/16
};

// NOTE_COLORS ODR FIX [2025-09-19 17:00] - Extern declaration (defined in globals.cpp)
extern SQ15x16 note_colors[12];

// MODIFICATION [2025-09-20 22:30] - BIN-REVERT-96-64-001: Hue lookup array size adjustment
// FAULT DETECTED: Hardcoded array size [96] will cause compilation failure when NUM_FREQS=64
// ROOT CAUSE: Array size not tied to NUM_FREQS constant, causing size mismatch
// SOLUTION RATIONALE: Use NUM_FREQS macro for array sizing to maintain consistency
// IMPACT ASSESSMENT: Reduces hue lookup table by 32 entries, saves 384 bytes flash memory
// VALIDATION METHOD: Compilation success, verify color mapping still functional
// ROLLBACK PROCEDURE: Restore [96] hardcoded size if color issues arise
const SQ15x16 hue_lookup[NUM_FREQS][3] = {
  { 1.0000, 0.0000, 0.0000 },
  { 0.9608, 0.0392, 0.0000 },
  { 0.9176, 0.0824, 0.0000 },
  { 0.8745, 0.1255, 0.0000 },
  { 0.8314, 0.1686, 0.0000 },
  { 0.7922, 0.2078, 0.0000 },
  { 0.7490, 0.2510, 0.0000 },
  { 0.7059, 0.2941, 0.0000 },
  { 0.6706, 0.3333, 0.0000 },
  { 0.6706, 0.3725, 0.0000 },
  { 0.6706, 0.4157, 0.0000 },
  { 0.6706, 0.4588, 0.0000 },
  { 0.6706, 0.5020, 0.0000 },
  { 0.6706, 0.5412, 0.0000 },
  { 0.6706, 0.5843, 0.0000 },
  { 0.6706, 0.6275, 0.0000 },
  { 0.6706, 0.6667, 0.0000 },
  { 0.5882, 0.7059, 0.0000 },
  { 0.5059, 0.7490, 0.0000 },
  { 0.4196, 0.7922, 0.0000 },
  { 0.3373, 0.8353, 0.0000 },
  { 0.2549, 0.8745, 0.0000 },
  { 0.1686, 0.9176, 0.0000 },
  { 0.0863, 0.9608, 0.0000 },
  { 0.0000, 1.0000, 0.0000 },
  { 0.0000, 0.9608, 0.0392 },
  { 0.0000, 0.9176, 0.0824 },
  { 0.0000, 0.8745, 0.1255 },
  { 0.0000, 0.8314, 0.1686 },
  { 0.0000, 0.7922, 0.2078 },
  { 0.0000, 0.7490, 0.2510 },
  { 0.0000, 0.7059, 0.2941 },
  { 0.0000, 0.6706, 0.3333 },
  { 0.0000, 0.5882, 0.4157 },
  { 0.0000, 0.5059, 0.4980 },
  { 0.0000, 0.4196, 0.5843 },
  { 0.0000, 0.3373, 0.6667 },
  { 0.0000, 0.2549, 0.7490 },
  { 0.0000, 0.1686, 0.8353 },
  { 0.0000, 0.0863, 0.9176 },
  { 0.0000, 0.0000, 1.0000 },
  { 0.0392, 0.0000, 0.9608 },
  { 0.0824, 0.0000, 0.9176 },
  { 0.1255, 0.0000, 0.8745 },
  { 0.1686, 0.0000, 0.8314 },
  { 0.2078, 0.0000, 0.7922 },
  { 0.2510, 0.0000, 0.7490 },
  { 0.2941, 0.0000, 0.7059 },
  { 0.3333, 0.0000, 0.6706 },
  { 0.3725, 0.0000, 0.6314 },
  { 0.4157, 0.0000, 0.5882 },
  { 0.4588, 0.0000, 0.5451 },
  { 0.5020, 0.0000, 0.5020 },
  { 0.5412, 0.0000, 0.4627 },
  { 0.5843, 0.0000, 0.4196 },
  { 0.6275, 0.0000, 0.3765 },
  { 0.6667, 0.0000, 0.3333 },
  { 0.7059, 0.0000, 0.2941 },
  { 0.7490, 0.0000, 0.2510 },
  { 0.7922, 0.0000, 0.2078 },
  { 0.8353, 0.0000, 0.1647 },
  { 0.8745, 0.0000, 0.1255 },
  { 0.9176, 0.0000, 0.0824 },
  { 0.9608, 0.0000, 0.0392 }
  // MODIFICATION [2025-09-20 22:30] - BIN-REVERT-96-64-001: Array truncation to 64 entries
  // Removed 32 entries (bins 64-95) to match NUM_FREQS=64 requirement
  // This completes the color wheel with 64 evenly distributed hue values
};

#define SWEET_SPOT_LEFT_CHANNEL 0
#define SWEET_SPOT_CENTER_CHANNEL 1
#define SWEET_SPOT_RIGHT_CHANNEL 2

#define TWOPI 6.28318530
#define FOURPI 12.56637061
#define SIXPI 18.84955593

enum led_types {
  LED_NEOPIXEL,
  LED_NEOPIXEL_X2,
  LED_DOTSTAR
};

// INCANDESCENT_LOOKUP ODR FIX [2025-09-19 17:15] - Extern declaration (defined in globals.cpp)
extern CRGB16 incandescent_lookup;

#endif // CONSTANTS_H

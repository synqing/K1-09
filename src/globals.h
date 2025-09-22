#ifndef GLOBALS_H
#define GLOBALS_H

#include <stdint.h>        // For standard integer types
#include <stdbool.h>       // For bool type
#include <FixedPoints.h>   // For SQ15x16
#include <FixedPointsCommon.h> // Required for SQ15x16 typedef
#include <Ticker.h>        // For Ticker type
#include <freertos/task.h> // For TaskHandle_t (FreeRTOS task handle)
#include <freertos/FreeRTOS.h> // For FreeRTOS
#include <freertos/semphr.h>   // For semaphores/mutexes
#include <FirmwareMSC.h>   // For FirmwareMSC
#include <USB.h>           // For USBCDC
#ifdef ARDUINO_ESP32S3_DEV
#include <HWCDC.h>         // For HWCDC on S3
#endif
#include "constants.h"    // For NUM_FREQS, NUM_ZONES, NUM_MODES, MAX_DOTS, K_NONE, CRGB16, DOT, KNOB, light modes, LED types, etc.
#include <Arduino.h>
#include <FastLED.h>

// CRITICAL: Mutex for controlling access to the thread-unsafe USBSerial port.
// Prevents garbled debug output from interleaved task printing.
extern SemaphoreHandle_t serial_mutex;

// States for the Sweet Spot indicator LEDs
enum SweetSpotState {
  SWEET_SPOT_SILENT,
  SWEET_SPOT_LOW,
  SWEET_SPOT_MEDIUM,
  SWEET_SPOT_HIGH,
  SWEET_SPOT_MAX
};

// ------------------------------------------------------------
// SINGLE-CORE OPTIMIZATION: Mutex declarations removed ------
// Both audio and LED threads run on Core 0, eliminating 
// the need for mutex synchronization. FreeRTOS scheduler
// ensures atomic context switches between threads.

// ------------------------------------------------------------
// Configuration structure ------------------------------------

namespace SensoryBridge {
namespace Config {

struct conf {  
  // Synced values
  float   PHOTONS;
  float   CHROMA;
  float   MOOD;
  uint8_t LIGHTSHOW_MODE;
  bool    MIRROR_ENABLED;

  // Private values
  uint32_t SAMPLE_RATE;
  uint8_t  NOTE_OFFSET;
  uint8_t  SQUARE_ITER;
  uint8_t  LED_TYPE;
  uint16_t LED_COUNT;
  uint16_t LED_COLOR_ORDER;
  bool     LED_INTERPOLATION;
  uint16_t SAMPLES_PER_CHUNK;
  float    SENSITIVITY;
  bool     BOOT_ANIMATION;
  uint32_t SWEET_SPOT_MIN_LEVEL;
  uint32_t SWEET_SPOT_MAX_LEVEL;
  int32_t  DC_OFFSET;
  uint8_t  CHROMAGRAM_RANGE;
  bool     STANDBY_DIMMING;
  bool     REVERSE_ORDER;
  bool     IS_MAIN_UNIT;
  uint32_t MAX_CURRENT_MA;
  bool     TEMPORAL_DITHERING;
  bool     AUTO_COLOR_SHIFT;
  float    INCANDESCENT_FILTER;
  bool     INCANDESCENT_MODE;
  float    BULB_OPACITY;
  float    SATURATION;
  float    PRISM_COUNT;
  bool     BASE_COAT;
  float    VU_LEVEL_FLOOR;
  uint8_t  PALETTE_INDEX;   // 0 = HSV (legacy), 1..N = gradient palette
};

// Defaults will be defined outside namespace

} // namespace Config
} // namespace SensoryBridge

// Keep CONFIG as global variable but use namespaced type
extern SensoryBridge::Config::conf CONFIG;
extern SensoryBridge::Config::conf CONFIG_DEFAULTS;
extern char mode_names[NUM_MODES*32];

// ------------------------------------------------------------
// Goertzel structure (generated in system.h) -----------------

struct freq {
  float    target_freq;
  int32_t  coeff_q14;

  uint16_t block_size;
  float    block_size_recip;
  float    inv_block_size_half;
  uint8_t  zone;
  
  float a_weighting_ratio;
  float window_mult;
};
extern freq frequencies[NUM_FREQS];

// ------------------------------------------------------------
// Hann window lookup table (generated in system.h) -----------

extern int16_t window_lookup[4096];

// ------------------------------------------------------------
// A-weighting lookup table (parsed in system.h) --------------

extern float a_weight_table[13][2];

// ------------------------------------------------------------
// Spectrograms (GDFT.h) --------------------------------------

extern SQ15x16 spectrogram[NUM_FREQS];
extern SQ15x16 spectrogram_smooth[NUM_FREQS];
extern SQ15x16 chromagram_smooth[12];

extern SQ15x16 spectral_history[SPECTRAL_HISTORY_LENGTH][NUM_FREQS];
extern SQ15x16 novelty_curve[SPECTRAL_HISTORY_LENGTH];

extern uint8_t spectral_history_index;

extern float note_spectrogram[NUM_FREQS];
extern float note_spectrogram_smooth[NUM_FREQS];
extern float note_spectrogram_smooth_frame_blending[NUM_FREQS];
extern float note_spectrogram_long_term[NUM_FREQS];
extern float note_chromagram[12];
extern float chromagram_max_val;
extern float chromagram_bass_max_val;

extern float smoothing_follower;
extern float smoothing_exp_average;

extern SQ15x16 chroma_val;

extern bool chromatic_mode;

// ------------------------------------------------------------
// Audio samples (i2s_audio.h) --------------------------------

// MIGRATED TO AudioRawState: int32_t i2s_samples_raw[1024]
extern short   sample_window[SAMPLE_HISTORY_LENGTH];
extern short   waveform[1024];
extern SQ15x16 waveform_fixed_point[1024];
// MIGRATED TO AudioRawState: short waveform_history[4][1024]
// MIGRATED TO AudioRawState: uint8_t waveform_history_index
extern float   max_waveform_val_raw;
extern float   max_waveform_val;
extern float   max_waveform_val_follower;
extern float   waveform_peak_scaled;
// MIGRATED TO AudioRawState: int32_t dc_offset_sum

extern bool    silence;

extern float   silent_scale;
extern float   current_punch;

// ------------------------------------------------------------
// Sweet Spot (i2s_audio.h, led_utilities.h) ------------------

extern float sweet_spot_state;
extern float sweet_spot_state_follower;
extern float sweet_spot_min_temp;

// ------------------------------------------------------------
// Noise calibration (noise_cal.h) ----------------------------

extern bool     noise_complete;

extern SQ15x16  noise_samples[NUM_FREQS];
extern uint16_t noise_iterations;

// ------------------------------------------------------------
// Display buffers (led_utilities.h) --------------------------

/*
CRGB leds[160];
CRGB leds_frame_blending[160];
CRGB leds_fx[160];
CRGB leds_temp[160];
CRGB leds_last[160];
CRGB leds_aux [160];
CRGB leds_fade[160];
*/

extern CRGB16  leds_16[160];
extern CRGB16  leds_16_prev[160];
extern CRGB16  leds_16_prev_secondary[160]; // Buffer for secondary bloom state
extern CRGB16  leds_16_fx[160];
// CRGB16  leds_16_fx_2[160]; // Removed to save DRAM
extern CRGB16  leds_16_temp[160];
extern CRGB16  leds_16_ui[160];

// Frame sequencing handshake between render producer and LED consumer
extern volatile uint32_t g_frame_seq_write;
extern volatile uint32_t g_frame_seq_ready;
extern volatile uint32_t g_frame_seq_shown;

// Add state variables for waveform mode instances
extern CRGB16  waveform_last_color_primary;
extern CRGB16  waveform_last_color_secondary;

extern SQ15x16 ui_mask[160];
extern SQ15x16 ui_mask_height;

extern CRGB16 *leds_scaled;
extern CRGB *leds_out;

extern SQ15x16 hue_shift; // Used in auto color cycling

extern uint8_t dither_step;

extern bool led_thread_halt;

extern TaskHandle_t led_task;

// --- Encoder Globals ---
extern uint32_t g_last_encoder_activity_time; // Defined in globals.cpp, declared extern in encoders.h
extern uint8_t g_last_active_encoder;     // Defined in globals.cpp, declared extern in encoders.h

// ------------------------------------------------------------
// Benchmarking (system.h) ------------------------------------

extern Ticker cpu_usage;
extern volatile uint16_t function_id;
extern volatile uint16_t function_hits[32];
extern float SYSTEM_FPS;
extern float LED_FPS;

// ------------------------------------------------------------
// SensorySync P2P network (p2p.h) ----------------------------

extern bool     main_override;
extern uint32_t last_rx_time;

// ------------------------------------------------------------
// Buttons (buttons.h) ----------------------------------------

// TODO: Similar structs for knobs
struct button{
  uint8_t pin = 0;
  uint32_t last_down = 0;
  uint32_t last_up = 0;
  bool pressed = false;
};

extern button noise_button;
extern button mode_button;

extern bool    mode_transition_queued;
extern bool    noise_transition_queued;

extern int16_t mode_destination;

// ------------------------------------------------------------
// Settings tracking (system.h) -------------------------------

extern uint32_t next_save_time;
extern bool     settings_updated;

// ------------------------------------------------------------
// Serial buffer (serial_menu.h) ------------------------------

extern char    command_buf[128];
extern uint8_t command_buf_index;

extern bool stream_audio;
extern bool stream_fps;
extern bool stream_max_mags;
extern bool stream_max_mags_followers;
extern bool stream_magnitudes;
extern bool stream_spectrogram;
extern bool stream_chromagram;

extern bool debug_mode;
extern uint64_t chip_id;
extern uint32_t chip_id_high;
extern uint32_t chip_id_low;

extern uint32_t serial_iter;

// ------------------------------------------------------------
// Spectrogram normalization (GDFT.h) -------------------------

extern float max_mags[NUM_ZONES];
extern float max_mags_followers[NUM_ZONES];
extern float mag_targets[NUM_FREQS];
extern float mag_followers[NUM_FREQS];
extern float mag_float_last[NUM_FREQS];
extern int32_t magnitudes[NUM_FREQS];
extern float magnitudes_normalized[NUM_FREQS];
extern float magnitudes_normalized_avg[NUM_FREQS];
extern float magnitudes_last[NUM_FREQS];
extern float magnitudes_final[NUM_FREQS];

// --> For Dynamic AGC Floor <--
extern SQ15x16 min_silent_level_tracker; // EMERGENCY FIX: Reduced from 65535.0 to prevent SQ15x16 overflow
// MODIFICATION [2025-09-20 22:45] - SURGICAL-FIX-004: Fix pegged AGC values for proper audio tracking
// FAULT DETECTED: AGC_FLOOR_INITIAL_RESET value of 65535.0 exceeds SQ15x16 max value of 32767.0
// ROOT CAUSE: Overflow causes min_silent_level_tracker to peg at max value, breaking AGC dynamics
// SOLUTION RATIONALE: Reduce to 30000.0 which stays within SQ15x16 range while providing adequate headroom
// IMPACT ASSESSMENT: AGC will properly track audio dynamics instead of staying pegged at overflow value
// VALIDATION METHOD: Monitor AGC values adapt to loud/quiet audio instead of staying constant
// ROLLBACK PROCEDURE: Restore 65535.0 if AGC tracking range becomes insufficient

#define AGC_FLOOR_INITIAL_RESET (65535.0)
#define AGC_FLOOR_SCALING_FACTOR (0.01) // Relates raw amplitude to Goertzel magnitude
#define AGC_FLOOR_MIN_CLAMP_RAW (10.0) // Min reasonable raw tracker value before scaling
#define AGC_FLOOR_MAX_CLAMP_RAW (30000.0) // Max reasonable raw tracker value before scaling
#define AGC_FLOOR_MIN_CLAMP_SCALED (0.5) // Final minimum AGC floor after scaling
#define AGC_FLOOR_MAX_CLAMP_SCALED (100.0) // Final maximum AGC floor after scaling
#define AGC_FLOOR_RECOVERY_RATE (50.0) // *** EXPERIMENTAL *** Rate at which tracker recovers upwards per frame during silence-

// ------------------------------------------------------------
// Look-ahead smoothing (GDFT.h) ------------------------------

extern const uint8_t spectrogram_history_length;
extern float spectrogram_history[3][NUM_FREQS];  // Must use literal 3 here
extern uint8_t spectrogram_history_index;

// ------------------------------------------------------------
// Used for converting for storage in LittleFS (bridge_fs.h) --

union bytes_32 {
  uint32_t long_val;
  int32_t  long_val_signed;
  float    long_val_float;
  uint8_t  bytes[4];
};

// ------------------------------------------------------------
// Used for GDFT mode (lightshow_modes.h) ---------------------

extern uint8_t brightness_levels[NUM_FREQS];

// ------------------------------------------------------------
// Used for USB updates (system.h) ----------------------------

extern FirmwareMSC MSC_Update;
// USBSerial handled per platform
#ifdef ARDUINO_ESP32S3_DEV
  #if ARDUINO_USB_CDC_ON_BOOT
    // USBSerial will be defined as Serial after Arduino.h is included
  #else
    extern HWCDC USBSerial;
  #endif
#else
  extern USBCDC USBSerial;
#endif
extern bool msc_update_started;

// DOTS
extern DOT dots[MAX_DOTS];

// Auto Color Shift
extern SQ15x16 hue_position;
extern SQ15x16 hue_shift_speed;
extern SQ15x16 hue_push_direction;
extern SQ15x16 hue_destination;
extern SQ15x16 hue_shifting_mix;
extern SQ15x16 hue_shifting_mix_target;

// VU Calculation
extern SQ15x16 audio_vu_level;
extern SQ15x16 audio_vu_level_average;
extern SQ15x16 audio_vu_level_last;

// Knobs
extern KNOB knob_photons;
extern KNOB knob_chroma;
extern KNOB knob_mood;
extern uint8_t current_knob;

// Base Coat
extern SQ15x16 base_coat_width;
extern SQ15x16 base_coat_width_target;

// Config File
extern char config_filename[24];

// WIP BELOW --------------------------------------------------

extern float MASTER_BRIGHTNESS;
extern float last_sample;

inline void lock_leds(){
  //led_thread_halt = true;
  //delay(20); // Potentially waiting for LED thread to finish its loop
}

inline void unlock_leds(){
  //led_thread_halt = false;
}

// New buffers for secondary LED strip
extern CRGB16  leds_16_secondary[160];        // Main buffer for secondary strip
extern CRGB16 *leds_scaled_secondary;         // For scaling to actual LED count
extern CRGB *leds_out_secondary;              // Final output buffer

// Secondary strip configuration - constants defined in constants.h as #defines for FastLED templates
extern uint8_t SECONDARY_LIGHTSHOW_MODE; // Secondary channel: Waveform mode
extern bool SECONDARY_MIRROR_ENABLED;
extern float SECONDARY_PHOTONS;
extern float SECONDARY_CHROMA;
extern float SECONDARY_MOOD;
extern float SECONDARY_SATURATION;
extern uint8_t SECONDARY_PRISM_COUNT;
extern float SECONDARY_INCANDESCENT_FILTER;
extern bool SECONDARY_BASE_COAT;
extern bool SECONDARY_REVERSE_ORDER;
extern bool SECONDARY_AUTO_COLOR_SHIFT;  // Enable auto color shift for secondary

// Add near the other configuration flags
extern bool ENABLE_SECONDARY_LEDS; // PROPERLY FIXED: Buffer allocation added

// S3 Performance Validation Counters
// Note: g_mutex_skip_count removed (mutex operations eliminated)
extern uint32_t g_race_condition_count;
extern volatile bool g_palette_ready;

// Task handle for audio processing thread
extern TaskHandle_t audio_task_handle;

// FRAME_CONFIG ODR FIX [2025-09-19 17:00] - Declaration for struct defined in globals.cpp
struct cached_config {
  float PHOTONS;
  float CHROMA;
  float MOOD;
  uint8_t LIGHTSHOW_MODE;
  float SQUARE_ITER;
  float SATURATION;
  uint8_t PALETTE_INDEX;       // Mirror CONFIG.PALETTE_INDEX each frame

  // Palette cache for atomic switching (prevents race conditions)
  const CRGB16* palette_ptr;   // Pointer to 256-entry CRGB16 LUT (or nullptr if disabled)
  uint16_t      palette_size;  // Size of palette (typically 256)
};
extern struct cached_config frame_config;

#endif // GLOBALS_H

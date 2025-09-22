// globals.cpp - Global variable definitions (moved from globals.h to fix ODR violation)
// CRITICAL: Preserve exact initialization syntax - DO NOT CHANGE aggregate initialization!

#include "../globals.h"
#include <Arduino.h>

// Forward declaration for LerpParams (defined in led_utilities.h)
struct LerpParams;

// Keep CONFIG as global variable but use namespaced type
// CRITICAL: Preserve exact aggregate initialization syntax - DO NOT ADD explicit constructors!
SensoryBridge::Config::conf CONFIG = {
  // Synced values
  1.00, // PHOTONS
  0.50, // CHROMA - Set to 50% for visible palette colors (was 0.00 = grayscale)
  0.05, // MOOD
  LIGHT_MODE_WAVEFORM, // LIGHTSHOW_MODE - Primary channel: Waveform mode for palette testing
  true,           // MIRROR_ENABLED

  // Private values
  DEFAULT_SAMPLE_RATE, // SAMPLE_RATE
  0,                   // NOTE_OFFSET
  1,                   // SQUARE_ITER
  LED_NEOPIXEL,        // LED_TYPE
  160,                 // LED_COUNT
  GRB,                 // LED_COLOR_ORDER
  true,                // LED_INTERPOLATION
  256,                 // SAMPLES_PER_CHUNK - Balanced latency and resolution (8ms window)
  1.0,                 // SENSITIVITY - 1.0 = no gain adjustment, >1.0 = amplify, <1.0 = attenuate
  true,                // BOOT_ANIMATION
  750,                 // SWEET_SPOT_MIN_LEVEL
  30000,               // SWEET_SPOT_MAX_LEVEL
  -14800,              // DC_OFFSET - measured from actual ESP32-S3 I2S input
  60,                  // CHROMAGRAM_RANGE
  true,                // STANDBY_DIMMING
  false,               // REVERSE_ORDER
  false,               // IS_MAIN_UNIT
  1500,                // MAX_CURRENT_MA
  true,                // TEMPORAL_DITHERING
  false,               // AUTO_COLOR_SHIFT - Disabled by default to preserve palette colors
  0.50,                // INCANDESCENT_FILTER
  false,               // INCANDESCENT_MODE
  0.00,                // BULB_OPACITY
  1.00,                // SATURATION
  1.42f,               // PRISM_COUNT
  false,               // BASE_COAT
  0.00,                // VU_LEVEL_FLOOR - CRITICAL: Must be 0.00 for proper sensitivity
  0,                   // PALETTE_INDEX - Start in HSV mode (0 = HSV, 1+ = palettes)
};

SensoryBridge::Config::conf CONFIG_DEFAULTS;

char mode_names[NUM_MODES*32] = { 0 };

// ------------------------------------------------------------
// Goertzel structure (generated in system.h) -----------------
freq frequencies[NUM_FREQS];

// ------------------------------------------------------------
// Hann window lookup table (generated in system.h) -----------
int16_t window_lookup[4096] = { 0 };

// ------------------------------------------------------------
// A-weighting lookup table (parsed in system.h) --------------
float a_weight_table[13][2] = {
  { 10,    -70.4 },  // hz, db
  { 20,    -50.5 },
  { 40,    -34.6 },
  { 80,    -22.5 },
  { 160,   -13.4 },
  { 315,    -6.6 },
  { 630,    -1.9 },
  { 1000,    0.0 },
  { 1250,    0.6 },
  { 2500,    1.3 },
  { 5000,    0.5 },
  { 10000,  -2.5 },
  { 20000,  -9.3 }
};

// ------------------------------------------------------------
// Spectrograms (GDFT.h) --------------------------------------
SQ15x16 spectrogram[NUM_FREQS] = { 0.0 };
SQ15x16 spectrogram_smooth[NUM_FREQS] = { 0.0 };
SQ15x16 chromagram_smooth[12] = { 0.0 };

SQ15x16 spectral_history[SPECTRAL_HISTORY_LENGTH][NUM_FREQS];
SQ15x16 novelty_curve[SPECTRAL_HISTORY_LENGTH] = { 0.0 };

uint8_t spectral_history_index = 0;

float note_spectrogram[NUM_FREQS] = {0};
float note_spectrogram_smooth[NUM_FREQS] = {0};
float note_spectrogram_smooth_frame_blending[NUM_FREQS] = {0};
float note_spectrogram_long_term[NUM_FREQS] = {0};
float note_chromagram[12]  = {0};
float chromagram_max_val = 0.0;
float chromagram_bass_max_val = 0.0;

float smoothing_follower    = 0.0;
float smoothing_exp_average = 0.0;

SQ15x16 chroma_val = 1.0;

bool chromatic_mode = true;

// ------------------------------------------------------------
// Audio samples (i2s_audio.h) --------------------------------
short   sample_window[SAMPLE_HISTORY_LENGTH] = { 0 };
short   waveform[1024]                       = { 0 };
SQ15x16 waveform_fixed_point[1024]           = { 0 };
float   max_waveform_val_raw = 0.0;
float   max_waveform_val = 0.0;
float   max_waveform_val_follower = 0.0;
float   waveform_peak_scaled = 0.0;

bool    silence = false;

float   silent_scale = 1.0;
float   current_punch = 0.0;

// ------------------------------------------------------------
// Sweet Spot (i2s_audio.h, led_utilities.h) ------------------
float sweet_spot_state = 0;
float sweet_spot_state_follower = 0;
float sweet_spot_min_temp = 0;

// ------------------------------------------------------------
// Noise calibration (noise_cal.h) ----------------------------
bool     noise_complete = true;

SQ15x16  noise_samples[NUM_FREQS] = { 1 };
uint16_t noise_iterations = 0;

// ------------------------------------------------------------
// Display buffers (led_utilities.h) --------------------------
CRGB16  leds_16[160];
CRGB16  leds_16_prev[160];
CRGB16  leds_16_prev_secondary[160]; // Buffer for secondary bloom state
CRGB16  leds_16_fx[160];
CRGB16  leds_16_temp[160];
CRGB16  leds_16_ui[160];

volatile uint32_t g_frame_seq_write = 0;
volatile uint32_t g_frame_seq_ready = 0;
volatile uint32_t g_frame_seq_shown = 0;

// Add state variables for waveform mode instances
// CRITICAL: Preserve exact aggregate initialization syntax!
CRGB16  waveform_last_color_primary = {0,0,0};
CRGB16  waveform_last_color_secondary = {0,0,0};

SQ15x16 ui_mask[160];
SQ15x16 ui_mask_height = 0.0;

CRGB16 *leds_scaled;
CRGB *leds_out;

SQ15x16 hue_shift = 0.0; // Used in auto color cycling

uint8_t dither_step = 0;

bool led_thread_halt = false;

TaskHandle_t led_task;

// --- Encoder Globals ---
uint32_t g_last_encoder_activity_time = 0; // Defined here, declared extern in encoders.h
uint8_t g_last_active_encoder = 255;     // Defined here, declared extern in encoders.h

// ------------------------------------------------------------
// Benchmarking (system.h) ------------------------------------
Ticker cpu_usage;
volatile uint16_t function_id = 0;
volatile uint16_t function_hits[32] = {0};
float SYSTEM_FPS = 0.0;
float LED_FPS    = 0.0;

// ------------------------------------------------------------
// SensorySync P2P network (p2p.h) ----------------------------
bool     main_override = true;
uint32_t last_rx_time = 0;

// ------------------------------------------------------------
// Buttons (buttons.h) ----------------------------------------
button noise_button;
button mode_button;

bool    mode_transition_queued  = false;
bool    noise_transition_queued = false;

int16_t mode_destination = -1;

// ------------------------------------------------------------
// Settings tracking (system.h) -------------------------------
uint32_t next_save_time = 0;
bool     settings_updated = false;

// ------------------------------------------------------------
// Serial buffer (serial_menu.h) ------------------------------
char    command_buf[128] = {0};
uint8_t command_buf_index = 0;

bool stream_audio = false;
bool stream_fps = false;
bool stream_max_mags = false;
bool stream_max_mags_followers = false;
bool stream_magnitudes = false;
bool stream_spectrogram = false;
bool stream_chromagram = false;

bool debug_mode = true;
uint64_t chip_id = 0;
uint32_t chip_id_high = 0;
uint32_t chip_id_low  = 0;

uint32_t serial_iter = 0;

// ------------------------------------------------------------
// Spectrogram normalization (GDFT.h) -------------------------
float max_mags[NUM_ZONES] = { 0.000 };
float max_mags_followers[NUM_ZONES] = { 0.000 };
float mag_targets[NUM_FREQS] = { 0.000 };
float mag_followers[NUM_FREQS] = { 0.000 };
float mag_float_last[NUM_FREQS] = { 0.000 };
int32_t magnitudes[NUM_FREQS] = { 0 };
float magnitudes_normalized[NUM_FREQS] = { 0.000 };
float magnitudes_normalized_avg[NUM_FREQS] = { 0.000 };
float magnitudes_last[NUM_FREQS] = { 0.000 };
float magnitudes_final[NUM_FREQS] = { 0.000 };

// --> For Dynamic AGC Floor <--
SQ15x16 min_silent_level_tracker = 65535.0; // Initialize high, tracks min max_waveform_val_raw during silence

// ------------------------------------------------------------
// Look-ahead smoothing (GDFT.h) ------------------------------
const uint8_t spectrogram_history_length = 3;
float spectrogram_history[spectrogram_history_length][NUM_FREQS];
uint8_t spectrogram_history_index = 0;

// ------------------------------------------------------------
// Used for GDFT mode (lightshow_modes.h) ---------------------
uint8_t brightness_levels[NUM_FREQS] = { 0 };

// ------------------------------------------------------------
// Used for USB updates (system.h) ----------------------------
FirmwareMSC MSC_Update;
// USBSerial handled per platform
#ifdef ARDUINO_ESP32S3_DEV
  #if ARDUINO_USB_CDC_ON_BOOT
    // USBSerial will be defined as Serial after Arduino.h is included
  #else
    HWCDC USBSerial;  // Definition moved here from globals.h
  #endif
#else
  USBCDC USBSerial;
#endif
bool msc_update_started = false;

// DOTS
DOT dots[MAX_DOTS];

// Auto Color Shift
SQ15x16 hue_position = 0.0;
SQ15x16 hue_shift_speed = 0.0;
SQ15x16 hue_push_direction = -1.0;
SQ15x16 hue_destination = 0.0;
SQ15x16 hue_shifting_mix = -0.35;
SQ15x16 hue_shifting_mix_target = 1.0;

// VU Calculation
SQ15x16 audio_vu_level = 0.0;
SQ15x16 audio_vu_level_average = 0.0;
SQ15x16 audio_vu_level_last = 0.0;

// Knobs
KNOB knob_photons;
KNOB knob_chroma;
KNOB knob_mood;
uint8_t current_knob = K_NONE;

// Base Coat
SQ15x16 base_coat_width        = 0.0;
SQ15x16 base_coat_width_target = 1.0;

// Config File
char config_filename[24];

// WIP BELOW --------------------------------------------------
float MASTER_BRIGHTNESS = 0.0;
float last_sample = 0;

// COMMENTED OUT [2025-09-19 16:30] - Redefinition error
// These functions are already defined elsewhere, likely in led_utilities.h
// void lock_leds(){
//   //led_thread_halt = true;
//   //delay(20); // Potentially waiting for LED thread to finish its loop
// }

// void unlock_leds(){
//   //led_thread_halt = false;
// }

// New buffers for secondary LED strip
CRGB16  leds_16_secondary[160];        // Main buffer for secondary strip
CRGB16 *leds_scaled_secondary;         // For scaling to actual LED count
CRGB *leds_out_secondary;              // Final output buffer

// Secondary strip configuration - constants moved to constants.h as #defines for FastLED templates
uint8_t SECONDARY_LIGHTSHOW_MODE = LIGHT_MODE_WAVEFORM; // Secondary channel: Waveform mode
bool SECONDARY_MIRROR_ENABLED = true;
float SECONDARY_PHOTONS = 1.0;
float SECONDARY_CHROMA = 0.50; // Set to 50% for visible palette colors (was 0.0 = grayscale)
float SECONDARY_MOOD = 0.05;
float SECONDARY_SATURATION = 1.0;
uint8_t SECONDARY_PRISM_COUNT = 0;
float SECONDARY_INCANDESCENT_FILTER = 0.5;
bool SECONDARY_BASE_COAT = false;
bool SECONDARY_REVERSE_ORDER = false;
bool SECONDARY_AUTO_COLOR_SHIFT = true;  // Enable auto color shift for secondary

// Add near the other configuration flags
bool ENABLE_SECONDARY_LEDS = true; // PROPERLY FIXED: Buffer allocation added

// S3 Performance Validation Counters (definitions)
// Note: g_mutex_skip_count removed as mutexes eliminated
uint32_t g_race_condition_count = 0;
volatile bool g_palette_ready = false;

// CRITICAL: Mutex for controlling access to the thread-unsafe USBSerial port.
SemaphoreHandle_t serial_mutex;

// Task handle for audio processing thread
TaskHandle_t audio_task_handle;

// FRAME_CONFIG ODR FIX [2025-09-19 17:00] - Variable instance only (struct defined in globals.h)
struct cached_config frame_config;

// NOTE_COLORS ODR FIX [2025-09-19 17:00] - Moved from constants.h
// CRITICAL: Preserve exact aggregate initialization syntax!
SQ15x16 note_colors[12] = {
  0.0000,
  0.0833,
  0.1666,
  0.2499,
  0.3333,
  0.4166,
  0.4999,
  0.5833,
  0.6666,
  0.7499,
  0.8333,
  0.9166
};

// INCANDESCENT_LOOKUP ODR FIX [2025-09-19 17:15] - Moved from constants.h
// CRITICAL: Preserve exact aggregate initialization syntax!
CRGB16 incandescent_lookup = { 1.0000, 0.4453, 0.1562 };

// LED_LERP ODR FIX [2025-09-19 17:15] - Moved from led_utilities.h
LerpParams* led_lerp_params = NULL;
bool lerp_params_initialized = false;

// PALETTE INTEGRATION [2025-09-20] - Phase 2 minimal state management
// REMOVED: g_current_palette_index - CONFIG.PALETTE_INDEX is now single source of truth
// uint8_t g_current_palette_index = 0;

#define FIRMWARE_VERSION 40101  // Try "V" on the Serial port for this!
//                       MmmPP     M = Major version, m = Minor version, P = Patch version
//                                 (i.e 3.5.4 would be 30504)

// COMMENTED OUT [2025-09-19 15:20] - Duplicate include causes circular dependency
// Lightshow modes moved to lightshow_modes.h to fix ODR violation - included properly at line 58
// #include "lightshow_modes.h"

// Define USBSerial reference for ESP32-S3 (must be before includes that use it)
#if ARDUINO_USB_CDC_ON_BOOT
  #define USBSerial Serial
#else
  HWCDC USBSerial;
#endif

// External dependencies -------------------------------------------------------------
#include <WiFi.h>         // Needed for Station Mode
#include <esp_now.h>      // P2P wireless communication library (p2p.h below)
#include <esp_random.h>   // RNG Functions
#include <FastLED.h>      // Handles LED color data and display
#include <FS.h>           // Filesystem functions (bridge_fs.h below)
#include <LittleFS.h>     // LittleFS implementation
#include <Ticker.h>       // Scheduled tasks library
#include <USB.h>          // USB Connection handling
#include <FirmwareMSC.h>  // Allows firmware updates via USB MSC
#include <FixedPoints.h>
#include <FixedPointsCommon.h>
#include <Wire.h>
#include <esp_task_wdt.h>
#include <esp_pm.h>

// Include Sensory Bridge firmware files, sorted high to low, by boringness ;) -------
#include "sb_strings.h"          // Strings for printing
#include "user_config.h"      // Nothing for now
#include "constants.h"        // Global constants
#include "globals.h"          // Global variables
#include "frame_sync.h"
#include "presets.h"          // Configuration presets by name
#include "bridge_fs.h"        // Filesystem access (save/load configuration)
#include "utilities.h"        // Misc. math and other functions

// Enable performance monitoring for 96-bin testing
#define ENABLE_PERFORMANCE_MONITORING
#ifdef ENABLE_PERFORMANCE_MONITORING
#include "debug/performance_monitor.h"
#endif
#include "debug/debug_manager.h"
#ifndef DEBUG_BUILD
#define DEBUG_BUILD 1
#endif
#include "performance_optimized_trace.h"
#include "i2s_audio.h"        // I2S Microphone audio capture
#include "led_utilities.h"    // LED color/transform utility functions
#include "noise_cal.h"        // Background noise removal
#include "p2p.h"              // Sensory Sync handling
#include "buttons.h"          // Watch the status of buttons
#include "knobs.h"            // Watch the status of knobs...
#include "serial_menu.h"      // Watch the Serial port... *sigh*
// DISABLED FOR TESTING: Checking if AudioGuard is causing issues
// #include "audio_guard.h"      // Audio pipeline protection layer
#include "audio_raw_state.h"  // Phase 2A: Audio state encapsulation (low risk)
#include "audio_processed_state.h"  // Phase 2B: Processed audio state (medium risk)
#include "system.h"           // Watch how fast I can check if settings were updated... yada yada..
// #include "palettes_bridge.h"  // Palette system integration - safe scaffold ready for Phase 2
#include "GDFT.h"             // Conversion to (and post-processing of) frequency data! (hey, something cool!)
#include "lightshow_modes.h"  // --- FINALLY, the FUN STUFF!
#include "dual_coordinator.h" // Dual-strip coordinator (Coupling plans, operators)
#include "debug/palette_debug.h"  // Palette debugging instrumentation
#include "palettes/palette_luts_api.h"  // Names + LUT count for calibrated palettes
#include "hmi/dual_encoder_controller.h"  // Dual encoder controller
#include "test_audio_diagnostics.h"  // Audio diagnostics for troubleshooting

namespace {
constexpr bool kEnableFrameLog = false;
constexpr bool kEnableAudioDebug = false;
constexpr bool kEnableStage1Debug = false;
}

// Define benchmark state variables (declared extern in serial_menu.h)
bool benchmark_running = false;
uint32_t benchmark_start_time = 0;
uint32_t system_fps_sum = 0;
uint32_t led_fps_sum = 0;
uint32_t benchmark_sample_count = 0;

// Add at the top of the file, near other global variables
uint32_t last_frame_us = 0;
HMI::DualEncoderController g_hmi_controller;

// COMMENTED OUT [2025-09-19 16:45] - Multiple definition error
// serial_mutex is now defined in globals.cpp to fix ODR violation
// SemaphoreHandle_t serial_mutex;

// Task handle for main loop on Core 0
TaskHandle_t main_loop_task = NULL;

// Forward declarations
void led_thread(void* arg);
void main_loop_thread(void* arg);
void main_loop_core0();

// Phase 2A: AudioRawState instance - MIGRATION IN PROGRESS
// SAFETY: Audio thread only, no shared access, replaces i2s_samples_raw[] first
SensoryBridge::Audio::AudioRawState audio_raw_state;

// Phase 2B: AudioProcessedState instance - MIGRATION IN PROGRESS
// SAFETY: Audio writes, LED reads - single-core scheduling provides atomicity
SensoryBridge::Audio::AudioProcessedState audio_processed_state;

// Encoder state globals (must be defined exactly once)

// Setup, runs only one time ---------------------------------------------------------
void setup() {
  // CRITICAL: Force Arduino to run on Core 0 ONLY
  if (xPortGetCoreID() != 0) {
    // We can't use USBSerial yet, but this is a critical failure.
    // A hardware UART might be better for this kind of early error.
    vTaskDelay(10);
  }
  
  // POWER MANAGEMENT FIX: Force consistent power management regardless of USB status
  // This prevents performance degradation when switching from USB to battery power
  esp_pm_config_esp32s3_t pm_config = {
    .max_freq_mhz = 240,
    .min_freq_mhz = 240,
    .light_sleep_enable = false
  };
  esp_pm_configure(&pm_config);
  // Task Watchdog: 2-second timeout, panic on trigger
  esp_task_wdt_init(2, true);
  
  init_system();  // (system.h) Initialize all hardware and arrays

  // Initialize organized debug manager
  DebugManager::init();
  // REMOVED [2025-09-20 16:30] - preset_audio_debug() was re-enabling the disabled categories
  // DebugManager::preset_audio_debug();  // Start with audio-focused debugging

  init_performance_trace(TRACE_CAT_LED | TRACE_CAT_PERF | TRACE_CAT_CRITICAL | TRACE_CAT_ERROR);
  set_trace_level(TRACE_LEVEL_DEBUG);
  set_trace_categories(TRACE_CAT_LED | TRACE_CAT_PERF | TRACE_CAT_CRITICAL | TRACE_CAT_ERROR);
  // Single-core mandate: run trace consumer on Core 0 (cooperative, low priority)
  xTaskCreatePinnedToCore(trace_consumer_task, "trace_consumer", 4096, nullptr,
                          tskIDLE_PRIORITY, nullptr, 0);

  // CRITICAL DEBUG: Test if execution continues after init_system()
  // Only print if USB serial is available (external power may not have USB)
  if (USBSerial) {
    USBSerial.println("CRITICAL: EXECUTION CONTINUES AFTER INIT_SYSTEM!");
    USBSerial.flush();
  }
  delay(100);

  // Create the serial mutex before starting any other tasks
  serial_mutex = xSemaphoreCreateMutex();
  if (serial_mutex == NULL) {
    if (USBSerial) {
      USBSerial.println("FATAL: Serial mutex creation FAILED");
    }
    // System cannot continue safely.
    while(1) { delay(1000); }
  }

  if (USBSerial) {
    USBSerial.println("DEBUG: Serial mutex created successfully");
    USBSerial.flush();
  }

  // CRITICAL: Initialize audio guard AFTER system init but BEFORE audio operations
  // DISABLED FOR TESTING: Checking if AudioGuard is causing issues
  // AudioGuard::init();
  // AudioGuard::initAudioSafe();  // Pre-initialize audio buffers
  
#ifdef ENABLE_PERFORMANCE_MONITORING
  init_performance_monitor();
  if (USBSerial) {
    USBSerial.println("Performance monitoring enabled for 96-bin testing");
  }
#endif

  // CRITICAL DEBUG: Testing if code execution continues after #endif
  if (USBSerial) {
    USBSerial.println("CRITICAL: Reached line immediately after #endif");
    USBSerial.flush();
  }
  delay(100);

  if (USBSerial) {
    USBSerial.println("DEBUG: Initializing dual encoder controller...");
    USBSerial.flush();
  }
  bool hmi_ok = g_hmi_controller.begin(true);
  if (!hmi_ok) {
    if (USBSerial) {
      USBSerial.println("WARNING: Dual encoder controller failed to initialize; continuing without physical HMI");
    }
  } else {
    if (USBSerial) {
      USBSerial.println("DEBUG: Dual encoder controller initialized");
    }
  }

  // Prepare dual-strip coordinator now that core state is initialized
  coordinator_init();

  // Single-core rendering: no LED thread. Rendering now runs in Phase C/D of main_loop_core0 on Core 0.
  if (USBSerial) {
    USBSerial.println("DEBUG: Single-core mode enabled; rendering on Core 0 Phase C/D");
    USBSerial.flush();
  }
  
  // CRITICAL: Create main loop task on Core 0 to prevent Core 1 watchdog issues
  if (USBSerial) {
    USBSerial.println("DEBUG: Creating main loop task on Core 0...");
  }
  xTaskCreatePinnedToCore(main_loop_thread, "main_loop", 16384, NULL, tskIDLE_PRIORITY + 2, &main_loop_task, 0);
  if (USBSerial) {
    USBSerial.println("DEBUG: Main loop task created on Core 0!");
  }
}

// Main loop thread that runs on Core 0
void main_loop_thread(void* arg) {
  if (USBSerial) {
    USBSerial.println("DEBUG: Main loop thread started on Core 0!");
    USBSerial.print("Running on Core: ");
    USBSerial.println(xPortGetCoreID());
  }
  
  // Register this task with the watchdog
  esp_task_wdt_add(NULL);  // NULL means current task
  if (USBSerial) {
    USBSerial.println("DEBUG: Task registered with watchdog");
  }
  
  // Run the actual loop code forever
  while (true) {
    main_loop_core0();
  }
}

// Actual loop code moved to separate function
void main_loop_core0() {
  static bool first_loop = true;
  static uint32_t a_time_acc_us = 0;
  static uint32_t b_time_acc_us = 0;
  static uint32_t c_time_acc_us = 0;
  static uint32_t d_time_acc_us = 0;
  static uint32_t cd_frames = 0;
  static uint32_t metrics_win_start_ms = 0;
  static uint32_t metrics_frames = 0;
  static uint32_t wdt_b_feeds = 0, wdt_c_feeds = 0, wdt_d_feeds = 0;
  static uint32_t last_show_arm_us = 0;
  static uint32_t last_wire_us = 0;
  if (first_loop) {
    if (USBSerial) {
      USBSerial.println("DEBUG: Entered main loop!");
      USBSerial.flush();
    }
    first_loop = false;
  }
  uint32_t t_loop_start = micros();
  // No frame rate limiting - target is 120+ FPS
  
  uint32_t t_now_us = micros();        // Timestamp for this loop, used by some core functions
  uint32_t t_now = t_now_us / 1000.0;  // Millisecond version
  
#ifdef ENABLE_PERFORMANCE_MONITORING
  perf_metrics.frame_start_time = t_now_us;
#endif

  // S3 Performance Validation Metrics (printing gated)
  static uint32_t frame_count = 0;
  static uint32_t last_fps_print = 0;
  frame_count++;
#if ENABLE_METRICS_LOGGING
  // Print performance metrics every 15 seconds
  if (t_now - last_fps_print > 15000) {
    xSemaphoreTake(serial_mutex, portMAX_DELAY);
    float actual_fps = frame_count / 5.0;
    if (DebugManager::should_print(DEBUG_PERFORMANCE)) {
      DebugManager::print_s3_performance(actual_fps, g_race_condition_count);
      USBSerial.print("RB reads/miss: ");
      USBSerial.print(g_rb_reads);
      USBSerial.print("/");
      USBSerial.println(g_rb_deadline_miss);
      DebugManager::mark_printed(DEBUG_PERFORMANCE);
    }
    frame_count = 0;
    g_race_condition_count = 0;
    last_fps_print = t_now;
    xSemaphoreGive(serial_mutex);
  }
#endif

  // ---- Phase A: controls/settings/p2p/serial ----
  #if ENABLE_INPUTS_RUNTIME
    function_id = 0;     // These are for debug_function_timing() in system.h to see what functions take up the most time
    check_knobs(t_now);  // (knobs.h)
    // Check if the knobs have changed

    function_id = 1;
    check_buttons(t_now);  // (buttons.h)
    // Check if the buttons have changed
  #endif

  g_hmi_controller.update(t_now);
  
  // AUDIO GUARD: Periodic integrity check
  // DISABLED FOR TESTING: Checking if AudioGuard is causing issues
  // AudioGuard::checkIntegrity(t_now);

  function_id = 2;
  check_settings(t_now);  // (system.h)
  // Check if the settings have changed

  function_id = 3;
  check_serial(t_now);  // (serial_menu.h)
  // Check if UART commands are available
  

  #if ENABLE_P2P_RUNTIME
    function_id = 4;
    run_p2p();  // (p2p.h)
    // Process P2P network packets to synchronize units
  #endif
  uint32_t t_after_A = micros();

  // ---- Phase B: audio capture + analysis ----
  function_id = 5;
#ifdef ENABLE_PERFORMANCE_MONITORING
  PERF_MONITOR_START();
#endif
  acquire_sample_chunk(t_now);  // (i2s_audio.h)
  // Capture a frame of I2S audio (holy crap, FINALLY something about sound)
#ifdef ENABLE_PERFORMANCE_MONITORING
  PERF_MONITOR_END(i2s_read_time);
#endif

  function_id = 6;
  run_sweet_spot();  // (led_utilities.h)
  // Based on the current audio volume, alter the Sweet Spot indicator LEDs

  // Calculates audio loudness (VU) using RMS, adjusting for noise floor based on calibration
  calculate_vu();

  function_id = 7;
  process_GDFT();  // (GDFT.h)
  // Execute GDFT and post-process
  // (If you're wondering about that weird acronym, check out the source file)

  // Watches the rate of change in the Goertzel bins to guide decisions for auto-color shifting
  calculate_novelty(t_now);
  uint32_t t_after_B = micros();
  // WDT feed after Phase B (audio)
  esp_task_wdt_reset();
  wdt_b_feeds++;
  // WDT feed after Phase B (audio)
  esp_task_wdt_reset();
  wdt_b_feeds++;

  // COLOR SHIFT PROCESSING [2025-09-20] - Simplified logic
  // palettes_bridge.h now handles all palette protection internally
  // This just manages whether to update hue_position or keep it static

  if (CONFIG.AUTO_COLOR_SHIFT == true && CONFIG.PALETTE_INDEX == 0) {
    // Only shift hue in HSV mode; keep palettes stable
    process_color_shift();

    #if DEBUG_COLOR_SHIFT_VALUES
    // Debug tracking for color shift values
    debug_color_shift_values(0.0f, 0.001f, 1.0f);
    #endif
  } else {
    // Keep hue static when auto shift disabled
    // For manual control in both HSV and palette modes
    hue_position = 0;
    hue_shifting_mix = -0.35;
  }

  #if DEBUG_PALETTE_INDEX_WALKING
  // Log palette selection changes for on-device debugging
  static uint8_t last_palette_idx = 255;
  if (CONFIG.PALETTE_INDEX != last_palette_idx) {
    last_palette_idx = CONFIG.PALETTE_INDEX;
    const char* name = palette_name_for_index(CONFIG.PALETTE_INDEX);
    USBSerial.printf("[PALETTE_SELECT] index=%u name=%s\n",
                     CONFIG.PALETTE_INDEX,
                     name ? name : "(null)");
  }
  #endif

  #if ENABLE_LOOKAHEAD_SMOOTHING
    function_id = 8;
    lookahead_smoothing();  // (GDFT.h)
    // Peek at upcoming frames to study/prevent flickering
  #endif

  // ---- Phase C: Render (single-core) ----
  function_id = 8;
  g_coupling_plan = coordinator_update(g_router_state,
                                       novelty_curve,
                                       audio_vu_level,
                                       millis());

  uint32_t t_c_start = micros();
  begin_frame();

  // Cache CONFIG values at start of frame
  cache_frame_config();

  if (mode_transition_queued == true || noise_transition_queued == true) {
    run_transition_fade();
  }

  get_smooth_spectrogram();
  make_smooth_chromagram();

  // Render the primary LED strip using Router-selected mode
  frame_config.coordinator_is_secondary = false;
  if (frame_config.coordinator_primary_mode == LIGHT_MODE_GDFT) {
    light_mode_gdft();
  } else if (frame_config.coordinator_primary_mode == LIGHT_MODE_GDFT_CHROMAGRAM) {
    light_mode_chromagram_gradient();
  } else if (frame_config.coordinator_primary_mode == LIGHT_MODE_GDFT_CHROMAGRAM_DOTS) {
    light_mode_chromagram_dots();
  } else if (frame_config.coordinator_primary_mode == LIGHT_MODE_BLOOM) {
    light_mode_bloom(leds_16_prev);
  } else if (frame_config.coordinator_primary_mode == LIGHT_MODE_VU_DOT) {
    light_mode_vu_dot();
  } else if (frame_config.coordinator_primary_mode == LIGHT_MODE_KALEIDOSCOPE) {
    light_mode_kaleidoscope();
  } else if (frame_config.coordinator_primary_mode == LIGHT_MODE_QUANTUM_COLLAPSE) {
    light_mode_quantum_collapse();
  } else if (frame_config.coordinator_primary_mode == LIGHT_MODE_WAVEFORM) {
    // Seed primary LED buffer for trails
    memcpy(leds_16, leds_16_prev, sizeof(CRGB16) * NATIVE_RESOLUTION);
    // Call waveform with primary state buffers/variables
    light_mode_waveform(leds_16_prev, waveform_last_color_primary);
    // Update primary previous buffer for next frame
    memcpy(leds_16_prev, leds_16, sizeof(CRGB16) * NATIVE_RESOLUTION);
  }

  extern volatile uint8_t g_last_effective_prism_primary;
  if (CONFIG.PRISM_COUNT > 0) {
    uint8_t min_prism = CONFIG.PRISM_COUNT;
    if (SECONDARY_PRISM_COUNT < min_prism) min_prism = SECONDARY_PRISM_COUNT;
    uint8_t effective_prism = (g_qos_prism_trim >= min_prism) ? 0 : (min_prism - g_qos_prism_trim);
    apply_prism_effect(effective_prism, 0.25);
    g_last_effective_prism_primary = effective_prism;
  } else {
    g_last_effective_prism_primary = 0;
  }

  if (CONFIG.BULB_OPACITY > 0.00) {
    render_bulb_cover();
  }

  // Secondary channel (no CONFIG mutation) — render via per-frame snapshot
  // Phase 3 parity guard: ensure secondary is always enabled
  if (!ENABLE_SECONDARY_LEDS) {
    ENABLE_SECONDARY_LEDS = true;
    if (USBSerial) USBSerial.println("PHASE3: Enforcing dual-strip parity (C path)");
  }
  if (ENABLE_SECONDARY_LEDS) {
    // Save primary buffer and frame snapshot
    CRGB16 primary_buffer[NATIVE_RESOLUTION];
    memcpy(primary_buffer, leds_16, sizeof(CRGB16) * NATIVE_RESOLUTION);
    struct cached_config saved_fc = frame_config;

    // Build a secondary per-frame view for modes that read frame_config
    frame_config.PHOTONS = SECONDARY_PHOTONS;
    frame_config.CHROMA  = SECONDARY_CHROMA;
    frame_config.MOOD    = SECONDARY_MOOD;
    frame_config.coordinator_is_secondary = true;

    // Seed secondary buffer from previous secondary frame
    memcpy(leds_16, leds_16_prev_secondary, sizeof(CRGB16) * NATIVE_RESOLUTION);

    // Anti-phase handled inside synthesis (no output reversal)
    if (frame_config.coordinator_secondary_mode == LIGHT_MODE_GDFT) {
      light_mode_gdft();
    } else if (frame_config.coordinator_secondary_mode == LIGHT_MODE_GDFT_CHROMAGRAM) {
      light_mode_chromagram_gradient();
    } else if (frame_config.coordinator_secondary_mode == LIGHT_MODE_GDFT_CHROMAGRAM_DOTS) {
      light_mode_chromagram_dots();
    } else if (frame_config.coordinator_secondary_mode == LIGHT_MODE_BLOOM) {
      light_mode_bloom(leds_16_prev_secondary);
    } else if (frame_config.coordinator_secondary_mode == LIGHT_MODE_VU_DOT) {
      light_mode_vu_dot();
    } else if (frame_config.coordinator_secondary_mode == LIGHT_MODE_KALEIDOSCOPE) {
      light_mode_kaleidoscope();
    } else if (frame_config.coordinator_secondary_mode == LIGHT_MODE_QUANTUM_COLLAPSE) {
      light_mode_quantum_collapse();
    } else if (frame_config.coordinator_secondary_mode == LIGHT_MODE_WAVEFORM) {
      light_mode_waveform(leds_16_prev_secondary, waveform_last_color_secondary);
      memcpy(leds_16_prev_secondary, leds_16, sizeof(CRGB16) * NATIVE_RESOLUTION);
    }

    // Apply anti-phase mapping and small temporal offset for secondary inside synthesis stage
    apply_antiphase_if_secondary(leds_16);
    apply_temporal_offset_if_secondary(leds_16, leds_16_prev_secondary);

    extern volatile uint8_t g_last_effective_prism_secondary;
    if (SECONDARY_PRISM_COUNT > 0) {
      uint8_t min_prism = CONFIG.PRISM_COUNT;
      if (SECONDARY_PRISM_COUNT < min_prism) min_prism = SECONDARY_PRISM_COUNT;
      uint8_t effective_prism = (g_qos_prism_trim >= min_prism) ? 0 : (min_prism - g_qos_prism_trim);
      apply_prism_effect(effective_prism, 0.25);
      g_last_effective_prism_secondary = effective_prism;
    } else {
      g_last_effective_prism_secondary = 0;
    }

    // Save secondary pattern
    memcpy(leds_16_secondary, leds_16, sizeof(CRGB16) * NATIVE_RESOLUTION);
    clip_led_values(leds_16_secondary);

    // Restore primary buffer and frame snapshot
    memcpy(leds_16, primary_buffer, sizeof(CRGB16) * NATIVE_RESOLUTION);
    frame_config = saved_fc;
  }

  publish_frame();
  uint32_t t_after_c = micros();
  // WDT feed after Phase C (render)
  esp_task_wdt_reset();
  wdt_c_feeds++;

  // ---- Phase D: Show (LED output) ----
  // Conservative busy fence: avoid arming show faster than last measured wire time
  if (last_wire_us > 0) {
    while ((micros() - last_show_arm_us) < last_wire_us) {
      taskYIELD();
    }
  }
  uint32_t t_before_show = micros();
  show_leds();
  uint32_t t_after_show = micros();
  last_wire_us = (t_after_show - t_before_show);
  last_show_arm_us = t_before_show;
  // WDT feed after Phase D (output)
  esp_task_wdt_reset();
  wdt_d_feeds++;

  // Update LED FPS (EMA)
  LED_FPS = 0.95 * LED_FPS + 0.05 * (1000000.0 / (esp_timer_get_time() - last_frame_us));
  last_frame_us = esp_timer_get_time();

  // ---- Phase E: Upkeep (metrics/logs) ----
  // Incorporate Phase C/D timings into once/4s report below
  uint32_t t_c_us = (t_after_c - t_c_start);
  uint32_t t_d_us = (t_after_show - t_before_show);
  c_time_acc_us += t_c_us;
  d_time_acc_us += t_d_us;
  cd_frames++;
  // Log the audio system FPS
  
  // Run diagnostics if enabled (DISABLED - was killing performance)
  // if (debug_mode) {
  //   diagnose_dc_offset();  // Diagnose DC offset issues
  //   // diagnose_audio_pipeline();  // Full pipeline diagnostics (uncomment if needed)
  // }
  
#ifdef ENABLE_PERFORMANCE_MONITORING
  perf_metrics.total_frame_time = micros() - perf_metrics.frame_start_time;
  update_performance_metrics();
  log_performance_data();
#endif

  // --- BENCHMARK LOGIC --- 
  if (benchmark_running) {
    uint32_t current_time = millis();
    if (current_time - benchmark_start_time < benchmark_duration) {
      // Accumulate FPS data
      system_fps_sum += SYSTEM_FPS; // Assumes SYSTEM_FPS is updated before this point
      led_fps_sum += LED_FPS;       // Assumes LED_FPS is updated before this point
      benchmark_sample_count++;
    } else {
      // Benchmark finished
      benchmark_running = false;
      float avg_system_fps = (benchmark_sample_count > 0) ? (float)system_fps_sum / benchmark_sample_count : 0.0f;
      float avg_led_fps = (benchmark_sample_count > 0) ? (float)led_fps_sum / benchmark_sample_count : 0.0f;
      
      xSemaphoreTake(serial_mutex, portMAX_DELAY);
      tx_begin(); // Use the tx_begin/end functions from serial_menu.h for formatted output
      USBSerial.println("Benchmark Complete!");
      USBSerial.print("  Average System FPS: ");
      USBSerial.println(avg_system_fps, 2);
      USBSerial.print("  Average LED FPS: ");
      USBSerial.println(avg_led_fps, 2);
      USBSerial.print("  Samples collected: ");
      USBSerial.println(benchmark_sample_count);
      tx_end();
      xSemaphoreGive(serial_mutex);

      // Reset sums and count for next run
      system_fps_sum = 0;
      led_fps_sum = 0;
      benchmark_sample_count = 0;
    }
  }
  // --- END BENCHMARK LOGIC ---

  // REMOVED: Useless function timing debug that just prints zeros
  
  // ---- Accumulate per-phase timings and emit once/4s summary ----
  metrics_frames++;
  a_time_acc_us += (t_after_A - t_loop_start);
  b_time_acc_us += (t_after_B - t_after_A);
  if (metrics_win_start_ms == 0) metrics_win_start_ms = t_now;
  if ((t_now - metrics_win_start_ms) >= 4000) { // throttle to ~0.25 Hz
    uint32_t a_avg = (metrics_frames > 0) ? (a_time_acc_us / metrics_frames) : 0;
    uint32_t b_avg = (metrics_frames > 0) ? (b_time_acc_us / metrics_frames) : 0;
    uint32_t c_avg = (cd_frames > 0) ? (c_time_acc_us / cd_frames) : 0;
    uint32_t d_avg = (cd_frames > 0) ? (d_time_acc_us / cd_frames) : 0;

    // Publish A/B averages for consolidated METRICS line
    g_avg_a_us = a_avg;
    g_avg_b_us = b_avg;
    g_wdt_b_feeds_window = wdt_b_feeds;
    g_last_c_avg_us = c_avg;
    g_last_d_avg_us = d_avg;

    #if ENABLE_METRICS_LOGGING
    if (USBSerial) {
      float a_ms = a_avg / 1000.0f;
      float b_ms = b_avg / 1000.0f;
      float c_ms = c_avg / 1000.0f;
      float d_ms = d_avg / 1000.0f;
      float e_ms = 0.0f; // not tracked separately
      float overlap_pct = (c_avg + d_avg) ? (100.0f * float(d_avg) / float(c_avg + d_avg)) : 0.0f;
      uint32_t wdt_total = g_wdt_b_feeds_window + wdt_c_feeds + wdt_d_feeds;
      USBSerial.printf(
        "METRICS fps=%.1f eff_fps=%.1f a_ms=%.2f b_ms=%.2f c_ms=%.2f d_ms=%.2f e_ms=%.2f overlap=%.1f%% rb_miss=%lu rb_over=0 rb_dead=0 wdt_feed=%lu flip_viol=%lu\n",
        SYSTEM_FPS,
        LED_FPS,
        a_ms, b_ms, c_ms, d_ms, e_ms,
        overlap_pct,
        (unsigned long)g_rb_deadline_miss,
        (unsigned long)wdt_total,
        (unsigned long)g_flip_violations);
      if (g_flip_violations > 0) {
        USBSerial.printf("FLIP_VIOL=%lu\n", (unsigned long)g_flip_violations);
        g_flip_violations = 0;
      }
    }
    #endif

    // Reset windows
    a_time_acc_us = b_time_acc_us = 0;
    c_time_acc_us = d_time_acc_us = 0;
    metrics_frames = 0;
    cd_frames = 0;
    wdt_b_feeds = 0;
    wdt_c_feeds = 0;
    wdt_d_feeds = 0;
    metrics_win_start_ms = t_now;
  }

  // Add useful audio debug output every 8 seconds
  static uint32_t last_audio_debug = 0;
  if (kEnableAudioDebug && debug_mode && (t_now - last_audio_debug > 8000)) {
    xSemaphoreTake(serial_mutex, portMAX_DELAY);
    USBSerial.println("=== AUDIO DEBUG ===");
    USBSerial.print("Waveform peak: ");
    USBSerial.println(waveform_peak_scaled);
    USBSerial.print("Max waveform val: ");
    USBSerial.println(max_waveform_val_raw);
    USBSerial.print("Audio VU: ");
    USBSerial.println(float(audio_vu_level));
    USBSerial.print("Silence: ");
    USBSerial.println(silence ? "YES" : "NO");
    USBSerial.print("Sweet spot state: ");
    USBSerial.println(sweet_spot_state);
    USBSerial.print("First 5 waveform samples: ");
    for(int i = 0; i < 5; i++) {
      USBSerial.print(waveform[i]);
      USBSerial.print(" ");
    }
    USBSerial.println();
    USBSerial.println("==================");
    last_audio_debug = t_now;
    xSemaphoreGive(serial_mutex);
  }
  
  // CRITICAL: Handle deferred config saves in a safe context
  // This prevents watchdog timeouts during file operations
  extern void do_config_save();
  do_config_save();
  

  // Feed the watchdog timer
  esp_task_wdt_reset();
  
  // Always yield to prevent watchdog timeouts
  taskYIELD();
}

// Default loop() - just yields to prevent Core 1 execution
void loop() {
  // CRITICAL: This runs on Core 1 by default
  // All actual work is done in main_loop_thread on Core 0
  taskYIELD();
}

// Run the lights in their own thread! -------------------------------------------------------------
#if 0  // LED thread is inactive in single-core build; metrics now emitted from main loop
void led_thread(void* arg) {
  /*
   * METRICS EMISSION CONTEXT (Phase 3):
   * - The system loop on Core 0 computes Phase A/B averages and publishes them to globals:
   *     g_avg_a_us, g_avg_b_us, g_wdt_b_feeds_window.
   * - This LED thread computes Phase C/D averages over a 4s window.
   * - Together we emit a consolidated, machine‑parsable METRICS line here:
   *   METRICS fps=<SYSTEM_FPS> eff_fps=<LED_FPS> a_ms=<Aavg_ms> b_ms=<Bavg_ms>
   *           c_ms=<Cavg_ms> d_ms=<Davg_ms> e_ms=<0>
   *           overlap=<D/(C+D) percent> rb_miss=<g_rb_deadline_miss>
   *           rb_over=0 rb_dead=0 wdt_feed=<B+C+D> flip_viol=<g_flip_violations>
  * - This matches docs/99.reference/metrics-and-logging.md while preserving double‑buffering and async output.
   */
  USBSerial.println("DEBUG: LED thread started!");
  USBSerial.flush();
  static uint32_t c_time_acc_us = 0;
  static uint32_t d_time_acc_us = 0;
  static uint32_t l_frames = 0;
  static uint32_t l_win_start_ms = 0;
  static uint32_t wdt_c_feeds_led = 0;
  static uint32_t wdt_d_feeds_led = 0;

  while (!g_palette_ready) {
    vTaskDelay(1);
  }
  
  while (true) {
    if (led_thread_halt == false) {
      // Phase 3 parity guard: ensure both strips are active at all times
      if (!ENABLE_SECONDARY_LEDS) {
        ENABLE_SECONDARY_LEDS = true;
        if (USBSerial) {
          USBSerial.println("PHASE3: Enforcing dual-strip parity; re-enabled secondary channel");
        }
      }
      uint32_t t_c_start = micros();
      begin_frame();

      // Cache CONFIG values at start of frame
      cache_frame_config();
      
      if (mode_transition_queued == true || noise_transition_queued == true) {
        run_transition_fade();
      }

      get_smooth_spectrogram();
      make_smooth_chromagram();

  // Render the primary LED strip using Router-selected mode
  frame_config.coordinator_is_secondary = false;
  if (frame_config.coordinator_primary_mode == LIGHT_MODE_GDFT) {
    light_mode_gdft();
      } else if (frame_config.coordinator_primary_mode == LIGHT_MODE_GDFT_CHROMAGRAM) {
        light_mode_chromagram_gradient();
      } else if (frame_config.coordinator_primary_mode == LIGHT_MODE_GDFT_CHROMAGRAM_DOTS) {
        light_mode_chromagram_dots();
      } else if (frame_config.coordinator_primary_mode == LIGHT_MODE_BLOOM) {
        light_mode_bloom(leds_16_prev);
      } else if (frame_config.coordinator_primary_mode == LIGHT_MODE_VU_DOT) {
        light_mode_vu_dot();
      } else if (frame_config.coordinator_primary_mode == LIGHT_MODE_KALEIDOSCOPE) {
        light_mode_kaleidoscope();
      } else if (frame_config.coordinator_primary_mode == LIGHT_MODE_QUANTUM_COLLAPSE) {
        light_mode_quantum_collapse();
      } else if (frame_config.coordinator_primary_mode == LIGHT_MODE_WAVEFORM) {
        // Seed primary LED buffer for trails
        memcpy(leds_16, leds_16_prev, sizeof(CRGB16) * NATIVE_RESOLUTION);
        // Call waveform with primary state buffers/variables
        light_mode_waveform(leds_16_prev, waveform_last_color_primary);
        // Update primary previous buffer for next frame
        memcpy(leds_16_prev, leds_16, sizeof(CRGB16) * NATIVE_RESOLUTION);

        // STAGE 1 DEBUG: Sample immediately after lightshow mode writes leds_16[]
        static uint16_t stage_burst_frames = 0; // set >0 to re-enable burst logging
        bool in_burst_mode = (stage_burst_frames > 0);
        if (kEnableStage1Debug && (in_burst_mode || DebugManager::should_print(DEBUG_COLOR))) {
          auto to_u8 = [](const SQ15x16& v) -> uint8_t {
            float f = float(v);
            if (f < 0.0f) f = 0.0f;
            if (f > 1.0f) f = 1.0f;
            return uint8_t(f * 255.0f + 0.5f);
          };
          auto to_float = [](const SQ15x16& v) -> double {
            return static_cast<double>(float(v));
          };
          uint16_t i0 = 0;
          uint16_t i1 = (NATIVE_RESOLUTION > 64) ? (NATIVE_RESOLUTION / 2) : 0;
          uint16_t i2 = (NATIVE_RESOLUTION > 1) ? (NATIVE_RESOLUTION - 1) : 0;
          USBSerial.printf("[STAGE-1-PRE-BRIGHT] Frame:%u 16bit f/m/l: "
                           "(%.3f,%.3f,%.3f) (%.3f,%.3f,%.3f) (%.3f,%.3f,%.3f) | u8 f/m/l: "
                           "(%u,%u,%u) (%u,%u,%u) (%u,%u,%u)\n",
                           perf_metrics.frame_count,
                           to_float(leds_16[i0].r), to_float(leds_16[i0].g), to_float(leds_16[i0].b),
                           to_float(leds_16[i1].r), to_float(leds_16[i1].g), to_float(leds_16[i1].b),
                           to_float(leds_16[i2].r), to_float(leds_16[i2].g), to_float(leds_16[i2].b),
                           to_u8(leds_16[i0].r), to_u8(leds_16[i0].g), to_u8(leds_16[i0].b),
                           to_u8(leds_16[i1].r), to_u8(leds_16[i1].g), to_u8(leds_16[i1].b),
                           to_u8(leds_16[i2].r), to_u8(leds_16[i2].g), to_u8(leds_16[i2].b));
          if (!in_burst_mode) DebugManager::mark_printed(DEBUG_COLOR);
        }
        TRACE_EVENT(TRACE_CAT_LED, LED_CALC_START,
                    pack_stage_state(1, leds_any_nonzero(leds_16, NATIVE_RESOLUTION)));
        if (kEnableStage1Debug && in_burst_mode) {
          stage_burst_frames--;
        }

        static uint32_t last_palette_log_s = 0;
        uint32_t palette_log_s = millis() / 1000;
        if (kEnableFrameLog && debug_mode && palette_log_s != last_palette_log_s) {
          last_palette_log_s = palette_log_s;
          USBSerial.printf("[FRAME] palette_ptr=%p size=%u leds_16=%p\n",
                           frame_config.palette_ptr,
                           static_cast<unsigned>(frame_config.palette_size),
                           leds_16);
        }
      }

      publish_frame();
      uint32_t t_after_c = micros();
      // WDT feed after Phase C (render)
      esp_task_wdt_reset();
      wdt_c_feeds_led++;

      uint32_t t_before_show = micros();
      show_leds();
      uint32_t t_after_show = micros();
      // WDT feed after Phase D (output)
      esp_task_wdt_reset();
      wdt_d_feeds_led++;
      // Accumulate C/D timings and print once/sec
      l_frames++;
      c_time_acc_us += (t_after_c - t_c_start);
      d_time_acc_us += (t_after_show - t_before_show);
      uint32_t now_ms = millis();
      if (l_win_start_ms == 0) l_win_start_ms = now_ms;
      if ((now_ms - l_win_start_ms) >= 4000) { // throttle to ~0.25 Hz
        uint32_t c_avg = (l_frames > 0) ? (c_time_acc_us / l_frames) : 0;
        uint32_t d_avg = (l_frames > 0) ? (d_time_acc_us / l_frames) : 0;
        float overlap_pct = (c_avg + d_avg) ? (100.0f * float(d_avg) / float(c_avg + d_avg)) : 0.0f;
        g_last_c_avg_us = c_avg;
        g_last_d_avg_us = d_avg;
        // Emit consolidated summary in the documented format
        float a_ms = g_avg_a_us / 1000.0f;
        float b_ms = g_avg_b_us / 1000.0f;
        float c_ms = c_avg / 1000.0f;
        float d_ms = d_avg / 1000.0f;
        // Phase E not tracked separately in this build
        float e_ms = 0.0f;
        uint32_t wdt_total = g_wdt_b_feeds_window + wdt_c_feeds_led + wdt_d_feeds_led;
        USBSerial.printf(
          "METRICS fps=%.1f eff_fps=%.1f a_ms=%.2f b_ms=%.2f c_ms=%.2f d_ms=%.2f e_ms=%.2f overlap=%.1f%% rb_miss=%lu rb_over=0 rb_dead=0 wdt_feed=%lu flip_viol=%lu\n",
          SYSTEM_FPS,
          LED_FPS,
          a_ms, b_ms, c_ms, d_ms, e_ms,
          overlap_pct,
          (unsigned long)g_rb_deadline_miss,
          (unsigned long)wdt_total,
          (unsigned long)g_flip_violations);
        if (g_flip_violations > 0) {
          USBSerial.printf("FLIP_VIOL=%lu\n", (unsigned long)g_flip_violations);
          g_flip_violations = 0;
        }
        // QoS degrade controller: adjust gently based on Phase C time
        // Target 120 FPS budget (~8ms total). Keep C under ~6ms with headroom.
        const uint32_t C_HIGH_US   = g_qos_c_high_us;     // high watermark
        const uint32_t C_LOW_US    = g_qos_c_low_us;      // low watermark (relax trims)
#if ENABLE_QOS_HYSTERESIS
        const uint32_t c_high_trigger = C_HIGH_US;
        const uint32_t c_low_trigger  = (C_LOW_US > QOS_HYSTERESIS_MARGIN_US)
                                          ? (C_LOW_US - QOS_HYSTERESIS_MARGIN_US)
                                          : 0;
        if (c_avg > c_high_trigger && g_qos_level < 3) {
          g_qos_level++;
        } else if (c_avg < c_low_trigger && g_qos_level > 0) {
          g_qos_level--;
        }
#else
        if (c_avg > C_HIGH_US && g_qos_level < 3) {
          g_qos_level++;
        } else if (c_avg < C_LOW_US && g_qos_level > 0) {
          g_qos_level--;
        }
#endif
#if ENABLE_QOS_LED_FPS_GUARD
        if (LED_FPS < 100.0f && g_qos_level < 3) {
          g_qos_level++;
          USBSerial.printf("QOS guard: LED_FPS=%.1f -> level=%u\n", LED_FPS, (unsigned)g_qos_level);
        }
#endif
        // Map level to prism trimming (uniform across both channels)
        // Level 0: 0 trim, 1:1, 2:2, 3:3
        g_qos_prism_trim = g_qos_level;
        // Optional uniform brightness scaling (OFF by default)
        if (g_qos_brightness_degrade_enabled) {
          float target_scale = 1.0f - 0.05f * float(g_qos_level); // -5% per level
          // Smooth toward target to avoid visual thrash
          float current = float(g_qos_brightness_scale);
          current = current * 0.8f + target_scale * 0.2f;
          if (current < 0.6f) current = 0.6f;
          g_qos_brightness_scale = SQ15x16(current);
        } else {
          g_qos_brightness_scale = SQ15x16(1.0);
        }
        USBSerial.printf("QOS level=%u prism_trim=%u bright=%.2f (Cavg=%luus)\n",
                         (unsigned)g_qos_level,
                         (unsigned)g_qos_prism_trim,
                         float(g_qos_brightness_scale),
                         (unsigned long)c_avg);
        // Reset windows
        c_time_acc_us = d_time_acc_us = 0;
        l_frames = 0;
        l_win_start_ms = now_ms;
        wdt_c_feeds_led = wdt_d_feeds_led = 0;
      }
      
      LED_FPS = 0.95 * LED_FPS + 0.05 * (1000000.0 / (esp_timer_get_time() - last_frame_us));
      last_frame_us = esp_timer_get_time();
    }
    // Avoid artificial 1ms sleeps; yield cooperatively instead
    taskYIELD();
  }
}
#endif

// // Add this new function to update encoder LEDs - REMOVED
// void update_encoder_leds() {
//   // ... function body ...
// }

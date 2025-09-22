// debug_manager.cpp - Implementation of centralized debug output management

#include "debug_manager.h"
#include "../globals.h"

// Define USBSerial for ESP32-S3
#if ARDUINO_USB_CDC_ON_BOOT
  #define USBSerial Serial
#else
  extern HWCDC USBSerial;
#endif

// Static member definitions
uint32_t DebugManager::last_print_time[DEBUG_CATEGORY_COUNT];
uint32_t DebugManager::print_intervals[DEBUG_CATEGORY_COUNT];
bool DebugManager::category_enabled[DEBUG_CATEGORY_COUNT];
uint8_t DebugManager::current_cycle_category = 0;
uint32_t DebugManager::last_cycle_time = 0;

void DebugManager::init() {
    // Initialize timing arrays
    for (int i = 0; i < DEBUG_CATEGORY_COUNT; i++) {
        last_print_time[i] = 0;
        category_enabled[i] = true;
    }

    // DISABLE the noisy categories that flood the monitor
    category_enabled[DEBUG_AUDIO] = false;     // DISABLED: Audio VU spam
    category_enabled[DEBUG_COLOR] = false;     // DISABLED: Color pipeline spam - using burst mode instead
    category_enabled[DEBUG_FREQUENCY] = false; // DISABLED: Frequency spectrum spam

    // Set intervals for ENABLED categories only
    print_intervals[DEBUG_ENCODER] = 100;      // Encoder events: MUST BE RESPONSIVE (100ms)
    print_intervals[DEBUG_PERFORMANCE] = 2400; // Performance: 2.4 second interval
    print_intervals[DEBUG_SYSTEM] = 3000;      // System state: 3 second interval

    // Disabled categories still need intervals (in case re-enabled later)
    print_intervals[DEBUG_AUDIO] = 3000;       // Audio VU: DISABLED
    print_intervals[DEBUG_COLOR] = 300;        // Color pipeline: SHORTENED for burst analysis
    print_intervals[DEBUG_FREQUENCY] = 5500;   // Frequency analysis: DISABLED

    current_cycle_category = 0;
    last_cycle_time = millis();
}

bool DebugManager::should_print(DebugCategory category) {
    if (!debug_mode) return false;
    if (!category_enabled[category]) return false;

    uint32_t now = millis();
    return (now - last_print_time[category]) >= print_intervals[category];
}

void DebugManager::mark_printed(DebugCategory category) {
    last_print_time[category] = millis();
}

void DebugManager::enable_category(DebugCategory category, bool enabled) {
    if (category < DEBUG_CATEGORY_COUNT) {
        category_enabled[category] = enabled;
    }
}

void DebugManager::set_interval(DebugCategory category, uint32_t interval_ms) {
    if (category < DEBUG_CATEGORY_COUNT) {
        print_intervals[category] = interval_ms;
    }
}

void DebugManager::request_once(DebugCategory category) {
    if (category < DEBUG_CATEGORY_COUNT) {
        // Ensure the category is enabled so the request has an effect
        category_enabled[category] = true;
        // Force the next should_print() call to pass by rewinding the timer
        last_print_time[category] = 0;
    }
}

void DebugManager::print_encoder(uint8_t channel, int32_t value, const char* name, float new_value) {
    USBSerial.printf("%s[ENC E%d]%s Raw:%d | %s:%.3f%s\n",
                    DebugColors::ENCODER, channel, DebugColors::DIM,
                    value, name, new_value, DebugColors::RESET);
}

void DebugManager::print_encoder_button(uint8_t channel, const char* action, const char* result) {
    USBSerial.printf("%s[ENC E%d BTN]%s %s | %s%s\n",
                    DebugColors::ENCODER, channel, DebugColors::DIM,
                    action, result, DebugColors::RESET);
}

void DebugManager::print_audio_vu(float raw_rms, float floor, float after_sub) {
    USBSerial.printf("%s[AUDIO VU]%s RMS:%.3f Floor:%.3f After:%.3f%s\n",
                    DebugColors::AUDIO, DebugColors::DIM,
                    raw_rms, floor, after_sub, DebugColors::RESET);
}

void DebugManager::print_audio_state(float sweet_spot, float max_waveform, float silence_thresh, bool is_silent) {
    USBSerial.printf("%s[AUDIO STATE]%s Sweet:%.2f Max:%.0f Thresh:%.0f Silent:%s%s\n",
                    DebugColors::SYSTEM, DebugColors::DIM,
                    sweet_spot, max_waveform, silence_thresh,
                    is_silent ? "YES" : "NO", DebugColors::RESET);
}

void DebugManager::print_performance(float fps, uint32_t gdft_us, uint32_t heap, float cpu_pct,
                                   int bins, int active, float peak_hz) {
    USBSerial.printf("%s[PERF]%s FPS:%.1f GDFT:%luus HEAP:%lu CPU:%.1f%% BINS:%d/%d PEAK:%.0fHz%s\n",
                    DebugColors::PERF, DebugColors::DIM,
                    fps, gdft_us, heap, cpu_pct, active, bins, peak_hz, DebugColors::RESET);
}

void DebugManager::print_s3_performance(float fps, uint32_t race_conditions) {
    USBSerial.printf("%s[S3 PERF]%s FPS:%.2f Race:%lu Target:120+%s\n",
                    DebugColors::PERF, DebugColors::DIM,
                    fps, race_conditions, DebugColors::RESET);
}

void DebugManager::print_color_shift(float novelty, float hue_pos, float speed) {
    // RESTORED [2025-09-20 16:15] - User wants colored debug output maintained
    // Frequency controlled by timing intervals (2.6s) to prevent spam
    USBSerial.printf("%s[COLOR]%s Novelty:%.3f Hue:%.3f Speed:%.3f%s\n",
                    DebugColors::COLOR, DebugColors::DIM,
                    novelty, hue_pos, speed, DebugColors::RESET);
}

void DebugManager::print_frequency_spectrum(int peak_bin, float peak_freq, float peak_mag,
                                          int high_bin, float high_freq, float high_mag) {
    // RESTORED [2025-09-20 16:15] - User wants colored debug output maintained
    // Frequency controlled by timing intervals (2.8s) to prevent spam
    USBSerial.printf("%s[FREQ SPEC]%s Peak=%d(%.0fHz,mag=%.1f) | High=%d(%.0fHz,mag=%.1f)%s\n",
                    DebugColors::FREQ, DebugColors::DIM,
                    peak_bin, peak_freq, peak_mag, high_bin, high_freq, high_mag,
                    DebugColors::RESET);
}

void DebugManager::print_frequency_distribution(int low_active, float low_sum,
                                              int mid_active, float mid_sum,
                                              int high_active, float high_sum) {
    USBSerial.printf("%s[FREQ DIST]%s Low[%d,%.1f] Mid[%d,%.1f] High[%d,%.1f]%s\n",
                    DebugColors::FREQ, DebugColors::DIM,
                    low_active, low_sum, mid_active, mid_sum, high_active, high_sum,
                    DebugColors::RESET);
}

DebugCategory DebugManager::get_current_cycle_category() {
    return static_cast<DebugCategory>(current_cycle_category);
}

void DebugManager::advance_cycle() {
    uint32_t now = millis();
    if (now - last_cycle_time >= 200) { // Advance cycle every 200ms
        current_cycle_category = (current_cycle_category + 1) % DEBUG_CATEGORY_COUNT;
        last_cycle_time = now;
    }
}

void DebugManager::enable_all(bool enabled) {
    for (int i = 0; i < DEBUG_CATEGORY_COUNT; i++) {
        category_enabled[i] = enabled;
    }
}

void DebugManager::preset_minimal() {
    enable_all(false);
    enable_category(DEBUG_ENCODER, true);
    enable_category(DEBUG_SYSTEM, true);
    set_interval(DEBUG_ENCODER, 200);
    set_interval(DEBUG_SYSTEM, 3000);
}

void DebugManager::preset_performance() {
    enable_all(false);
    enable_category(DEBUG_PERFORMANCE, true);
    enable_category(DEBUG_AUDIO, true);
    set_interval(DEBUG_PERFORMANCE, 1000);
    set_interval(DEBUG_AUDIO, 1000);
}

void DebugManager::preset_audio_debug() {
    enable_all(false);
    enable_category(DEBUG_AUDIO, true);
    enable_category(DEBUG_FREQUENCY, true);
    enable_category(DEBUG_COLOR, true);
    enable_category(DEBUG_SYSTEM, true);
    set_interval(DEBUG_AUDIO, 500);
    set_interval(DEBUG_FREQUENCY, 1000);
    set_interval(DEBUG_COLOR, 800);
    set_interval(DEBUG_SYSTEM, 2000);
}

void DebugManager::preset_full_debug() {
    enable_all(true);
    // Keep default intervals
    init();
}

#pragma once
#include <Arduino.h>
#include <FastLED.h>
#include "globals.h"
#include "palettes/palettes_bridge.h"

// ===== COMPREHENSIVE LED COLOR DEBUGGING SYSTEM =====
// Integrates with existing waveform enhancement debug framework
// Provides detailed real-time color/brightness analysis

#define LED_COLOR_DEBUG_ENABLED 1

#if LED_COLOR_DEBUG_ENABLED

// Debug output control
static bool led_debug_verbose = false;
static bool led_debug_palette_tracking = true;
static bool led_debug_corruption_detection = true;
static uint32_t led_debug_sample_interval = 10; // Debug every N frames

// Color tracking statistics
struct LEDColorStats {
    uint32_t total_samples;
    uint32_t corruption_events;
    uint32_t palette_transitions;
    float avg_brightness;
    float max_brightness;
    float min_brightness;
    uint32_t zero_brightness_frames;
    uint32_t oversaturated_frames;
    uint32_t last_frame_number;
};

static LEDColorStats led_color_stats = {0};

// Enhanced color analysis with audio correlation
struct ColorDebugSample {
    CRGB16 color;
    float brightness;
    float hue_approximation;
    float saturation_approximation;
    bool palette_mode;
    uint8_t palette_index;
    float audio_energy;
    uint32_t frame_number;
    uint32_t timestamp_ms;
};

// ANSI colors for debug output
static const char* DEBUG_CYAN = "\033[36m";
static const char* DEBUG_GREEN = "\033[32m";
static const char* DEBUG_YELLOW = "\033[33m";
static const char* DEBUG_RED = "\033[31m";
static const char* DEBUG_BLUE = "\033[34m";
static const char* DEBUG_RESET = "\033[0m";

// Initialize LED color debugging
inline void init_led_color_debug() {
    memset(&led_color_stats, 0, sizeof(led_color_stats));
    USBSerial.printf("%s[LED_COLOR_DEBUG] Initialized - Interval: every %lu frames%s\n",
                     DEBUG_GREEN, led_debug_sample_interval, DEBUG_RESET);
}

// Calculate approximate HSV values from CRGB16 (fast approximation)
inline void calculate_hsv_approximation(const CRGB16& color, float* hue, float* saturation, float* value) {
    float r = float(color.r);
    float g = float(color.g);
    float b = float(color.b);

    float max_val = max(max(r, g), b);
    float min_val = min(min(r, g), b);
    float diff = max_val - min_val;

    *value = max_val;

    if (max_val == 0.0f) {
        *saturation = 0.0f;
        *hue = 0.0f;
        return;
    }

    *saturation = diff / max_val;

    if (diff == 0.0f) {
        *hue = 0.0f;
    } else if (max_val == r) {
        *hue = 60.0f * fmod((g - b) / diff, 6.0f);
    } else if (max_val == g) {
        *hue = 60.0f * ((b - r) / diff + 2.0f);
    } else {
        *hue = 60.0f * ((r - g) / diff + 4.0f);
    }

    if (*hue < 0.0f) *hue += 360.0f;
}

// Detect color corruption patterns
inline bool detect_color_corruption(const CRGB16& color, uint32_t frame_num) {
    bool corruption_detected = false;

    // Range validation
    if (float(color.r) > 1.0f || float(color.g) > 1.0f || float(color.b) > 1.0f) {
        USBSerial.printf("%s[CORRUPTION] Frame:%lu RGB overflow: (%.3f,%.3f,%.3f)%s\n",
                         DEBUG_RED, frame_num, float(color.r), float(color.g), float(color.b), DEBUG_RESET);
        corruption_detected = true;
    }

    // Negative values (should be impossible with SQ15x16 but check anyway)
    if (float(color.r) < 0.0f || float(color.g) < 0.0f || float(color.b) < 0.0f) {
        USBSerial.printf("%s[CORRUPTION] Frame:%lu RGB underflow: (%.3f,%.3f,%.3f)%s\n",
                         DEBUG_RED, frame_num, float(color.r), float(color.g), float(color.b), DEBUG_RESET);
        corruption_detected = true;
    }

    // NaN detection (floating point corruption)
    if (isnan(float(color.r)) || isnan(float(color.g)) || isnan(float(color.b))) {
        USBSerial.printf("%s[CORRUPTION] Frame:%lu RGB NaN detected: (%.3f,%.3f,%.3f)%s\n",
                         DEBUG_RED, frame_num, float(color.r), float(color.g), float(color.b), DEBUG_RESET);
        corruption_detected = true;
    }

    if (corruption_detected) {
        led_color_stats.corruption_events++;
    }

    return corruption_detected;
}

// Comprehensive color analysis with audio correlation
inline void analyze_led_color_detailed(const CRGB16& color, uint32_t frame_num, float audio_energy = 0.0f) {
    static uint32_t debug_frame_counter = 0;
    static uint8_t last_palette_index = 255; // Invalid value to trigger initial detection

    debug_frame_counter++;

    // Skip frames based on sample interval
    if (debug_frame_counter % led_debug_sample_interval != 0) {
        return;
    }

    // Update statistics
    led_color_stats.total_samples++;
    led_color_stats.last_frame_number = frame_num;

    // Calculate brightness and HSV approximations
    float brightness = (float(color.r) + float(color.g) + float(color.b)) / 3.0f;
    float hue, saturation, value;
    calculate_hsv_approximation(color, &hue, &saturation, &value);

    // Update brightness statistics
    if (led_color_stats.total_samples == 1) {
        led_color_stats.min_brightness = brightness;
        led_color_stats.max_brightness = brightness;
        led_color_stats.avg_brightness = brightness;
    } else {
        led_color_stats.min_brightness = min(led_color_stats.min_brightness, brightness);
        led_color_stats.max_brightness = max(led_color_stats.max_brightness, brightness);
        led_color_stats.avg_brightness = (led_color_stats.avg_brightness * (led_color_stats.total_samples - 1) + brightness) / led_color_stats.total_samples;
    }

    // Track special conditions
    if (brightness == 0.0f) {
        led_color_stats.zero_brightness_frames++;
    }

    if (saturation > 0.95f && brightness > 0.1f) {
        led_color_stats.oversaturated_frames++;
    }

    // Detect corruption
    if (led_debug_corruption_detection) {
        detect_color_corruption(color, frame_num);
    }

    // Palette tracking
    const uint8_t palette_total = palette_lut_count();
    const uint8_t current_palette_index = CONFIG.PALETTE_INDEX;
    const bool palette_mode = (current_palette_index > 0) && (current_palette_index < palette_total);

    if (led_debug_palette_tracking && current_palette_index != last_palette_index) {
        const char* palette_name = palette_name_for_index(current_palette_index);
        USBSerial.printf("%s[PALETTE] Frame:%lu Transition: %d → %d (%s)%s\n",
                         DEBUG_BLUE, frame_num, last_palette_index, current_palette_index, palette_name, DEBUG_RESET);
        led_color_stats.palette_transitions++;
        last_palette_index = current_palette_index;
    }

    // Verbose color output
    if (led_debug_verbose) {
        const char* mode_str = palette_mode ? palette_name_for_index(current_palette_index) : "HSV";

        USBSerial.printf("%s[COLOR] Frame:%lu RGB:(%.3f,%.3f,%.3f) Bright:%.3f Sat:%.3f Hue:%.1f° Mode:%s",
                         DEBUG_CYAN, frame_num, float(color.r), float(color.g), float(color.b),
                         brightness, saturation, hue, mode_str);

        if (audio_energy > 0.0f) {
            USBSerial.printf(" Audio:%.3f", audio_energy);
        }

        USBSerial.printf("%s\n", DEBUG_RESET);
    }
}

// Quick color debug (minimal output)
inline void debug_led_color_quick(const CRGB16& color, uint32_t frame_num) {
    static uint32_t quick_counter = 0;
    quick_counter++;

    // Output every 60 frames (roughly 1 second at 60 FPS)
    if (quick_counter % 60 == 0) {
        float brightness = (float(color.r) + float(color.g) + float(color.b)) / 3.0f;
        const uint8_t palette_total = palette_lut_count();
        const uint8_t current_palette_index = CONFIG.PALETTE_INDEX;
        const bool palette_mode = (current_palette_index > 0) && (current_palette_index < palette_total);
        const char* mode_str = palette_mode ? palette_name_for_index(current_palette_index) : "HSV";

        USBSerial.printf("%sRGB:(%.3f,%.3f,%.3f) Bright:%.3f Mode:%s%s\n",
                         DEBUG_CYAN, float(color.r), float(color.g), float(color.b),
                         brightness, mode_str, DEBUG_RESET);
    }
}

// Print comprehensive color debugging statistics
inline void print_led_color_stats() {
    USBSerial.printf("\n%s=== LED COLOR DEBUG STATISTICS ===%s\n", DEBUG_YELLOW, DEBUG_RESET);
    USBSerial.printf("Total Samples: %lu\n", led_color_stats.total_samples);
    USBSerial.printf("Last Frame: %lu\n", led_color_stats.last_frame_number);
    USBSerial.printf("Corruption Events: %lu (%.2f%%)\n",
                     led_color_stats.corruption_events,
                     led_color_stats.total_samples > 0 ?
                     (float(led_color_stats.corruption_events) / led_color_stats.total_samples) * 100.0f : 0.0f);
    USBSerial.printf("Palette Transitions: %lu\n", led_color_stats.palette_transitions);
    USBSerial.printf("Brightness - Avg:%.3f Min:%.3f Max:%.3f\n",
                     led_color_stats.avg_brightness, led_color_stats.min_brightness, led_color_stats.max_brightness);
    USBSerial.printf("Zero Brightness Frames: %lu (%.2f%%)\n",
                     led_color_stats.zero_brightness_frames,
                     led_color_stats.total_samples > 0 ?
                     (float(led_color_stats.zero_brightness_frames) / led_color_stats.total_samples) * 100.0f : 0.0f);
    USBSerial.printf("Oversaturated Frames: %lu (%.2f%%)\n",
                     led_color_stats.oversaturated_frames,
                     led_color_stats.total_samples > 0 ?
                     (float(led_color_stats.oversaturated_frames) / led_color_stats.total_samples) * 100.0f : 0.0f);
    USBSerial.printf("Current Palette: %s (%d)\n", get_current_palette_name(), CONFIG.PALETTE_INDEX);
    USBSerial.printf("%s================================%s\n", DEBUG_YELLOW, DEBUG_RESET);
}

// Reset statistics
inline void reset_led_color_stats() {
    memset(&led_color_stats, 0, sizeof(led_color_stats));
    USBSerial.printf("%s[LED_COLOR_DEBUG] Statistics reset%s\n", DEBUG_GREEN, DEBUG_RESET);
}

// Debug control functions
inline void set_led_debug_verbose(bool enabled) {
    led_debug_verbose = enabled;
    USBSerial.printf("%s[LED_COLOR_DEBUG] Verbose mode: %s%s\n",
                     DEBUG_GREEN, enabled ? "ON" : "OFF", DEBUG_RESET);
}

inline void set_led_debug_interval(uint32_t interval) {
    led_debug_sample_interval = interval;
    USBSerial.printf("%s[LED_COLOR_DEBUG] Sample interval: every %lu frames%s\n",
                     DEBUG_GREEN, interval, DEBUG_RESET);
}

inline void toggle_led_debug_palette_tracking() {
    led_debug_palette_tracking = !led_debug_palette_tracking;
    USBSerial.printf("%s[LED_COLOR_DEBUG] Palette tracking: %s%s\n",
                     DEBUG_GREEN, led_debug_palette_tracking ? "ON" : "OFF", DEBUG_RESET);
}

// Integration point for lightshow modes
// Call this from your main LED rendering functions
inline void debug_current_led_state(const CRGB16* led_buffer, uint16_t led_count, uint32_t frame_num) {
    if (!led_buffer || led_count == 0) return;

    // Analyze representative LEDs (first, middle, last)
    if (led_count > 0) {
        analyze_led_color_detailed(led_buffer[0], frame_num);
    }

    if (led_count > 64) {
        analyze_led_color_detailed(led_buffer[led_count / 2], frame_num);
    }

    if (led_count > 1) {
        analyze_led_color_detailed(led_buffer[led_count - 1], frame_num);
    }
}

#else // LED_COLOR_DEBUG_ENABLED disabled

// Dummy implementations when debug is disabled
inline void init_led_color_debug() {}
inline void analyze_led_color_detailed(const CRGB16& color, uint32_t frame_num, float audio_energy = 0.0f) {}
inline void debug_led_color_quick(const CRGB16& color, uint32_t frame_num) {}
inline void print_led_color_stats() {}
inline void reset_led_color_stats() {}
inline void set_led_debug_verbose(bool enabled) {}
inline void set_led_debug_interval(uint32_t interval) {}
inline void toggle_led_debug_palette_tracking() {}
inline void debug_current_led_state(const CRGB16* led_buffer, uint16_t led_count, uint32_t frame_num) {}

#endif // LED_COLOR_DEBUG_ENABLED

// debug_manager.h - Centralized debug output management with organized categories and timing
// Eliminates debug spam by organizing output into logical groups with staggered display timing

#ifndef DEBUG_MANAGER_H
#define DEBUG_MANAGER_H

#include <Arduino.h>
#include <stdint.h>

// Debug category definitions
enum DebugCategory {
    DEBUG_ENCODER = 0,      // Encoder rotations and button presses
    DEBUG_AUDIO,            // VU levels, RMS, frequency analysis
    DEBUG_PERFORMANCE,      // FPS, timing, memory usage
    DEBUG_COLOR,            // Color shifting, palette changes
    DEBUG_FREQUENCY,        // Frequency spectrum and distribution
    DEBUG_SYSTEM,           // System state, sweet spot, silence detection
    DEBUG_CATEGORY_COUNT    // Total number of categories
};

// Color codes for organized output
struct DebugColors {
    static constexpr const char* HEADER = "\033[1;36m";    // Bold cyan for headers
    static constexpr const char* ENCODER = "\033[36m";     // Cyan for encoders
    static constexpr const char* AUDIO = "\033[33m";       // Yellow for audio
    static constexpr const char* PERF = "\033[32m";        // Green for performance
    static constexpr const char* COLOR = "\033[35m";       // Magenta for color
    static constexpr const char* FREQ = "\033[34m";        // Blue for frequency
    static constexpr const char* SYSTEM = "\033[37m";      // White for system
    static constexpr const char* RESET = "\033[0m";        // Reset color
    static constexpr const char* DIM = "\033[2m";          // Dim text
};

class DebugManager {
private:
    static uint32_t last_print_time[DEBUG_CATEGORY_COUNT];
    static uint32_t print_intervals[DEBUG_CATEGORY_COUNT];
    static bool category_enabled[DEBUG_CATEGORY_COUNT];
    static uint8_t current_cycle_category;
    static uint32_t last_cycle_time;

public:
    // Initialize debug manager with default intervals
    static void init();

    // Check if a category should print now (based on timing)
    static bool should_print(DebugCategory category);

    // Mark that a category just printed (updates timing)
    static void mark_printed(DebugCategory category);

    // Enable/disable specific debug categories
    static void enable_category(DebugCategory category, bool enabled = true);
    static void disable_category(DebugCategory category) { enable_category(category, false); }

    // Set print interval for a category (in milliseconds)
    static void set_interval(DebugCategory category, uint32_t interval_ms);
    static void request_once(DebugCategory category);

    // Organized print functions for each category
    static void print_encoder(uint8_t channel, int32_t value, const char* name, float new_value);
    static void print_encoder_button(uint8_t channel, const char* action, const char* result);

    static void print_audio_vu(float raw_rms, float floor, float after_sub);
    static void print_audio_state(float sweet_spot, float max_waveform, float silence_thresh, bool is_silent);

    static void print_performance(float fps, uint32_t gdft_us, uint32_t heap, float cpu_pct,
                                 int bins, int active, float peak_hz);
    static void print_s3_performance(float fps, uint32_t race_conditions);

    static void print_color_shift(float novelty, float hue_pos, float speed);

    static void print_frequency_spectrum(int peak_bin, float peak_freq, float peak_mag,
                                        int high_bin, float high_freq, float high_mag);
    static void print_frequency_distribution(int low_active, float low_sum,
                                           int mid_active, float mid_sum,
                                           int high_active, float high_sum);

    // Cycle through categories for staggered display
    static DebugCategory get_current_cycle_category();
    static void advance_cycle();

    // Master enable/disable
    static void enable_all(bool enabled = true);
    static void disable_all() { enable_all(false); }

    // Quick presets
    static void preset_minimal();      // Only critical info
    static void preset_performance();  // Performance monitoring focused
    static void preset_audio_debug();  // Audio pipeline debugging
    static void preset_full_debug();   // Everything enabled
};

// Convenience macros for cleaner code
#define DBG_ENCODER(ch, val, name, new_val) \
    if (DebugManager::should_print(DEBUG_ENCODER)) { \
        DebugManager::print_encoder(ch, val, name, new_val); \
        DebugManager::mark_printed(DEBUG_ENCODER); \
    }

#define DBG_ENCODER_BTN(ch, action, result) \
    if (DebugManager::should_print(DEBUG_ENCODER)) { \
        DebugManager::print_encoder_button(ch, action, result); \
        DebugManager::mark_printed(DEBUG_ENCODER); \
    }

#define DBG_AUDIO_VU(rms, floor, after) \
    if (DebugManager::should_print(DEBUG_AUDIO)) { \
        DebugManager::print_audio_vu(rms, floor, after); \
        DebugManager::mark_printed(DEBUG_AUDIO); \
    }

#define DBG_PERFORMANCE(fps, gdft, heap, cpu, bins, active, peak) \
    if (DebugManager::should_print(DEBUG_PERFORMANCE)) { \
        DebugManager::print_performance(fps, gdft, heap, cpu, bins, active, peak); \
        DebugManager::mark_printed(DEBUG_PERFORMANCE); \
    }

#define DBG_COLOR_SHIFT(novelty, hue, speed) \
    if (DebugManager::should_print(DEBUG_COLOR)) { \
        DebugManager::print_color_shift(novelty, hue, speed); \
        DebugManager::mark_printed(DEBUG_COLOR); \
    }

#define DBG_FREQ_SPECTRUM(peak_bin, peak_freq, peak_mag, high_bin, high_freq, high_mag) \
    if (DebugManager::should_print(DEBUG_FREQUENCY)) { \
        DebugManager::print_frequency_spectrum(peak_bin, peak_freq, peak_mag, high_bin, high_freq, high_mag); \
        DebugManager::mark_printed(DEBUG_FREQUENCY); \
    }

#define DBG_FREQ_DIST(low_active, low_sum, mid_active, mid_sum, high_active, high_sum) \
    if (DebugManager::should_print(DEBUG_FREQUENCY)) { \
        DebugManager::print_frequency_distribution(low_active, low_sum, mid_active, mid_sum, high_active, high_sum); \
    }

#endif // DEBUG_MANAGER_H

// PALETTE DEBUG INSTRUMENTATION [2025-09-20]
// Systematic debugging for palette color washing bug
// ROOT CAUSE HYPOTHESIS: hue_position walking through bright palette regions

#pragma once
#include <Arduino.h>
#include "../globals.h"

// Define USBSerial for ESP32-S3
#if ARDUINO_USB_CDC_ON_BOOT
  #define USBSerial Serial
#else
  extern HWCDC USBSerial;
#endif

// Debug flags - set to true to enable specific debug output
#define DEBUG_PALETTE_INDEX_WALKING false
#define DEBUG_PALETTE_INITIALIZATION false
#define DEBUG_COLOR_SHIFT_VALUES false
#define DEBUG_PALETTE_COLOR_EXTRACTION false

// Track palette state changes
struct PaletteDebugState {
    uint32_t frame_count;
    float last_hue_position;
    uint8_t last_palette_index;
    uint8_t last_palette_pos;
    bool palette_null_detected;
    uint32_t null_detection_count;
    uint32_t bright_region_hits;
    float brightness_threshold;

    PaletteDebugState() :
        frame_count(0),
        last_hue_position(-1.0f),
        last_palette_index(255),
        last_palette_pos(255),
        palette_null_detected(false),
        null_detection_count(0),
        bright_region_hits(0),
        brightness_threshold(0.8f) {}
};

extern PaletteDebugState palette_debug_state;

// Inline debug functions for minimal overhead
inline void debug_palette_index_walking(float hue_pos, uint8_t palette_pos, uint8_t palette_idx) {
    if (!DEBUG_PALETTE_INDEX_WALKING) return;

    // Only log when values change or every 60 frames
    if ((palette_debug_state.frame_count % 60 == 0) ||
        (fabs(hue_pos - palette_debug_state.last_hue_position) > 0.01f) ||
        (palette_pos != palette_debug_state.last_palette_pos)) {

        USBSerial.printf("[PALETTE_WALK] frame=%u hue_pos=%.4f palette_pos=%u palette_idx=%u\n",
                        palette_debug_state.frame_count, hue_pos, palette_pos, palette_idx);

        palette_debug_state.last_hue_position = hue_pos;
        palette_debug_state.last_palette_pos = palette_pos;
    }
}

inline void debug_palette_initialization(const void* palette_ptr, size_t palette_size) {
    if (!DEBUG_PALETTE_INITIALIZATION) return;

    if (palette_ptr == nullptr || palette_size == 0) {
        if (!palette_debug_state.palette_null_detected) {
            USBSerial.printf("[PALETTE_INIT] NULL DETECTED! frame=%u ptr=%p size=%u\n",
                            palette_debug_state.frame_count, palette_ptr, palette_size);
            palette_debug_state.palette_null_detected = true;
        }
        palette_debug_state.null_detection_count++;
    } else if (palette_debug_state.palette_null_detected) {
        USBSerial.printf("[PALETTE_INIT] Recovered from NULL. frame=%u ptr=%p size=%u null_count=%u\n",
                        palette_debug_state.frame_count, palette_ptr, palette_size,
                        palette_debug_state.null_detection_count);
        palette_debug_state.palette_null_detected = false;
    }
}

inline void debug_color_shift_values(float novelty, float hue_speed, float hue_direction) {
    if (!DEBUG_COLOR_SHIFT_VALUES) return;

    // Log every 2 seconds
    static uint32_t last_log = 0;
    if (millis() - last_log > 2000) {
        USBSerial.printf("[COLOR_SHIFT] novelty=%.4f speed=%.6f direction=%.1f hue_pos=%.4f\n",
                        novelty, hue_speed, hue_direction, float(hue_position));
        last_log = millis();
    }
}

inline void debug_palette_color_extraction(uint8_t palette_pos, float r, float g, float b) {
    if (!DEBUG_PALETTE_COLOR_EXTRACTION) return;

    // Detect bright regions (high luminance)
    float luminance = 0.2126f * r + 0.7152f * g + 0.0722f * b;

    if (luminance > palette_debug_state.brightness_threshold) {
        palette_debug_state.bright_region_hits++;

        // Log bright region hits every 10 occurrences
        if (palette_debug_state.bright_region_hits % 10 == 0) {
            USBSerial.printf("[PALETTE_BRIGHT] BRIGHT REGION HIT! pos=%u lum=%.3f rgb=(%.2f,%.2f,%.2f) total_hits=%u\n",
                            palette_pos, luminance, r, g, b, palette_debug_state.bright_region_hits);
        }
    }

    // Periodic color sampling
    if (palette_debug_state.frame_count % 120 == 0) {
        USBSerial.printf("[PALETTE_COLOR] pos=%u rgb=(%.3f,%.3f,%.3f) lum=%.3f\n",
                        palette_pos, r, g, b, luminance);
    }
}

inline void debug_frame_increment() {
    palette_debug_state.frame_count++;

    // Periodic status report
    if (palette_debug_state.frame_count % 300 == 0) {
        USBSerial.printf("[PALETTE_STATUS] frames=%u null_detections=%u bright_hits=%u\n",
                        palette_debug_state.frame_count,
                        palette_debug_state.null_detection_count,
                        palette_debug_state.bright_region_hits);
    }
}

// Macro to insert at key points
#define PALETTE_DEBUG_FRAME() debug_frame_increment()
#define PALETTE_DEBUG_INDEX(hue, pos, idx) debug_palette_index_walking(hue, pos, idx)
#define PALETTE_DEBUG_INIT(ptr, size) debug_palette_initialization(ptr, size)
#define PALETTE_DEBUG_SHIFT(nov, speed, dir) debug_color_shift_values(nov, speed, dir)
#define PALETTE_DEBUG_COLOR(pos, r, g, b) debug_palette_color_extraction(pos, r, g, b)

#pragma once
#include <Arduino.h>
#include "../constants.h"
#include "../globals.h"
#include <algorithm>
#include <cmath>

// PALETTE PROFILING SYSTEM [2025-09-20]
// Analyzes each palette's luminance distribution to find optimal sampling zones
// Prevents both white washing AND dark muddy colors

struct PaletteProfile {
    uint8_t safe_idx_low;    // Lower bound of safe sampling range
    uint8_t safe_idx_high;   // Upper bound of safe sampling range
    uint8_t optimal_idx;     // Best single index for static sampling
    float luma_peak;         // Peak luminance in palette [0..1]
    float luma_avg;          // Average luminance [0..1]
    float chroma_avg;        // Average chromaticity
    float max_brightness;    // Calculated max safe brightness for this palette
    bool has_white_regions;  // True if palette contains near-white areas
    const char* name;        // Palette name for debugging
};

// Global palette profiles - one for each Crameri palette
extern PaletteProfile g_palette_profiles[25]; // Index 0 = HSV (dummy), 1-24 = Crameri

// Profile a single palette and find its safe sampling window
inline PaletteProfile profile_palette(const CRGB16* lut, size_t size, const char* name) {
    PaletteProfile prof;
    prof.name = name;

    // Calculate perceptual luminance using sRGB weights
    auto calc_luma = [](const CRGB16& c) -> float {
        // Linear space calculation (CRGB16 is already linear)
        float r = float(c.r);
        float g = float(c.g);
        float b = float(c.b);
        return 0.2126f * r + 0.7152f * g + 0.0722f * b;
    };

    // Calculate chromaticity (color saturation)
    auto calc_chroma = [](const CRGB16& c) -> float {
        float r = float(c.r);
        float g = float(c.g);
        float b = float(c.b);
        float max_ch = fmax(fmax(r, g), b);
        float min_ch = fmin(fmin(r, g), b);
        if (max_ch < 0.001f) return 0.0f;
        return (max_ch - min_ch) / max_ch;
    };

    // Analyze the entire palette
    float* lumas = new float[size];
    float* chromas = new float[size];
    float luma_sum = 0.0f;
    float chroma_sum = 0.0f;
    prof.luma_peak = 0.0f;
    prof.has_white_regions = false;

    for (size_t i = 0; i < size; i++) {
        lumas[i] = calc_luma(lut[i]);
        chromas[i] = calc_chroma(lut[i]);

        luma_sum += lumas[i];
        chroma_sum += chromas[i];

        if (lumas[i] > prof.luma_peak) {
            prof.luma_peak = lumas[i];
        }

        // Detect near-white regions (high luma + low chroma)
        if (lumas[i] > 0.85f && chromas[i] < 0.15f) {
            prof.has_white_regions = true;
        }
    }

    prof.luma_avg = luma_sum / size;
    prof.chroma_avg = chroma_sum / size;

    // Find the safe sampling window (avoid extremes)
    // Strategy: Find contiguous region with good color (chroma > 0.2) and moderate brightness

    // Method 1: Statistical percentile approach
    // Find indices where luminance is between 10th and 85th percentile
    float sorted_lumas[256];
    memcpy(sorted_lumas, lumas, size * sizeof(float));
    std::sort(sorted_lumas, sorted_lumas + size);

    float luma_p10 = sorted_lumas[size / 10];      // 10th percentile
    float luma_p85 = sorted_lumas[size * 85 / 100]; // 85th percentile

    // Find the longest contiguous safe region
    int best_start = 0;
    int best_length = 0;
    int current_start = -1;
    int current_length = 0;

    for (size_t i = 0; i < size; i++) {
        bool is_safe = (lumas[i] >= luma_p10 &&
                       lumas[i] <= luma_p85 &&
                       chromas[i] > 0.15f);  // Require some color

        if (is_safe) {
            if (current_start < 0) {
                current_start = i;
                current_length = 1;
            } else {
                current_length++;
            }

            if (current_length > best_length) {
                best_start = current_start;
                best_length = current_length;
            }
        } else {
            current_start = -1;
            current_length = 0;
        }
    }

    // Set safe bounds with fallback
    if (best_length > 32) {  // Require at least 32 consecutive safe indices
        prof.safe_idx_low = best_start;
        prof.safe_idx_high = best_start + best_length - 1;
    } else {
        // Fallback: use middle 50% of palette
        prof.safe_idx_low = size / 4;
        prof.safe_idx_high = size * 3 / 4;
    }

    // Find optimal single index (best color/brightness balance)
    float best_score = -1.0f;
    prof.optimal_idx = 128; // Default fallback

    for (int i = prof.safe_idx_low; i <= prof.safe_idx_high; i++) {
        // Score based on: moderate luminance + high chroma
        float luma_score = 1.0f - fabs(lumas[i] - 0.5f) * 2.0f; // Peak at 0.5
        float chroma_score = chromas[i];
        float score = luma_score * 0.4f + chroma_score * 0.6f;

        if (score > best_score) {
            best_score = score;
            prof.optimal_idx = i;
        }
    }

    // Calculate max safe brightness for this palette
    // Inverse of peak luminance, clamped to reasonable range
    if (prof.luma_peak > 0.001f) {
        prof.max_brightness = fmin(0.95f, 0.85f / prof.luma_peak);
        prof.max_brightness = fmax(0.5f, prof.max_brightness); // Don't go too dim
    } else {
        prof.max_brightness = 0.85f; // Safe default
    }

    // Clean up
    delete[] lumas;
    delete[] chromas;

    return prof;
}

// Initialize all palette profiles (call once at startup)
void initialize_palette_profiles();

// Get profile for current palette
inline const PaletteProfile& get_current_palette_profile() {
    uint8_t idx = CONFIG.PALETTE_INDEX;
    if (idx > 24) idx = 0; // Safety clamp
    return g_palette_profiles[idx];
}

// CHROMATIC GUARD - Detect and prevent near-white colors
inline bool is_near_white(const CRGB16& c, float threshold = 0.10f) {
    float r = float(c.r);
    float g = float(c.g);
    float b = float(c.b);

    float max_ch = fmax(fmax(r, g), b);
    float min_ch = fmin(fmin(r, g), b);

    // High luminance + low chromaticity = near white
    bool high_luma = max_ch > 0.90f;
    bool low_chroma = (max_ch - min_ch) < (threshold * max_ch);

    return high_luma && low_chroma;
}

// Apply chromatic guard to prevent white washing
inline void apply_chromatic_guard(CRGB16& c) {
    if (is_near_white(c)) {
        // Reduce all channels by 15% to pull away from white
        c.r = SQ15x16(float(c.r) * 0.85f);
        c.g = SQ15x16(float(c.g) * 0.85f);
        c.b = SQ15x16(float(c.b) * 0.85f);

        // Optional: boost the weakest channel slightly to add color
        float r = float(c.r);
        float g = float(c.g);
        float b = float(c.b);

        if (r <= g && r <= b) {
            c.r = SQ15x16(r * 0.95f); // Slight de-boost for weakest
        } else if (g <= r && g <= b) {
            c.g = SQ15x16(g * 0.95f);
        } else {
            c.b = SQ15x16(b * 0.95f);
        }
    }
}
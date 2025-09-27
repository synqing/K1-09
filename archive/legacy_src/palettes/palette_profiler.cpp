// PALETTE PROFILER IMPLEMENTATION [2025-09-20]
// Pre-calculates safe sampling windows for all Crameri palettes

#include "palette_profiler.h"
#include "palette_luts_api.h"
#include "palette_metadata.h"
#include <FastLED.h>

// Define USBSerial for ESP32-S3
#if ARDUINO_USB_CDC_ON_BOOT
  #define USBSerial Serial
#else
  extern HWCDC USBSerial;
#endif

// Global palette profiles storage
PaletteProfile g_palette_profiles[25];

// Initialize all palette profiles at startup
void initialize_palette_profiles() {
    static bool initialized = false;
    if (initialized) return;

    USBSerial.println("[PALETTE_PROFILER] Initializing palette profiles...");

    // Profile 0: HSV mode (dummy profile)
    g_palette_profiles[0] = {
        0,      // safe_idx_low
        255,    // safe_idx_high
        128,    // optimal_idx
        1.0f,   // luma_peak
        0.5f,   // luma_avg
        1.0f,   // chroma_avg
        1.0f,   // max_brightness (no limit for HSV)
        false,  // has_white_regions
        "HSV"   // name
    };

    // Profile each Crameri palette
    for (uint8_t i = 0; i < CRAMERI_PALETTE_COUNT && i < 24; i++) {
        // Get the palette LUT
        const CRGB16* lut = palette_lut_for_index(i + 1);
        size_t size = palette_lut_size(i + 1);

        if (lut && size > 0) {
            // Profile this palette
            PaletteProfile prof = profile_palette(lut, size, get_crameri_palette_names()[i]);

            // Store the profile
            g_palette_profiles[i + 1] = prof;

            // Debug output
            USBSerial.printf("[PROFILE] %s: safe=[%d-%d] optimal=%d luma_peak=%.2f max_bright=%.2f%s\n",
                           prof.name,
                           prof.safe_idx_low,
                           prof.safe_idx_high,
                           prof.optimal_idx,
                           prof.luma_peak,
                           prof.max_brightness,
                           prof.has_white_regions ? " [HAS_WHITE]" : "");
        } else {
            // Fallback profile if LUT not available
            g_palette_profiles[i + 1] = {
                64,     // safe_idx_low
                192,    // safe_idx_high
                128,    // optimal_idx
                0.85f,  // luma_peak
                0.5f,   // luma_avg
                0.5f,   // chroma_avg
                0.85f,  // max_brightness
                false,  // has_white_regions
                get_crameri_palette_names()[i]
            };

            USBSerial.printf("[PROFILE] %s: Using fallback profile (LUT not available)\n",
                           get_crameri_palette_names()[i]);
        }
    }

    initialized = true;
    USBSerial.println("[PALETTE_PROFILER] Profile initialization complete");
}

// RUNTIME PALETTE PROFILING (optional - for debugging new palettes)
void debug_profile_current_palette() {
    uint8_t idx = CONFIG.PALETTE_INDEX;

    if (idx == 0) {
        USBSerial.println("[PROFILE_DEBUG] HSV mode - no palette to profile");
        return;
    }

    const CRGB16* lut = palette_lut_for_index(idx);
    size_t size = palette_lut_size(idx);

    if (!lut || size == 0) {
        USBSerial.printf("[PROFILE_DEBUG] Palette %d has no LUT\n", idx);
        return;
    }

    // Calculate detailed statistics
    float min_luma = 1.0f, max_luma = 0.0f;
    float min_chroma = 1.0f, max_chroma = 0.0f;
    int white_count = 0;
    int dark_count = 0;
    int vibrant_count = 0;

    for (size_t i = 0; i < size; i++) {
        float r = float(lut[i].r);
        float g = float(lut[i].g);
        float b = float(lut[i].b);

        // Luminance
        float luma = 0.2126f * r + 0.7152f * g + 0.0722f * b;
        min_luma = fmin(min_luma, luma);
        max_luma = fmax(max_luma, luma);

        // Chromaticity
        float max_ch = fmax(fmax(r, g), b);
        float min_ch = fmin(fmin(r, g), b);
        float chroma = (max_ch > 0.001f) ? (max_ch - min_ch) / max_ch : 0.0f;
        min_chroma = fmin(min_chroma, chroma);
        max_chroma = fmax(max_chroma, chroma);

        // Categorize
        if (luma > 0.85f && chroma < 0.15f) white_count++;
        if (luma < 0.15f) dark_count++;
        if (chroma > 0.5f && luma > 0.2f && luma < 0.8f) vibrant_count++;
    }

    const PaletteProfile& prof = g_palette_profiles[idx];

    USBSerial.println("=== PALETTE PROFILE DEBUG ===");
    USBSerial.printf("Palette: %s (index %d)\n", prof.name, idx);
    USBSerial.printf("Luminance: min=%.3f avg=%.3f max=%.3f\n", min_luma, prof.luma_avg, max_luma);
    USBSerial.printf("Chroma: min=%.3f avg=%.3f max=%.3f\n", min_chroma, prof.chroma_avg, max_chroma);
    USBSerial.printf("Safe window: [%d - %d] optimal=%d\n", prof.safe_idx_low, prof.safe_idx_high, prof.optimal_idx);
    USBSerial.printf("Max brightness: %.2f\n", prof.max_brightness);
    USBSerial.printf("Regions: white=%d/256 dark=%d/256 vibrant=%d/256\n", white_count, dark_count, vibrant_count);
    USBSerial.printf("Has white regions: %s\n", prof.has_white_regions ? "YES" : "NO");
    USBSerial.println("=============================");
}
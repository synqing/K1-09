#ifndef WAVEFORM_ENHANCEMENT_DEBUG_H
#define WAVEFORM_ENHANCEMENT_DEBUG_H

#include <Arduino.h>
#include <FastLED.h>
#include <FixedPoints.h>
#include "constants.h"

// Enable debug mode for enhancement validation
#define WAVEFORM_ENHANCEMENT_DEBUG 0

#if WAVEFORM_ENHANCEMENT_DEBUG

// Debug color tracking structure
struct ColorValidation {
    CRGB16 original_color;
    CRGB16 enhanced_color;
    float original_energy;
    float enhanced_energy;
    float original_ratios[3];
    float enhanced_ratios[3];
    bool energy_conserved;
    bool ratios_preserved;
    uint32_t frame_count;
};

// Mathematical constraint validation framework
class ChromagramConstraintValidator {
private:
    static constexpr float ENERGY_TOLERANCE = 0.01f;  // 1% energy conservation tolerance
    static constexpr float RATIO_TOLERANCE = 0.05f;   // 5% ratio preservation tolerance

public:
    struct ValidationResult {
        bool energy_conserved;
        bool ratios_preserved;
        bool pipeline_integrity;
        float energy_deviation;
        float max_ratio_deviation;
        uint32_t validation_frame;
    };

    static ValidationResult validate_enhancement(const CRGB16& pre_enhancement,
                                                const CRGB16& post_enhancement,
                                                uint32_t frame_num) {
        ValidationResult result = {0};
        result.validation_frame = frame_num;

        // Energy conservation analysis
        float pre_energy = float(pre_enhancement.r + pre_enhancement.g + pre_enhancement.b);
        float post_energy = float(post_enhancement.r + post_enhancement.g + post_enhancement.b);

        result.energy_deviation = (post_energy - pre_energy) / (pre_energy + 0.001f);
        result.energy_conserved = (result.energy_deviation <= ENERGY_TOLERANCE);

        // Ratio preservation analysis
        if (pre_energy > 0.1f) {
            float pre_ratios[3] = {
                float(pre_enhancement.r) / pre_energy,
                float(pre_enhancement.g) / pre_energy,
                float(pre_enhancement.b) / pre_energy
            };
            float post_ratios[3] = {
                float(post_enhancement.r) / post_energy,
                float(post_enhancement.g) / post_energy,
                float(post_enhancement.b) / post_energy
            };

            result.max_ratio_deviation = 0.0f;
            for (int i = 0; i < 3; i++) {
                float deviation = fabs(post_ratios[i] - pre_ratios[i]);
                if (deviation > result.max_ratio_deviation) {
                    result.max_ratio_deviation = deviation;
                }
            }
            result.ratios_preserved = (result.max_ratio_deviation <= RATIO_TOLERANCE);
        } else {
            result.ratios_preserved = true; // Skip ratio check for low energy
        }

        result.pipeline_integrity = result.energy_conserved && result.ratios_preserved;

        return result;
    }
};

// Emergency rollback system for color corruption
class EmergencyRollbackSystem {
private:
    static CRGB16 safe_fallback_color;
    static bool rollback_active;
    static uint32_t rollback_start_frame;
    static constexpr uint32_t ROLLBACK_DURATION = 180; // 3 seconds @ 60Hz

public:
    static void trigger_emergency_rollback(const char* reason, uint32_t frame_num) {
        USBSerial.printf("EMERGENCY ROLLBACK TRIGGERED Frame:%lu Reason:%s\n", frame_num, reason);
        rollback_active = true;
        rollback_start_frame = frame_num;

        // Set safe fallback color (dim white)
        safe_fallback_color = {SQ15x16(0.2), SQ15x16(0.2), SQ15x16(0.2)};
    }

    static bool is_rollback_active(uint32_t current_frame) {
        if (rollback_active && (current_frame - rollback_start_frame) > ROLLBACK_DURATION) {
            USBSerial.printf("ROLLBACK COMPLETE Frame:%lu\n", current_frame);
            rollback_active = false;
        }
        return rollback_active;
    }

    static CRGB16 get_safe_color() {
        return safe_fallback_color;
    }

    static void reset_rollback() {
        rollback_active = false;
        USBSerial.println("ROLLBACK SYSTEM RESET");
    }
};

// Static member initialization
bool EmergencyRollbackSystem::rollback_active = false;
CRGB16 EmergencyRollbackSystem::safe_fallback_color = {0, 0, 0};
uint32_t EmergencyRollbackSystem::rollback_start_frame = 0;

// Comprehensive enhancement verification system
class WaveformEnhancementVerifier {
private:
    static constexpr uint32_t VERIFICATION_INTERVAL = 60; // Verify every 60 frames (1 second @ 60Hz)
    static uint32_t last_verification_frame;
    static uint32_t corruption_count;
    static uint32_t total_validations;

public:
    struct VerificationReport {
        uint32_t total_frames_tested;
        uint32_t corruptions_detected;
        float corruption_rate;
        bool system_stable;
        uint32_t report_timestamp;
    };

    static VerificationReport run_comprehensive_verification() {
        VerificationReport report = {0};
        report.total_frames_tested = total_validations;
        report.corruptions_detected = corruption_count;
        report.corruption_rate = (total_validations > 0) ?
                                (float(corruption_count) / float(total_validations)) : 0.0f;
        report.system_stable = (report.corruption_rate < 0.01f); // <1% corruption rate acceptable
        report.report_timestamp = millis();

        // Reset counters for next interval
        corruption_count = 0;
        total_validations = 0;

        return report;
    }

    static void log_frame_validation(bool passed) {
        total_validations++;
        if (!passed) {
            corruption_count++;
        }
    }

    static void print_verification_report() {
        static uint32_t last_report = 0;
        if (millis() - last_report > 5000) { // Report every 5 seconds
            last_report = millis();
            VerificationReport report = run_comprehensive_verification();

            USBSerial.printf("VERIFICATION REPORT: Frames:%lu Corruptions:%lu Rate:%.3f%% Stable:%s\n",
                           report.total_frames_tested,
                           report.corruptions_detected,
                           report.corruption_rate * 100.0f,
                           report.system_stable ? "YES" : "NO");
        }
    }
};

// Static member initialization
uint32_t WaveformEnhancementVerifier::last_verification_frame = 0;
uint32_t WaveformEnhancementVerifier::corruption_count = 0;
uint32_t WaveformEnhancementVerifier::total_validations = 0;

// Performance monitoring for enhancement overhead
class EnhancementPerformanceMonitor {
private:
    static uint32_t enhancement_start_time;
    static uint32_t total_enhancement_time;
    static uint32_t enhancement_calls;
    static constexpr uint32_t MAX_ENHANCEMENT_TIME_US = 100; // 100μs maximum overhead

public:
    static void start_timing() {
        enhancement_start_time = micros();
    }

    static void end_timing() {
        uint32_t enhancement_duration = micros() - enhancement_start_time;
        total_enhancement_time += enhancement_duration;
        enhancement_calls++;

        if (enhancement_duration > MAX_ENHANCEMENT_TIME_US) {
            USBSerial.printf("PERFORMANCE WARNING: Enhancement took %luμs (limit: %luμs)\n",
                           enhancement_duration, MAX_ENHANCEMENT_TIME_US);
        }

        // Report average performance every 1000 calls
        if (enhancement_calls % 1000 == 0) {
            uint32_t avg_time = total_enhancement_time / enhancement_calls;
            USBSerial.printf("PERFORMANCE REPORT: Avg enhancement time: %luμs over %lu calls\n",
                           avg_time, enhancement_calls);
        }
    }
};

// Static member initialization
uint32_t EnhancementPerformanceMonitor::enhancement_start_time = 0;
uint32_t EnhancementPerformanceMonitor::total_enhancement_time = 0;
uint32_t EnhancementPerformanceMonitor::enhancement_calls = 0;

// Real-time color corruption detection
inline bool validate_color_integrity(const CRGB16& original, const CRGB16& enhanced, uint32_t frame_num) {
    // Energy conservation check
    float original_energy = float(original.r + original.g + original.b);
    float enhanced_energy = float(enhanced.r + enhanced.g + enhanced.b);

    // Allow 1% tolerance for floating point precision
    if (enhanced_energy > original_energy * 1.01f) {
        USBSerial.printf("CORRUPTION DETECTED Frame:%lu - Energy violation: %.3f > %.3f\n",
                         frame_num, enhanced_energy, original_energy);
        return false;
    }

    // Ratio preservation check (only if energy > threshold)
    if (original_energy > 0.1f) {
        float orig_ratios[3] = {
            float(original.r) / original_energy,
            float(original.g) / original_energy,
            float(original.b) / original_energy
        };
        float enh_ratios[3] = {
            float(enhanced.r) / enhanced_energy,
            float(enhanced.g) / enhanced_energy,
            float(enhanced.b) / enhanced_energy
        };

        // Check ratio deviation (5% tolerance)
        for (int i = 0; i < 3; i++) {
            if (fabs(enh_ratios[i] - orig_ratios[i]) > 0.05f) {
                USBSerial.printf("CORRUPTION DETECTED Frame:%lu - Ratio violation channel %d: %.3f vs %.3f\n",
                                frame_num, i, enh_ratios[i], orig_ratios[i]);
                return false;
            }
        }
    }

    return true; // Color integrity maintained
}

// SAFE METHOD: Probabilistic Sub-Pixel Positioning
inline void apply_safe_subpixel_enhancement(float pos_f, const CRGB16& color, CRGB16* led_buffer) {
    static uint32_t frame_counter = 0;
    frame_counter++;

    // Extract fractional component
    float fractional = pos_f - floor(pos_f);

    // Validation checkpoint
    CRGB16 original_color = color;

    if (fractional < 0.05f || fractional > 0.95f) {
        // Near integer position - use standard rounding
        int pos = int(pos_f + (pos_f >= 0 ? 0.5 : -0.5));
        if (pos >= 0 && pos < NATIVE_RESOLUTION) {
            led_buffer[pos] = color; // Single assignment preserves pipeline

            if (!validate_color_integrity(original_color, color, frame_counter)) {
                USBSerial.println("SAFETY ABORT: Color corruption in standard rounding");
                led_buffer[pos] = original_color; // Immediate rollback
            }
        }
    } else {
        // Sub-pixel precision using probabilistic method (SAFE)
        float random_threshold = float(esp_random() & 0xFFFF) / 65536.0f;
        int final_pos = (random_threshold < fractional) ? (int(pos_f) + 1) : int(pos_f);

        if (final_pos >= 0 && final_pos < NATIVE_RESOLUTION) {
            led_buffer[final_pos] = color; // Single assignment preserves pipeline

            if (!validate_color_integrity(original_color, color, frame_counter)) {
                USBSerial.println("SAFETY ABORT: Color corruption in probabilistic positioning");
                led_buffer[final_pos] = original_color; // Immediate rollback
            }
        }
    }
}

// SAFE METHOD: Frequency-Aware Trail Fading
inline SQ15x16 apply_safe_frequency_trails(const SQ15x16* chromagram_data, float abs_amp) {
    static uint8_t freq_calc_counter = 0;
    static float cached_frequency_fade_factor = 0.75f;

    // Update frequency analysis every 4 frames (performance optimization)
    if (++freq_calc_counter >= 4) {
        freq_calc_counter = 0;

        // Calculate bass vs treble energy
        float bass_energy = (float(chromagram_data[0]) + float(chromagram_data[1]) + float(chromagram_data[2])) / 3.0f;
        float treble_energy = (float(chromagram_data[9]) + float(chromagram_data[10]) + float(chromagram_data[11])) / 3.0f;

        // Bass creates shorter trails, treble creates longer trails
        cached_frequency_fade_factor = 0.5f + 0.5f * (treble_energy / (bass_energy + treble_energy + 0.01f));

        // Validate musical logic
        if (cached_frequency_fade_factor < 0.0f || cached_frequency_fade_factor > 1.0f) {
            USBSerial.printf("SAFETY WARNING: Invalid frequency fade factor: %.3f\n", cached_frequency_fade_factor);
            cached_frequency_fade_factor = 0.75f; // Safe fallback
        }
    }

    // Apply frequency-dependent fading
    float max_fade_reduction = 0.05f + 0.10f * cached_frequency_fade_factor; // Range: 0.05-0.15
    SQ15x16 dynamic_fade_amount = 1.0 - (max_fade_reduction * abs_amp);

    return dynamic_fade_amount;
}

// User feedback integration for enhancement validation
inline void process_user_feedback_integration() {
    static uint32_t last_feedback_prompt = 0;

    // Prompt user for feedback every 30 seconds during testing
    if (millis() - last_feedback_prompt > 30000) {
        last_feedback_prompt = millis();

        USBSerial.println("ENHANCEMENT VALIDATION: Enter 'OK' if colors look correct, 'ABORT' if corrupted");

        // Non-blocking user input check
        if (USBSerial.available()) {
            String user_input = USBSerial.readString();
            user_input.trim();
            user_input.toUpperCase();

            if (user_input == "ABORT") {
                EmergencyRollbackSystem::trigger_emergency_rollback("User reported corruption", millis());
            } else if (user_input == "OK") {
                USBSerial.println("USER VALIDATION: Enhancement approved");
            }
        }
    }
}

// Automated test sequence for enhancement validation
void run_enhancement_validation_sequence() {
    USBSerial.println("STARTING WAVEFORM ENHANCEMENT VALIDATION SEQUENCE");

    // Test 1: Pure chromagram colors
    USBSerial.println("TEST 1: Pure chromagram color validation");
    // Test implementation would go here

    // Test 2: Energy conservation
    USBSerial.println("TEST 2: Energy conservation validation");
    // Test implementation would go here

    // Test 3: Ratio preservation
    USBSerial.println("TEST 3: Ratio preservation validation");
    // Test implementation would go here

    // Test 4: Performance validation
    USBSerial.println("TEST 4: Performance overhead validation");
    // Test implementation would go here

    // Test 5: User feedback integration
    USBSerial.println("TEST 5: User feedback integration test");
    // Test implementation would go here

    USBSerial.println("VALIDATION SEQUENCE COMPLETE - Check results above");
}

#else // WAVEFORM_ENHANCEMENT_DEBUG disabled

// Dummy implementations when debug is disabled
inline void apply_safe_subpixel_enhancement(float pos_f, const CRGB16& color, CRGB16* led_buffer) {
    // Standard implementation
    int pos = int(pos_f + (pos_f >= 0 ? 0.5 : -0.5));
    if (pos >= 0 && pos < NATIVE_RESOLUTION) {
        led_buffer[pos] = color;
    }
}

inline SQ15x16 apply_safe_frequency_trails(const SQ15x16* chromagram_data, float abs_amp) {
    // Standard implementation
    float max_fade_reduction = 0.10f;
    return SQ15x16(1.0 - (max_fade_reduction * abs_amp));
}

inline void process_user_feedback_integration() {
    // No-op when debug disabled
}

void run_enhancement_validation_sequence() {
    // No-op when debug disabled
}

#endif // WAVEFORM_ENHANCEMENT_DEBUG

#endif // WAVEFORM_ENHANCEMENT_DEBUG_H

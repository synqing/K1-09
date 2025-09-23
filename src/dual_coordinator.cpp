#include "dual_coordinator.h"
#include "globals.h"
#include "constants.h"
#include <Arduino.h>  // For millis(), random()

// Global coordinator instances
CouplingPlan g_coupling_plan;
RouterState g_router_state;

void coordinator_init() {
    // Initialize coupling plan with safe default values that respect current configuration
    g_coupling_plan.primary_mode = CONFIG.LIGHTSHOW_MODE; // Use current primary mode
    g_coupling_plan.secondary_mode = SECONDARY_LIGHTSHOW_MODE; // Use current secondary mode
    g_coupling_plan.phase_offset = SQ15x16(0.0);
    g_coupling_plan.anti_phase = false;
    g_coupling_plan.hue_detune = SQ15x16(0.0);
    g_coupling_plan.variation_type = OP_MIRROR;
    g_coupling_plan.intensity_balance = SQ15x16(0.5);
    
    // Initialize router state
    g_router_state.dwell_start = millis();
    g_router_state.current_pair = 0;
    g_router_state.cooldown_remaining = 0;
    g_router_state.variation_pending = false;
    g_router_state.last_beat_time = 0;
    g_router_state.novelty_peak = SQ15x16(0.0);
}

CouplingPlan coordinator_update(RouterState& router,
                                const SQ15x16* novelty_curve_ptr,
                                SQ15x16 audio_vu,
                                uint32_t current_time_ms) {
    CouplingPlan plan;
    
    // Update router state based on audio features
    router_update(router, novelty_curve_ptr, audio_vu, current_time_ms);
    
    // For Phase 1: Maintain current mode assignments to avoid breaking existing functionality
    // Future enhancement: implement complementary pair selection
    plan.primary_mode = CONFIG.LIGHTSHOW_MODE;
    plan.secondary_mode = SECONDARY_LIGHTSHOW_MODE;
    
    // Apply variations if pending
    if (router.variation_pending) {
        plan.variation_type = random(NUM_OPERATOR_TYPES);
        
        switch (plan.variation_type) {
            case OP_ANTI_PHASE:
                plan.anti_phase = true;
                plan.phase_offset = SQ15x16(0.5);
                break;
            case OP_HUE_DETUNE:
                // Small hue detune: ±0.08 as specified in operating plan
                plan.hue_detune = SQ15x16((random(17) - 8) / 100.0); // ±0.08
                break;
            case OP_CIRCULATE:
                plan.phase_offset = SQ15x16(random(4) / 10.0); // 0.0-0.3
                break;
            default:
                plan.anti_phase = false;
                plan.phase_offset = SQ15x16(0.0);
                plan.hue_detune = SQ15x16(0.0);
                break;
        }
        
        router.variation_pending = false;
    } else {
        // Default: minimal variations for organic feel
        plan.anti_phase = false;
        plan.phase_offset = SQ15x16(random(11) / 100.0); // 0.0-0.1
        plan.hue_detune = SQ15x16((random(9) - 4) / 100.0); // ±0.04
        plan.variation_type = OP_MIRROR;
    }
    
    // Balance intensity based on audio energy
    plan.intensity_balance = SQ15x16(0.5) + (audio_vu - SQ15x16(0.5)) * SQ15x16(0.2);
    if (plan.intensity_balance < SQ15x16(0.3)) plan.intensity_balance = SQ15x16(0.3);
    if (plan.intensity_balance > SQ15x16(0.7)) plan.intensity_balance = SQ15x16(0.7);
    
    return plan;
}

void router_update(RouterState& state, const SQ15x16* novelty_curve_ptr, SQ15x16 audio_vu, uint32_t current_time_ms) {
    // Safety check for novelty curve access
    if (!novelty_curve_ptr || spectral_history_index >= SPECTRAL_HISTORY_LENGTH) {
        return;
    }
    
    // Track novelty peaks for variation triggering
    SQ15x16 current_novelty = novelty_curve_ptr[spectral_history_index];
    if (current_novelty > state.novelty_peak) {
        state.novelty_peak = current_novelty;
    }
    
    // Natural decay of novelty peak
    state.novelty_peak *= SQ15x16(0.995);
    if (state.novelty_peak < SQ15x16(0.01)) state.novelty_peak = SQ15x16(0.01);
    
    // Check for beat detection (simple threshold-based)
    static SQ15x16 last_vu = SQ15x16(0.0);
    SQ15x16 vu_delta = audio_vu - last_vu;
    last_vu = audio_vu;
    
    // Simple beat detection: rapid rise in VU level
    if (vu_delta > SQ15x16(0.08) && audio_vu > SQ15x16(0.15)) {
        state.last_beat_time = current_time_ms;
    }
    
    // Handle cooldown period
    if (state.cooldown_remaining > 0) {
        state.cooldown_remaining--;
        return;
    }
    
    // Check dwell time (4-8 beats as per operating plan)
    uint32_t dwell_time = current_time_ms - state.dwell_start;
    
    // Estimate beats based on time (assuming 120 BPM = 500ms per beat)
    uint32_t beats_elapsed = dwell_time / 500;
    
    // Check if we should transition (4-8 beat dwell)
    if (beats_elapsed >= 4 && beats_elapsed <= 8) {
        // 30% chance to transition after minimum dwell
        if (random(100) < 30) {
            // Start cooldown (2-4 beats as per operating plan)
            state.cooldown_remaining = 2 + random(3); // 2-4 beats cooldown
            state.dwell_start = current_time_ms;
            
            // Trigger variation on transition
            state.variation_pending = should_trigger_variation(state, current_novelty, audio_vu);
        }
    } else if (beats_elapsed > 8) {
        // Force transition after maximum dwell
        state.cooldown_remaining = 2 + random(3);
        state.dwell_start = current_time_ms;
        state.variation_pending = should_trigger_variation(state, current_novelty, audio_vu);
    }
}

bool should_trigger_variation(const RouterState& state, SQ15x16 current_novelty, SQ15x16 audio_vu) {
    // Higher novelty peaks increase variation chance
    SQ15x16 novelty_factor = state.novelty_peak * SQ15x16(2.0);
    if (novelty_factor > SQ15x16(1.0)) novelty_factor = SQ15x16(1.0);
    
    // Higher audio energy increases variation chance
    SQ15x16 energy_factor = audio_vu * SQ15x16(1.5);
    if (energy_factor > SQ15x16(1.0)) energy_factor = SQ15x16(1.0);
    
    // Combined probability (0-40% chance)
    uint8_t variation_chance = (novelty_factor + energy_factor).getInteger() * 20;
    
    return random(100) < variation_chance;
}

uint8_t select_complementary_pair(const RouterState& state, SQ15x16 audio_energy) {
    // For Phase 1: maintain current pair to avoid disrupting existing functionality
    // Future enhancement: implement audio-based pair selection
    return state.current_pair;
}

// Pure operator functions (no buffer writes, as required by operating plan)
SQ15x16 operator_mirror(SQ15x16 position, bool anti_phase) {
    if (anti_phase) {
        return SQ15x16(1.0) - position;
    }
    return position;
}

SQ15x16 operator_hue_detune(SQ15x16 base_hue, SQ15x16 detune_amount) {
    SQ15x16 result = base_hue + detune_amount;
    
    // Wrap around hue space
    while (result < SQ15x16(0.0)) result += SQ15x16(1.0);
    while (result >= SQ15x16(1.0)) result -= SQ15x16(1.0);
    
    return result;
}

// Coordinator application helpers intentionally removed; channel runners will
// consume plan metadata directly to avoid buffer post-processing.

SQ15x16 operator_temporal_offset(SQ15x16 base_value, SQ15x16 phase_offset) {
    SQ15x16 result = base_value + phase_offset;
    
    // Wrap around temporal space
    while (result < SQ15x16(0.0)) result += SQ15x16(1.0);
    while (result >= SQ15x16(1.0)) result -= SQ15x16(1.0);
    
    return result;
}

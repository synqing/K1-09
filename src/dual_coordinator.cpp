#include "dual_coordinator.h"
#include "globals.h"
#include "constants.h"
#include <Arduino.h>  // For millis(), random()

#if ARDUINO_USB_CDC_ON_BOOT
#define ROUTER_SERIAL Serial
#else
#define ROUTER_SERIAL USBSerial
#endif

// Global coordinator instances
CouplingPlan g_coupling_plan;
RouterState g_router_state;
bool g_router_enabled = true; // Agent B: freeze router plan when false
uint8_t g_router_last_reason = 0;
uint8_t g_router_last_dwell_beats = 0;

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
    // Cadence window initialization (Agent B)
    g_router_state.cadence_window_ms_start = millis();
    g_router_state.cadence_transitions = 0;
    g_router_state.cadence_variations = 0;
    g_router_state.last_variation_ms = 0;
}

CouplingPlan coordinator_update(RouterState& router,
                                const SQ15x16* novelty_curve_ptr,
                                SQ15x16 audio_vu,
                                uint32_t current_time_ms) {
    CouplingPlan plan;

#if !ENABLE_ROUTER_FSM
    (void)novelty_curve_ptr;
    (void)audio_vu;
    (void)current_time_ms;
#endif

#if ENABLE_ROUTER_FSM
    const bool router_active = g_router_enabled;
#else
    const bool router_active = false;
#endif

    if (!router_active) {
        plan.primary_mode = CONFIG.LIGHTSHOW_MODE;
        plan.secondary_mode = SECONDARY_LIGHTSHOW_MODE;
        plan.phase_offset = SQ15x16(0.0);
        plan.anti_phase = false;
        plan.hue_detune = SQ15x16(0.0);
        plan.variation_type = OP_MIRROR;
        plan.intensity_balance = SQ15x16(0.5);
        g_router_last_reason = ROUTER_NONE;
        g_router_last_dwell_beats = 0;
        return plan;
    }

#if ENABLE_ROUTER_FSM
    RouterReason reason = ROUTER_NONE;
    uint8_t dwell_beats = 0;
    // Drive FSM to determine pair selection and variations
    router_fsm_tick(novelty_curve_ptr, audio_vu, current_time_ms, router, plan, reason, dwell_beats);

    if (reason != ROUTER_NONE && ROUTER_SERIAL) {
        g_router_last_reason = (uint8_t)reason;
        g_router_last_dwell_beats = dwell_beats;
        const char* p_name = mode_names + (plan.primary_mode * 32);
        const char* s_name = mode_names + (plan.secondary_mode * 32);
        const char* reason_str = (reason == ROUTER_ONSET) ? "onset" :
                                 (reason == ROUTER_TIMEOUT) ? "timeout" :
                                 (reason == ROUTER_COOLDOWN_END) ? "cooldown" : "none";
        int offset_frames = int(plan.phase_offset * SQ15x16(60));
        if (offset_frames < 0) offset_frames = 0;
        ROUTER_SERIAL.printf("ROUTER pair=%s|%s reason=%s dwell=%u/8 var=%s,hue%+0.2f,offset=%d\n",
                             p_name ? p_name : "?",
                             s_name ? s_name : "?",
                             reason_str,
                             (unsigned)dwell_beats,
                             (plan.variation_type == OP_ANTI_PHASE ? "anti-phase" :
                              plan.variation_type == OP_CIRCULATE ? "circulate" :
                              plan.variation_type == OP_HUE_DETUNE ? "hue-detune" : "mirror"),
                             float(plan.hue_detune),
                             offset_frames);
    }

    if (ROUTER_SERIAL) {
        uint32_t win_ms = current_time_ms - router.cadence_window_ms_start;
        if (win_ms >= 4000) {
            ROUTER_SERIAL.printf("ROUTER cadence trans=%u var=%u window=%.1fs\n",
                                 (unsigned)router.cadence_transitions,
                                 (unsigned)router.cadence_variations,
                                 win_ms / 1000.0f);
            router.cadence_window_ms_start = current_time_ms;
            router.cadence_transitions = 0;
            router.cadence_variations = 0;
        }
    }
#endif

    if (plan.primary_mode >= NUM_MODES) {
        plan.primary_mode = CONFIG.LIGHTSHOW_MODE;
    }
    if (plan.secondary_mode >= NUM_MODES) {
        plan.secondary_mode = SECONDARY_LIGHTSHOW_MODE;
    }

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
#undef ROUTER_SERIAL

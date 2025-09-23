#ifndef DUAL_COORDINATOR_H
#define DUAL_COORDINATOR_H

#include <Arduino.h>
#include <FixedPoints.h>
#include <FixedPointsCommon.h>
#include "constants.h"
#include "globals.h"

// Coupling Plan - defines how the two strips interact for a frame
struct CouplingPlan {
    uint8_t primary_mode;
    uint8_t secondary_mode;
    SQ15x16 phase_offset;        // 0.0-1.0 temporal offset between strips
    bool anti_phase;             // Mirror/opposite phase relationship
    SQ15x16 hue_detune;          // ±0.08 hue variation between strips
    uint8_t variation_type;      // Operator type (mirror, circulate, complementary)
    SQ15x16 intensity_balance;   // Brightness balance between strips (0.0-1.0)
};

// Router State Machine - manages mode pairing transitions
struct RouterState {
    uint32_t dwell_start;        // When current pair started
    uint8_t current_pair;        // Index of current complementary pair
    uint8_t cooldown_remaining;  // Frames remaining in cooldown
    bool variation_pending;      // If variation should be injected
    uint32_t last_beat_time;     // Last detected beat time
    SQ15x16 novelty_peak;        // Peak novelty for variation triggering
};

// Complementary mode pairs (top/bottom strip combinations)
const uint8_t COMPLEMENTARY_PAIRS[][2] = {
    {LIGHT_MODE_WAVEFORM, LIGHT_MODE_BLOOM},           // Waveform + Bloom
    {LIGHT_MODE_GDFT, LIGHT_MODE_GDFT_CHROMAGRAM},     // GDFT + Chromagram
    {LIGHT_MODE_KALEIDOSCOPE, LIGHT_MODE_VU_DOT},      // Kaleidoscope + VU
    {LIGHT_MODE_QUANTUM_COLLAPSE, LIGHT_MODE_GDFT_CHROMAGRAM_DOTS} // Quantum + Chroma Dots
};

const uint8_t NUM_COMPLEMENTARY_PAIRS = 4;

// Operator types
enum OperatorType {
    OP_MIRROR = 0,      // Simple mirroring
    OP_ANTI_PHASE,      // Opposite phase relationship
    OP_CIRCULATE,       // Circular motion between strips
    OP_COMPLEMENTARY,   // Complementary color schemes
    OP_HUE_DETUNE,      // Small hue variations
    NUM_OPERATOR_TYPES
};

// External declarations
extern CouplingPlan g_coupling_plan;
extern RouterState g_router_state;

// Function declarations
void coordinator_init();
CouplingPlan coordinator_update(RouterState& router,
                                const SQ15x16* novelty_curve,
                                SQ15x16 audio_vu,
                                uint32_t current_time_ms);
void router_update(RouterState& state, const SQ15x16* novelty_curve, SQ15x16 audio_vu, uint32_t current_time_ms);
bool should_trigger_variation(const RouterState& state, SQ15x16 current_novelty, SQ15x16 audio_vu);
uint8_t select_complementary_pair(const RouterState& state, SQ15x16 audio_energy);

// Pure operator functions (no buffer writes)
SQ15x16 operator_mirror(SQ15x16 position, bool anti_phase);
SQ15x16 operator_hue_detune(SQ15x16 base_hue, SQ15x16 detune_amount);
SQ15x16 operator_temporal_offset(SQ15x16 base_value, SQ15x16 phase_offset);

#endif // DUAL_COORDINATOR_H

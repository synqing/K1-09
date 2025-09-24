/*
 * Phase 3 – Router FSM Overview
 * --------------------------------
 * Goals:
 * - Deterministic dwell/cooldown between complementary mode pairs to avoid thrash.
 * - Inject small, musical variations (anti‑phase, hue detune, temporal offsets).
 * - Keep operators pure (no buffer writes); plans are consumed by modes.
 *
 * Policy:
 * - Dwell 4–8 beats (est. 500ms/beat at 120 BPM). Force transition after 8 beats.
 * - Cooldown 2–4 beats after transitions; on cooldown end, allow one variation.
 * - On strong onsets (novelty+VU) after minimum dwell, 30% chance to transition.
 * - Variation selection is biased toward gentle changes:
 *     40% hue detune (±0.08), 30% anti‑phase, 30% circulate (small temporal offset).
 * - Intensity balance is a small bias around 0.5, driven by audio VU (clamped [0.3,0.7]).
 *
 * Implementation Notes:
 * - This file avoids USBSerial prints; logs are emitted in coordinator_update().
 * - The CouplingPlan is read‑only to modes via frame_config; no buffer reordering here.
 */

#include "dual_coordinator.h"
#include "globals.h"
#include <Arduino.h>

namespace {
// Beat estimation helper: assume ~120 BPM if we don't have a tempo; 500ms/beat
inline uint32_t ms_per_beat() { return 500; }

// Should we consider this a strong onset?
inline bool strong_onset(SQ15x16 novelty, SQ15x16 audio_vu) {
  // Thresholds are tunable via serial; defaults 0.20/0.08
  return (novelty > g_router_novelty_thresh) && (audio_vu > g_router_vu_delta_thresh);
}

// Pick a variation operator, biased toward gentle changes
inline OperatorType pick_variation_op() {
  uint8_t det = g_router_var_mix_detune;
  uint8_t anti = g_router_var_mix_anti;
  uint8_t circ = g_router_var_mix_circ;
  uint16_t sum = uint16_t(det) + anti + circ;
  if (sum == 0) { det = 40; anti = 30; circ = 30; sum = 100; }
  uint16_t r = random(sum);
  if (r < det) return OP_HUE_DETUNE;
  if (r < det + anti) return OP_ANTI_PHASE;
  return OP_CIRCULATE;
}

// Choose a small hue detune within ±0.08 as per plan
inline SQ15x16 random_hue_detune() {
  // Random in [-max, +max], max tunable (default 0.08)
  int steps = int(float(g_router_detune_max) * 100.0f + 0.5f);
  if (steps < 1) steps = 1;
  int s = random(-steps, steps + 1);
  return SQ15x16(s) / SQ15x16(100);
}

// Choose a temporal offset of 1–3 frames (encode as small phase offset)
inline SQ15x16 random_phase_offset() {
  uint8_t maxf = g_router_circ_frames_max;
  if (maxf < 1) maxf = 1;
  uint8_t frames = 1 + (random(maxf)); // 1..maxf
  // Represent as a very small phase shift; interpretation is mode‑specific
  return SQ15x16(frames) / SQ15x16(60); // ~ up to ~0.05 of a cycle
}

// Compute intensity balance around 0.5 with small audio bias
inline SQ15x16 compute_balance(SQ15x16 audio_vu) {
  SQ15x16 bal = SQ15x16(0.5) + (audio_vu - SQ15x16(0.5)) * SQ15x16(0.2);
  if (bal < g_router_balance_min) bal = g_router_balance_min;
  if (bal > g_router_balance_max) bal = g_router_balance_max;
  return bal;
}
}

void router_fsm_tick(const SQ15x16* novelty_curve,
                     SQ15x16 audio_vu,
                     uint32_t now_ms,
                     RouterState& state,
                     CouplingPlan& plan,
                     RouterReason& reason_out,
                     uint8_t& dwell_beats_estimate)
{
  reason_out = ROUTER_NONE;
  dwell_beats_estimate = 0;

  // Safety: novelty access
  SQ15x16 novelty_now = SQ15x16(0.0);
  if (novelty_curve && spectral_history_index < SPECTRAL_HISTORY_LENGTH) {
    novelty_now = novelty_curve[spectral_history_index];
  }
  // Track novelty peak with gentle decay
  if (novelty_now > state.novelty_peak) state.novelty_peak = novelty_now;
  state.novelty_peak *= SQ15x16(0.995);
  if (state.novelty_peak < SQ15x16(0.01)) state.novelty_peak = SQ15x16(0.01);

  // Beat detection via rising VU; keep local state here
  static SQ15x16 s_last_vu = SQ15x16(0.0);
  SQ15x16 vu_delta = audio_vu - s_last_vu;
  s_last_vu = audio_vu;
  bool beat = (vu_delta > SQ15x16(0.08)) && (audio_vu > SQ15x16(0.15));
  if (beat) state.last_beat_time = now_ms;

  // Handle cooldown in beats (2–4 beats)
  if (state.cooldown_remaining > 0 && beat) {
    state.cooldown_remaining--;
    if (state.cooldown_remaining == 0) {
      state.variation_pending = true; // allow a variation post‑cooldown
      reason_out = ROUTER_COOLDOWN_END;
    }
  }

  // Estimate dwell beats
  uint32_t dwell_ms = now_ms - state.dwell_start;
  uint32_t est_beats = ms_per_beat() ? (dwell_ms / ms_per_beat()) : 0;
  dwell_beats_estimate = (est_beats > 255) ? 255 : (uint8_t)est_beats;

  // Determine if we should transition pairs
  const uint8_t min_dwell_beats = g_router_dwell_min_beats;
  const uint8_t max_dwell_beats = g_router_dwell_max_beats;
  bool dwell_met = (est_beats >= min_dwell_beats);
  bool dwell_forced = (est_beats > max_dwell_beats);
  bool onset_trigger = strong_onset(state.novelty_peak, audio_vu);

  bool do_pair_transition = false;
  if (state.cooldown_remaining == 0 && dwell_met) {
    if (dwell_forced) {
      do_pair_transition = true;
      reason_out = ROUTER_TIMEOUT;
    } else if (onset_trigger) {
      // Tunable chance when onset is strong, to avoid thrashing
      if (random(100) < g_router_onset_prob_percent) {
        do_pair_transition = true;
        reason_out = ROUTER_ONSET;
      }
    }
  }

  if (do_pair_transition) {
    // Advance to next complementary pair deterministically
    state.current_pair = (state.current_pair + 1) % NUM_COMPLEMENTARY_PAIRS;
    uint8_t cd_min = g_router_cooldown_min_beats;
    uint8_t cd_max = g_router_cooldown_max_beats;
    if (cd_min > cd_max) { uint8_t t = cd_min; cd_min = cd_max; cd_max = t; }
    uint8_t span = (cd_max > cd_min) ? (cd_max - cd_min + 1) : 1;
    state.cooldown_remaining = cd_min + (random(span));
    state.dwell_start = now_ms;
    state.variation_pending = true; // allow a variation on transition
    // Cadence accounting
    if (state.cadence_transitions < 0xFFFF) state.cadence_transitions++;
  }

  // Construct CouplingPlan for this frame
  const uint8_t* pair = COMPLEMENTARY_PAIRS[state.current_pair];
  plan.primary_mode = pair[0];
  plan.secondary_mode = pair[1];
  plan.intensity_balance = compute_balance(audio_vu);

  // Variations: consume if pending, otherwise minimal gentle drift
  if (state.variation_pending) {
    OperatorType op = pick_variation_op();
    switch (op) {
      case OP_ANTI_PHASE:
        plan.anti_phase = true;
        plan.phase_offset = SQ15x16(0.5); // half‑cycle opposition
        plan.hue_detune = SQ15x16(0.0);
        plan.variation_type = OP_ANTI_PHASE;
        break;
      case OP_CIRCULATE:
        plan.anti_phase = false;
        plan.phase_offset = random_phase_offset();
        plan.hue_detune = SQ15x16(0.0);
        plan.variation_type = OP_CIRCULATE;
        break;
      case OP_HUE_DETUNE:
      default:
        plan.anti_phase = false;
        plan.phase_offset = SQ15x16(0.0);
        plan.hue_detune = random_hue_detune();
        plan.variation_type = OP_HUE_DETUNE;
        break;
    }
    state.variation_pending = false;
    state.last_variation_ms = now_ms;
    if (state.cadence_variations < 0xFFFF) state.cadence_variations++;
  } else {
    // Default gentle variation to keep visuals alive
    plan.anti_phase = false;
    plan.phase_offset = (random(11) / 100.0);  // 0.00–0.10
    plan.hue_detune = (SQ15x16((int8_t)random(-4, 5)) / SQ15x16(100)); // ±0.04
    plan.variation_type = OP_MIRROR;
  }
}

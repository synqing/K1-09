#pragma once
#include <stdint.h>
#include "audio_config.h"   // chunk_size, freq_bins, audio_sample_rate

/* ============================================================================
   mel_filterbank.h  —  Perceptual spectral post-processing (bin-wise)
   Pipeline (per 64-bin frame, Q16.16 linear magnitudes):
     1) A-weighting  (perceived loudness tilt)
     2) Noise-floor  (per-bin adaptive floor; subtract → clamp≥0)
     3) Soft-knee compression (log-like, LUT-free)
     4) Attack/Release smoothing (dual-tau, per-bin)
   Output: 64-bin Q16.16 processed magnitudes (same length; no API break)
   All state is internal; call melproc_init() once at startup.
   ==========================================================================*/

namespace melproc {

// Call once after Goertzel init (needs bin centres or fs to compute weights)
bool melproc_init();

// In:  bins_q16[freq_bins] (Q16.16 linear, e.g. Goertzel output)
// Out: out_q16[freq_bins]  (Q16.16 linear, perceptually massaged)
void melproc_process64(const int32_t* bins_q16, int32_t* out_q16);

// Optional knobs (call anytime). Values are clamped internally.
void set_attack_release(float attack_hz, float release_hz);   // defaults: 35 Hz / 8 Hz @ ~125 fps
void set_softknee(float knee);                                 // default 0.65 (0..1)
void set_floor_tc(float tc_seconds);                           // default 2.5 s

} // namespace melproc

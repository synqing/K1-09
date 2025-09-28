#pragma once
#include <stdint.h>
#include "audio_config.h"   // freq_bins, audio_sample_rate

/* ============================================================================
   perceptual_map24.h  —  64→24 perceptual mapping (Mel or Bark), triangular FB.
   - Call pmap24::init(scale, fmin_hz, fmax_hz) once (defaults given).
   - Call pmap24::map64to24(in64_q16, out24_q16) per frame.
   Notes:
     * Input is Q16.16 linear (e.g., your processed 64 bins).
     * Output is Q16.16 linear (24 bands).
     * No changes to AudioFrame; VP can compute this on demand.
   ==========================================================================*/

namespace pmap24 {

enum class Scale : uint8_t { Mel = 0, Bark = 1 };

// Build 24 triangular bands between fmin..fmax (Hz) on selected scale.
// Safe defaults: Mel, 30..8000 Hz.
bool init(Scale scale = Scale::Mel, float fmin_hz = 30.0f, float fmax_hz = 8000.0f);

// Map 64-bin Q16.16 → 24-bin Q16.16 using precomputed weights.
void map64to24(const int32_t* in64_q16, int32_t* out24_q16);

// Optional: switch scale at runtime; calls init() internally.
inline bool set_scale(Scale s) { return init(s); }

} // namespace pmap24

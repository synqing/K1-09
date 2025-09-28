#pragma once
#include <stdint.h>
#include "audio_config.h"

/* ===================== Goertzel Backend (R-SPEC) =====================
   - Public API unchanged:
       bool init();
       void compute_bins(const int32_t* q24, int32_t* out_q16);
   - Output format: Q16.16 linear magnitude per bin (0.0 .. ~1.0 -> 0 .. 65535/65536).
   - Windowing: uses g_window_q15 (Hann or Gaussian) pre-initialised elsewhere.
=======================================================================*/

namespace goertzel_backend {

// Precompute twiddles + window float LUT and normalisation.
bool init();

// Compute per-bin magnitudes for one 128-sample frame.
//  in:  q24[chunk_size]  (DC-removed, Q24 PCM, range ~[-2^23, 2^23-1])
// out: out_q16[freq_bins] (Q16.16 linear magnitude; clamp [0 .. 0x0000FFFF])
void compute_bins(const int32_t* q24, int32_t* out_q16);

} // namespace goertzel_backend

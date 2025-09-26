#pragma once
#include <stdint.h>
#include "audio_config.h"

/* ===================== Goertzel Backend =====================
   - persistent per-bin state (static)
   - produces linear magnitudes in Q16.16 for freq_bins
==============================================================*/

namespace goertzel_backend {

bool init(); // precompute coefficients, clear state
void compute_bins(const int32_t* q24, int32_t* out_q16 /*len=freq_bins*/);

} // namespace goertzel_backend

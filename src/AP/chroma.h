#pragma once
#include <stdint.h>
#include "audio_config.h"

namespace chroma_map {

// Build static bin->pitchClass map (0..11)
void init();

// Accumulate chroma[12] from raw_spectral[freq_bins] (Q16.16 linear)
void accumulate(const int32_t* raw_q16, int32_t* chroma_q16 /*len=12*/);

} // namespace chroma_map


/* ===================== End of chroma.h ===================== */

#pragma once
#include <stdint.h>
#include "audio_config.h"

/* Spectral Flux (linear): sum_k max(0, raw[k] - prev[k]) */
namespace flux {

void init();
int32_t compute(const int32_t* raw_q16); // returns Q16.16; updates internal prev[]

} // namespace flux


/* ===================== End of flux.h ===================== */
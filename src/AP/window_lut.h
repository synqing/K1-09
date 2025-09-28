#pragma once
#include <stdint.h>
#include <math.h>
#include "audio_config.h"

/* ========= Window LUT (Q1.15) =========
   - Single global buffer g_window_q15[chunk_size]
   - Two initialisers available:
       - init_gaussian_window(float sigma)
       - init_hann_window()
*/

#ifdef __cplusplus
extern "C" {
#endif

// Single shared buffer (defined in window_lut.cpp)
extern int16_t g_window_q15[chunk_size];  // Q1.15, 0..1 scaled

// Initialise a Gaussian window into g_window_q15 (sigma ~0.4 default)
void init_gaussian_window(float sigma);

// Initialise a Hann window into g_window_q15 (w[n] = 0.5 - 0.5*cos(2πn/(N-1)))
void init_hann_window();

#ifdef __cplusplus
} // extern "C"
#endif

#pragma once
#include <stdint.h>
#include <math.h>
#include "audio_config.h"

/* ========== Gaussian window LUT (Q1.15) – SINGLE DEFINITION IN .CPP ========== */
/* This header only declares the storage and the init function. */

#ifdef __cplusplus
extern "C" {
#endif

// Single shared buffer (defined in window_lut.cpp)
extern int16_t g_window_q15[chunk_size];  // Q1.15

// Initialize Gaussian window into g_window_q15 (sigma ~0.4 default)
void init_gaussian_window(float sigma);

#ifdef __cplusplus
} // extern "C"
#endif


/* ========== End Gaussian window LUT ========== */

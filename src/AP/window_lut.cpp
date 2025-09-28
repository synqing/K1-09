#include "window_lut.h"

// Single, shared definition (one ODR instance)
int16_t g_window_q15[chunk_size] = {0};

static inline int16_t f_to_q15(float x) {
  // clamp to [-1, 0.9999695] to avoid overflow
  if (x <= -1.0f) return -32768;
  if (x >=  0.9999695f) return 32767;
  int32_t v = (int32_t)lrintf(x * 32767.0f);
  if (v > 32767)      v = 32767;
  else if (v < -32768) v = -32768;
  return (int16_t)v;
}

// ========== Gaussian (kept for compatibility) ==========
void init_gaussian_window(float sigma) {
  if (sigma <= 0.0f) sigma = 0.4f;       // sane default
  const float N = (float)chunk_size;
  const float M = (N - 1.0f) * 0.5f;
  for (uint32_t n = 0; n < chunk_size; ++n) {
    const float t = ( ((float)n) - M ) / (sigma * M);
    const float w = expf(-0.5f * t * t); // Gaussian
    g_window_q15[n] = f_to_q15(w);
  }
}

// ========== Hann (new, Q15) ==========
void init_hann_window() {
  // w[n] = 0.5 - 0.5*cos(2π n/(N-1)), n = 0..N-1
  const float N = (float)chunk_size;
  const float denom = (N > 1.0f) ? (N - 1.0f) : 1.0f;

  for (uint32_t n = 0; n < chunk_size; ++n) {
    const float w = 0.5f - 0.5f * cosf((2.0f * (float)M_PI * (float)n) / denom);
    g_window_q15[n] = f_to_q15(w);  // 0..1 mapped to 0..32767
  }
}

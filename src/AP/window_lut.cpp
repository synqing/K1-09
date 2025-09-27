#include "window_lut.h"

// Single, shared definition (one ODR instance)
int16_t g_window_q15[chunk_size] = {0};

static inline int16_t f_to_q15(float x) {
  if (x <= -1.0f) return -32768;
  if (x >=  0.9999695f) return 32767;
  int32_t v = (int32_t)lrintf(x * 32767.0f);
  if (v > 32767) {
    v = 32767;
  } else if (v < -32768) {
    v = -32768;
  }
  return (int16_t)v;
}

void init_gaussian_window(float sigma) {
  if (sigma <= 0.0f) sigma = 0.4f;     // sane default
  const float N = (float)chunk_size;
  const float M = (N - 1.0f) * 0.5f;
  for (uint32_t n = 0; n < chunk_size; ++n) {
    float t = (((float)n) - M) / (sigma * M);
    float w = expf(-0.5f * t * t);     // Gaussian
    g_window_q15[n] = f_to_q15(w);
  }
}

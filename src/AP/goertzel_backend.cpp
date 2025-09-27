#include "goertzel_backend.h"
#include <math.h>
#include "audio_config.h"
#include "window_lut.h"

namespace goertzel_backend {

// persistent state
static float coeff[freq_bins];        // 2*cos(2πf/Fs)
static float norm_scale[freq_bins];   // normalization to map amplitude ~[0,1]
static float s1[freq_bins];
static float s2[freq_bins];

static inline float clamp01(float x){ return x<0.f?0.f:(x>1.f?1.f:x); }

bool init() {
  // precompute coefficients and conservative normalization
  // denom ~= (N/2) * mean(window)
  double wsum = 0.0;
  for (uint32_t n=0;n<chunk_size;++n) wsum += (double)g_window_q15[n] / 32767.0;
  double wmean = wsum / (double)chunk_size;
  double denom = (double)chunk_size * 0.5 * wmean;
  float inv = (denom > 0.0) ? (float)(1.0 / denom) : 1.0f;

  for (uint32_t k = 0; k < freq_bins; ++k) {
    float f = freq_bin_centers_hz[k];
    float w = 2.0f * (float)M_PI * (f / (float)audio_sample_rate);
    coeff[k] = 2.0f * cosf(w);
    norm_scale[k] = inv;
    s1[k] = 0.0f; s2[k] = 0.0f;
  }
  return true;
}

void compute_bins(const int32_t* q24, int32_t* out_q16) {
  // reset accumulators per bin for this block
  for (uint32_t k=0;k<freq_bins;++k) { s1[k]=0.0f; s2[k]=0.0f; }

  for (uint32_t n=0;n<chunk_size;++n) {
    float x = (float)q24[n] / 8388607.0f;          // Q24 -> [-1,1)
    x *= (float)g_window_q15[n] / 32767.0f;        // window
    for (uint32_t k=0;k<freq_bins;++k) {
      float s0 = x + coeff[k]*s1[k] - s2[k];
      s2[k] = s1[k];
      s1[k] = s0;
    }
  }

  for (uint32_t k=0;k<freq_bins;++k) {
    float w = 2.0f * (float)M_PI * (freq_bin_centers_hz[k] / (float)audio_sample_rate);
    float c = cosf(w), s = sinf(w);
    float re = s1[k] - s2[k] * c;
    float im = s2[k] * s;
    float mag = sqrtf(re*re + im*im);
    float lin = clamp01( mag * norm_scale[k] );    // ~[0,1] for FS sine
    int32_t q = (int32_t)(lin * 65536.0f + 0.5f);
    if (q < 0) q = 0; if (q > 65535) q = 65535;
    out_q16[k] = q;
  }
}

} // namespace goertzel_backend
/* ===================== End of goertzel_backend.cpp ===================== */

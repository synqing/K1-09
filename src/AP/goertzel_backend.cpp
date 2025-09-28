#include "goertzel_backend.h"
#include <math.h>
#include <string.h>
#include "audio_config.h"
#include "window_lut.h"

// ===== Attributes (portable fallbacks) =================================
#ifndef DRAM_ATTR
#define DRAM_ATTR
#endif
#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

namespace goertzel_backend {

// ===== Constants ========================================================
static constexpr float kInvQ24 = 1.0f / 8388607.0f;   // Q24 -> [-1,1)
static constexpr float kInvQ15 = 1.0f / 32767.0f;     // Q15 -> [0,1]

// ===== Persistent State (per bin / per frame) ==========================
// Twiddles
static DRAM_ATTR float coeff2[freq_bins];   // 2*cos(w)
static DRAM_ATTR float cosw[freq_bins];     // cos(w)
static DRAM_ATTR float sinw[freq_bins];     // sin(w)
// Goertzel recurrence state
static DRAM_ATTR float s1[freq_bins];
static DRAM_ATTR float s2[freq_bins];
// Window as float (built once from g_window_q15)
static DRAM_ATTR float window_f32[chunk_size];
// Normalisation (scalar): ~1/( (N/2) * mean(window) )
static float norm_scale = 1.0f;

// ===== Helpers ==========================================================
static inline float clamp01(float x) {
  return (x < 0.0f) ? 0.0f : (x > 1.0f ? 1.0f : x);
}

// ===== Init =============================================================
bool init() {
  // Build a float window LUT once from the active Q15 window (Hann or Gaussian).
  // Expectation: init_hann_window() or init_gaussian_window() already called.
  double wsum = 0.0;
  for (uint32_t n = 0; n < chunk_size; ++n) {
    float wf = (float)g_window_q15[n] * kInvQ15;     // [0..1]
    window_f32[n] = wf;
    wsum += (double)wf;
  }
  const double wmean = wsum / (double)chunk_size;
  const double denom = (double)chunk_size * 0.5 * wmean;  // ≈ amplitude gain for FS sine
  norm_scale = (denom > 0.0) ? (float)(1.0 / denom) : 1.0f;

  // Precompute twiddles for each bin center
  for (uint32_t k = 0; k < freq_bins; ++k) {
    const float w = 2.0f * (float)M_PI * (freq_bin_centers_hz[k] / (float)audio_sample_rate);
    cosw[k]   = cosf(w);
    sinw[k]   = sinf(w);
    coeff2[k] = 2.0f * cosw[k];
    s1[k] = 0.0f; s2[k] = 0.0f;  // clear state
  }
  return true;
}

// ===== Compute (single frame) ==========================================
//  - Sample-major loop: stream x[n] to all bins (good cache behaviour).
//  - No trig, no divides in the inner loop.
//  - Output magnitudes in Q16.16 (0..65535 maps ~[0,1)).
void IRAM_ATTR compute_bins(const int32_t* q24, int32_t* out_q16) {
  // reset accumulators per bin for this block
  for (uint32_t k = 0; k < freq_bins; ++k) { s1[k] = 0.0f; s2[k] = 0.0f; }

  // Goertzel recurrence across the frame
  for (uint32_t n = 0; n < chunk_size; ++n) {
    // Q24 -> float, apply window (both pre-scaled)
    const float x = (float)q24[n] * kInvQ24 * window_f32[n];

    for (uint32_t k = 0; k < freq_bins; ++k) {
      const float s0 = x + coeff2[k]*s1[k] - s2[k];
      s2[k] = s1[k];
      s1[k] = s0;
    }
  }

  // Final magnitude per bin, with normalisation
  for (uint32_t k = 0; k < freq_bins; ++k) {
    // Re/Im at bin centre:
    //   Re = s1 - s2*cos(w),  Im = s2*sin(w)
    const float re  = s1[k] - s2[k]*cosw[k];
    const float im  = s2[k]*sinw[k];
    float mag = sqrtf(re*re + im*im);

    // Normalise such that FS sine ~ 1.0 (window-compensated)
    float lin = clamp01(mag * norm_scale);

    // Q16.16 encode (cap at 0x0000FFFF so 1.0-ε fits)
    int32_t q = (int32_t)lrintf(lin * 65536.0f);
    if (q < 0) q = 0;
    if (q > 65535) q = 65535;
    out_q16[k] = q;
  }
}

} // namespace goertzel_backend

#include "mel_filterbank.h"
#include <math.h>
#include <string.h>

// If your audio_config.h already exposes real bin centres (Hz) as freq_bin_centers_hz[],
// we’ll use them. Otherwise we fall back to linear estimate.
#ifndef HAVE_REAL_BIN_CENTERS
#define HAVE_REAL_BIN_CENTERS 1
#endif

namespace melproc {

static float s_aweight[freq_bins];     // A-weight per bin (linear)
static float s_floor[freq_bins];       // adaptive noise floor (linear)
static float s_env [freq_bins];        // AR-smoothed envelope (linear)
static float s_attack_alpha = 0.28f;   // ≈ 35 Hz @ 125 fps
static float s_release_alpha= 0.064f;  // ≈ 8  Hz @ 125 fps
static float s_floor_alpha  = 0.0032f; // ≈ 2.5 s TC at ~125 fps
static float s_softknee     = 0.65f;   // 0..1 (softer→more compression)
static bool  s_inited       = false;

// Helpers ---------------------------------------------------------------
static inline float q16_to_lin(int32_t q) { return (q <= 0) ? 0.0f : ((float)q / 65536.0f); }
static inline int32_t lin_to_q16(float x) {
  if (x <= 0.0f) return 0;
  float y = (x >= 0.99998474f) ? 0.99998474f : x;
  return (int32_t)lrintf(y * 65536.0f);
}

// IEC 61672 A-weighting (approx) ------------------------------------------------
static float aweight_from_hz(float f) {
  if (f <= 0.0f) return 0.0f;
  // constants in Hz
  const double f2 = f*f;
  const double ra_num = (12200.0*12200.0) * f2*f2;
  const double ra_den = (f2 + 20.6*20.6) * sqrt((f2 + 107.7*107.7)*(f2 + 737.9*737.9)) * (f2 + 12200.0*12200.0);
  double Ra = ra_num / (ra_den + 1e-30);
  // convert to dB (A), then to linear
  double A_dB = 2.0 + 20.0 * log10(Ra + 1e-30);
  double A_lin = pow(10.0, A_dB / 20.0);
  if (!isfinite(A_lin) || A_lin < 0.0) A_lin = 0.0;
  return (float)A_lin;
}

// Precompute per-bin A-weight
static void build_aweight() {
  for (int k=0; k<freq_bins; ++k) {
#if HAVE_REAL_BIN_CENTERS
    extern const float freq_bin_centers_hz[freq_bins];
    float f = freq_bin_centers_hz[k];
#else
    // Fallback: spread bins evenly to Nyquist
    float f = (0.5f * (float)audio_sample_rate) * ((float)k / (float)(freq_bins - 1));
#endif
    s_aweight[k] = aweight_from_hz(f);
  }
}

// Public API ------------------------------------------------------------
bool melproc_init() {
  build_aweight();
  for (int k=0; k<freq_bins; ++k) {
    s_floor[k] = 0.0f;
    s_env[k]   = 0.0f;
  }
  s_inited = true;
  return true;
}

void set_attack_release(float attack_hz, float release_hz) {
  // alpha = 1 - exp(-2π f_c / F_update)
  const float F = (float)audio_sample_rate / (float)chunk_size; // ~125 Hz
  if (attack_hz  < 0.1f) attack_hz  = 0.1f;
  if (release_hz < 0.1f) release_hz = 0.1f;
  s_attack_alpha  = 1.0f - expf(-2.0f * (float)M_PI * attack_hz  / F);
  s_release_alpha = 1.0f - expf(-2.0f * (float)M_PI * release_hz / F);
}

void set_softknee(float knee) {
  if (knee < 0.1f) knee = 0.1f;
  if (knee > 0.95f) knee = 0.95f;
  s_softknee = knee;
}

void set_floor_tc(float tc_seconds) {
  if (tc_seconds < 0.5f) tc_seconds = 0.5f;
  if (tc_seconds > 10.0f) tc_seconds = 10.0f;
  const float F = (float)audio_sample_rate / (float)chunk_size; // ~125 Hz
  s_floor_alpha = 1.0f - expf(-1.0f / (tc_seconds * F));
}

void melproc_process64(const int32_t* bins_q16, int32_t* out_q16) {
  if (!s_inited) (void)melproc_init();

  // 1) to linear & A-weight
  float wlin[freq_bins];
  for (int k=0; k<freq_bins; ++k) {
    float x = q16_to_lin(bins_q16[k]);
    wlin[k] = x * s_aweight[k];
  }

  // 2) adaptive floor tracking (slow) then subtract & clamp
  for (int k=0; k<freq_bins; ++k) {
    // slow floor follows the weighted signal
    s_floor[k] += s_floor_alpha * (wlin[k] - s_floor[k]);
    float y = wlin[k] - s_floor[k];
    if (y < 0.0f) y = 0.0f;
    wlin[k] = y;
  }

  // 3) soft-knee compression (y = x / (x + c)) with knee -> 0..1
  const float c = (1.0f - s_softknee) / (s_softknee + 1e-6f); // smaller knee → stronger comp
  for (int k=0; k<freq_bins; ++k) {
    float y = wlin[k];
    float comp = y / (y + c + 1e-9f);
    wlin[k] = comp;
  }

  // 4) attack/release smoothing (dual-tau EMA)
  for (int k=0; k<freq_bins; ++k) {
    float x = wlin[k];
    float y = s_env[k];
    float a = (x > y) ? s_attack_alpha : s_release_alpha;
    y += a * (x - y);
    s_env[k] = y;
    out_q16[k] = lin_to_q16(y);
  }
}

} // namespace melproc

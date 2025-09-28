#include "perceptual_map24.h"
#include <math.h>
#include <string.h>

namespace pmap24 {

static constexpr int K_IN   = freq_bins;   // 64
static constexpr int K_OUT  = 24;
static_assert(K_IN == 64, "This mapper assumes 64 input bins");

// Center freqs
#if defined(HAVE_REAL_BIN_CENTERS) && HAVE_REAL_BIN_CENTERS
extern const float freq_bin_centers_hz[freq_bins];
static inline float in_bin_hz(int k) { return freq_bin_centers_hz[k]; }
#else
static inline float in_bin_hz(int k) {
  // Linear to Nyquist fallback
  return 0.5f * (float)audio_sample_rate * ((float)k / (float)(K_IN - 1));
}
#endif

enum class Scale : uint8_t; // fwd
static Scale   s_scale = Scale::Mel;
static float   s_fmin = 30.0f, s_fmax = 8000.0f;

// weights[o][i]
static float   s_w[K_OUT][K_IN];
static bool    s_ready = false;

// -------- scale conversions --------
static inline float hz2mel(float f){ return 2595.0f * log10f(1.0f + f/700.0f); }
static inline float mel2hz(float m){ return 700.0f * (powf(10.0f, m/2595.0f) - 1.0f); }
static inline float hz2bark(float f){
  // Traunmüller (1990) approximation
  return 26.81f * (f / (1960.0f + f)) - 0.53f;
}
static inline float bark2hz(float b){
  // inverse approx
  float x = b + 0.53f;
  return 1960.0f * x / (26.28f - x);
}

static inline float to_scale(float hz){
  return (s_scale == Scale::Mel) ? hz2mel(hz) : hz2bark(hz);
}
static inline float from_scale(float u){
  return (s_scale == Scale::Mel) ? mel2hz(u) : bark2hz(u);
}

// -------- init: build 24 triangles --------
static void zero_weights(){ memset(s_w, 0, sizeof(s_w)); }

bool init(Scale scale, float fmin_hz, float fmax_hz){
  s_scale = scale;
  s_fmin = fmin_hz; s_fmax = fmax_hz;
  if (s_fmin < 10.0f) s_fmin = 10.0f;
  if (s_fmax > 0.49f * (float)audio_sample_rate) s_fmax = 0.49f * (float)audio_sample_rate;
  if (s_fmin >= s_fmax) s_fmin = 0.5f * s_fmax;

  zero_weights();

  const float umin = to_scale(s_fmin);
  const float umax = to_scale(s_fmax);
  // K_OUT bands need K_OUT+2 boundaries (include guard edges)
  float u[K_OUT + 2];
  for (int i=0;i<K_OUT+2;++i){
    float r = (float)i / (float)(K_OUT + 1);
    u[i] = umin + r * (umax - umin);
  }
  // convert to Hz for clarity (not strictly needed)
  float f[K_OUT + 2];
  for (int i=0;i<K_OUT+2;++i) f[i] = from_scale(u[i]);

  // For each output band j, triangle spans [f[j], f[j+2]] with peak at f[j+1]
  for (int j=0;j<K_OUT;++j){
    float lo = f[j], mid = f[j+1], hi = f[j+2];
    if (lo < 0.0f) lo = 0.0f;
    // For each input bin k, compute triangular weight
    float norm = 0.0f;
    for (int k=0;k<K_IN;++k){
      float fk = in_bin_hz(k);
      float w = 0.0f;
      if (fk >= lo && fk <= mid) {
        w = (fk - lo) / (mid - lo + 1e-9f);
      } else if (fk > mid && fk <= hi) {
        w = (hi - fk) / (hi - mid + 1e-9f);
      }
      if (w < 0.0f) w = 0.0f;
      s_w[j][k] = w;
      norm += w;
    }
    // Normalize triangle so unity response on white spectrum
    if (norm > 1e-9f) {
      float inv = 1.0f / norm;
      for (int k=0;k<K_IN;++k) s_w[j][k] *= inv;
    }
  }

  s_ready = true;
  return true;
}

// -------- map 64→24 --------
static inline float q16_to_lin(int32_t q) { return (q <= 0) ? 0.0f : ((float)q / 65536.0f); }
static inline int32_t lin_to_q16(float x) {
  if (x <= 0.0f) return 0;
  float y = (x >= 0.99998474f) ? 0.99998474f : x;
  return (int32_t)lrintf(y * 65536.0f);
}

void map64to24(const int32_t* in64_q16, int32_t* out24_q16){
  if (!s_ready) (void)init(); // default Mel 30..8000
  // Convert once to linear floats
  float xin[K_IN];
  for (int k=0;k<K_IN;++k) xin[k] = q16_to_lin(in64_q16[k]);
  // Apply 24×64 matrix
  for (int j=0;j<K_OUT;++j){
    float acc = 0.0f;
    const float* wj = s_w[j];
    for (int k=0;k<K_IN;++k) acc += wj[k] * xin[k];
    out24_q16[j] = lin_to_q16(acc);
  }
}

} // namespace pmap24

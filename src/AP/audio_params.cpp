#include "audio_params.h"

#include <math.h>

#include "audio_config.h"
#include "storage/NVS.h"

namespace audio_params {

static uint32_t g_alpha_q16 = ema_alpha_q16_default;

static inline uint32_t clamp_q16(uint32_t v) {
  if (v < (uint32_t)ema_alpha_q16_min) return (uint32_t)ema_alpha_q16_min;
  if (v > (uint32_t)ema_alpha_q16_max) return (uint32_t)ema_alpha_q16_max;
  return v;
}

void init() {
  if (!storage::nvs::init()) {
    g_alpha_q16 = clamp_q16(g_alpha_q16);
    return;
  }
  uint32_t stored = 0;
  if (storage::nvs::read_u32("smoothing_alpha_q16", stored)) {
    g_alpha_q16 = clamp_q16(stored);
  } else {
    g_alpha_q16 = clamp_q16(g_alpha_q16);
    storage::nvs::write_u32_debounced("smoothing_alpha_q16", g_alpha_q16, 0u, true);
  }
}

uint32_t get_smoothing_alpha_q16() {
  return g_alpha_q16;
}

uint32_t set_smoothing_alpha_q16(uint32_t a_q16) {
  g_alpha_q16 = clamp_q16(a_q16);
  storage::nvs::write_u32_debounced("smoothing_alpha_q16", g_alpha_q16, 1000u, false);
  return g_alpha_q16;
}

uint32_t set_smoothing_tau_ms(uint32_t tau_ms) {
  if (tau_ms == 0u) tau_ms = 1u;
  const double frame_dt_ms = (1000.0 * (double)chunk_size) / (double)audio_sample_rate;
  const double alpha = 1.0 - exp(-frame_dt_ms / (double)tau_ms);
  uint32_t a_q16 = 0u;
  if (alpha >= 0.9999695) {
    a_q16 = 65535u;
  } else if (alpha > 0.0) {
    a_q16 = (uint32_t)(alpha * 65536.0 + 0.5);
  }
  return set_smoothing_alpha_q16(a_q16);
}

uint32_t set_smoothing_profile(SmoothProfile profile) {
  double alpha = 0.10;
  switch (profile) {
    case SmoothProfile::Silk:    alpha = 0.08; break;
    case SmoothProfile::Default: alpha = 0.10; break;
    case SmoothProfile::Snappy:  alpha = 0.25; break;
  }
  uint32_t a_q16 = (uint32_t)(alpha * 65536.0 + 0.5);
  return set_smoothing_alpha_q16(a_q16);
}

} // namespace audio_params

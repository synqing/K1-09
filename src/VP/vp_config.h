#pragma once

#include <stdint.h>

namespace vp_config {

struct VPConfig {
  bool use_smoothed_spectrum;   // true: display smooth_spectral; false: raw_spectral
  uint32_t debug_period_ms;     // min ms between VP debug prints
};

void init();
const VPConfig& get();
bool set(const VPConfig& cfg, bool persist = true);

} // namespace vp_config


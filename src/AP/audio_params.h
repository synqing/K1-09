#pragma once
#include <stdint.h>

namespace audio_params {

enum class SmoothProfile : uint8_t {
  Silk,
  Default,
  Snappy
};

uint32_t get_smoothing_alpha_q16();
uint32_t set_smoothing_alpha_q16(uint32_t a_q16);
uint32_t set_smoothing_tau_ms(uint32_t tau_ms);
uint32_t set_smoothing_profile(SmoothProfile profile);
void init();

} // namespace audio_params

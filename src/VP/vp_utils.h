#pragma once

#include <stdint.h>
#include "AP/audio_bus.h"

namespace vp_utils {

inline float q16_to_float(q16 v) {
  return audio_frame_utils::q16_to_float(v);
}

// Clamp helper for debug-friendly prints
template<typename T>
inline T clamp(T v, T lo, T hi) {
  return (v < lo) ? lo : (v > hi ? hi : v);
}

} // namespace vp_utils

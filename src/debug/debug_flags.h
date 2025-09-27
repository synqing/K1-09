#pragma once

#include <stdint.h>

namespace debug_flags {

enum Group : uint32_t {
  kGroupAPInput     = 1u << 0,
  kGroupTempoEnergy = 1u << 1,
  kGroupTempoFlux   = 1u << 2,
  kGroupDCAndDrift  = 1u << 3,
  kGroupVP          = 1u << 4,
};

constexpr uint32_t kAllGroups = kGroupAPInput |
                                kGroupTempoEnergy |
                                kGroupTempoFlux |
                                kGroupDCAndDrift |
                                kGroupVP;

extern volatile uint32_t g_debug_mask;

inline bool enabled(uint32_t bits) {
  return (g_debug_mask & bits) != 0u;
}

inline void toggle(uint32_t bits) {
  g_debug_mask ^= bits;
}

inline void set(uint32_t bits, bool on) {
  if (on) {
    g_debug_mask |= bits;
  } else {
    g_debug_mask &= ~bits;
  }
}

inline void set_mask(uint32_t mask) {
  g_debug_mask = mask & kAllGroups;
}

inline uint32_t mask() {
  return g_debug_mask;
}

} // namespace debug_flags

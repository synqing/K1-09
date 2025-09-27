#pragma once
#include <stdint.h>

namespace levels {

static constexpr double Q24_FS = 8388607.0; // 2^23 - 1

// peak(|x|)/FS in Q16.16
inline int32_t peak_q16_from_q24(const int32_t* q24, uint32_t n) {
  int32_t peak = 0;
  for (uint32_t i = 0; i < n; ++i) {
    int32_t a = q24[i] >= 0 ? q24[i] : -q24[i];
    if (a > peak) peak = a;
  }
  double lin = (double)peak / Q24_FS;
  if (lin > 1.0) lin = 1.0;
  return (int32_t)(lin * 65536.0 + 0.5);
}

// RMS (after per-chunk mean removal)/FS in Q16.16
inline int32_t rms_q16_from_q24(const int32_t* q24, uint32_t n) {
  long double sum = 0.0L, sq = 0.0L;
  for (uint32_t i = 0; i < n; ++i) { sum += q24[i]; sq += (long double)q24[i] * (long double)q24[i]; }
  long double mean = sum / (long double)n;
  long double var  = (sq / (long double)n) - mean * mean;
  if (var < 0) var = 0;
  double rms = sqrt((double)var) / Q24_FS;
  if (rms > 1.0) rms = 1.0;
  return (int32_t)(rms * 65536.0 + 0.5);
}

} // namespace levels
/* ===================== End of levels.h ===================== */
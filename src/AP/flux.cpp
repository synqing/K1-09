#include "flux.h"

namespace flux {

static int32_t prev_q16[freq_bins];

void init() {
  for (uint32_t k=0;k<freq_bins;++k) prev_q16[k] = 0;
}

int32_t compute(const int32_t* raw_q16) {
  int64_t sum = 0;
  for (uint32_t k=0;k<freq_bins;++k) {
    int32_t d = raw_q16[k] - prev_q16[k];
    if (d > 0) sum += d;
    prev_q16[k] = raw_q16[k];
  }
  if (sum > 0x7FFFFFFFLL) sum = 0x7FFFFFFFLL;
  return (int32_t)sum; // already Q16.16
}

} // namespace flux


/* ===================== End of flux.h ===================== */
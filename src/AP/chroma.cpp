#include "chroma.h"
#include <math.h>

namespace chroma_map {

static uint8_t bin_to_pc[freq_bins];

static inline int pitch_class_from_hz(float hz) {
  if (hz <= 0.0f) return 0;
  float n = 12.0f * log2f(hz / 440.0f) + 69.0f; // MIDI note
  int pc = ((int)lroundf(n) % 12 + 12) % 12;    // 0..11
  return pc;
}

void init() {
  for (uint32_t k=0;k<freq_bins;++k) {
    bin_to_pc[k] = (uint8_t)pitch_class_from_hz(freq_bin_centers_hz[k]);
  }
}

void accumulate(const int32_t* raw_q16, int32_t* chroma_q16) {
  for (int i=0;i<12;++i) chroma_q16[i] = 0;
  for (uint32_t k=0;k<freq_bins;++k) {
    int pc = bin_to_pc[k];
    int32_t v = raw_q16[k];
    int64_t acc = (int64_t)chroma_q16[pc] + (int64_t)v;
    if (acc > 0x7FFFFFFFLL) acc = 0x7FFFFFFFLL;
    chroma_q16[pc] = (int32_t)acc;
  }
}

} // namespace chroma_map


/* ===================== End of chroma.h ===================== */
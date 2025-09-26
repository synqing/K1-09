#pragma once
#include <stdint.h>
#include "audio_config.h"
#include "bands.h"

// Q16.16 linear fixed-point
using q16 = int32_t;

/* ===================== AudioFrame (public contract to VP) =====================
   - fixed-size POD, linear domain
   - stable pointer lifetime; VP copies what it needs
   - ONE publish per tick via memcpy, then audio_frame_epoch++
===============================================================================*/
struct AudioFrame {
  uint32_t audio_frame_epoch;           // increments after a full publish
  uint32_t t_ms;                        // producer time (ms since boot)

  // Optional oscilloscope view (Q15 normalized from Q24)
  int16_t  waveform[chunk_size];

  // Levels (linear)
  q16      vu_peak;                     // peak(|x|)/FS in Q16.16
  q16      vu_rms;                      // RMS after mean removal in Q16.16

  // Frequency-domain (linear)
  q16      raw_spectral[freq_bins];     // instantaneous magnitudes
  q16      smooth_spectral[freq_bins];  // EMA magnitudes
  q16      chroma[12];                  // 12 pitch-class energies (linear)

  // Handy band summaries (linear sums of raw_spectral)
  q16      band_low;        // bins 0-10
  q16      band_low_mid;    // bins 11-32
  q16      band_presence;   // bins 33-53
  q16      band_high;       // bins 54-63

  // Spectral Flux (novelty), linear
  q16      flux;

  // ---- Tempo / Beat (linear) ----
  q16      tempo_bpm;        // Q16.16 BPM (e.g., 120.0 -> 120<<16)
  q16      beat_phase;       // Q16.16 cycle fraction [0,1)
  q16      beat_strength;    // Q16.16 [0,1]
  q16      tempo_confidence; // Q16.16 [0,1]
  q16      tempo_silence;    // Q16.16 [0,1]
  uint8_t  beat_flag;        // 0/1 pulse on wrap (phase crosses 1->0)
  uint8_t  tempo_ready;      // 1 when novelty history full & heavy work valid
  uint8_t  _pad_[2];         // deterministic alignment
};

// Global mirror for fast polling
extern volatile uint32_t audio_frame_epoch;

// Lock-free snapshot (stable pointer). VP can read/copy without locks.
const AudioFrame* acquire_spectral_frame();

namespace audio_frame_utils {

inline float q16_to_float(q16 value) {
  return static_cast<float>(value) / 65536.0f;
}

inline float q16_to_bpm(q16 value) {
  return q16_to_float(value);
}

inline float freq_from_bin(uint16_t bin) {
  if (bin >= freq_bins) {
    bin = freq_bins ? static_cast<uint16_t>(freq_bins - 1u) : 0u;
  }
  return freq_bin_centers_hz[bin];
}

inline float band_low_hz(uint8_t band) {
  using Bands = K1Lightwave::Audio::Bands4_64;
  if (band >= Bands::kNumBands) {
    band = Bands::kNumBands ? static_cast<uint8_t>(Bands::kNumBands - 1u) : 0u;
  }
  const uint8_t start = Bands::kBandStart[band];
  return freq_from_bin(start);
}

inline float band_high_hz(uint8_t band) {
  using Bands = K1Lightwave::Audio::Bands4_64;
  if (band >= Bands::kNumBands) {
    band = Bands::kNumBands ? static_cast<uint8_t>(Bands::kNumBands - 1u) : 0u;
  }
  const uint8_t end = Bands::kBandEnd[band];
  const uint8_t idx = (end == 0u) ? 0u : static_cast<uint8_t>(end - 1u);
  return freq_from_bin(idx);
}

inline bool snapshot_audio_frame(AudioFrame& out, uint8_t max_attempts = 3u) {
  const AudioFrame* src = acquire_spectral_frame();
  if (!src) {
    return false;
  }
  for (uint8_t attempt = 0u; attempt < max_attempts; ++attempt) {
    const uint32_t start_epoch = src->audio_frame_epoch;
    out = *src;
    if (start_epoch == out.audio_frame_epoch) {
      return true;
    }
  }
  return false;
}

} // namespace audio_frame_utils

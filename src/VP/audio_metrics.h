#pragma once

#include <array>
#include <stdint.h>

#include "AP/audio_bus.h"

namespace vp {

// Normalised audio metrics consumed by Visual Pipeline effects.
struct AudioMetrics {
  std::array<float, freq_bins> spectrum{};      // Smoothed magnitudes [0, 1]
  std::array<float, 12> chroma{};              // Pitch-class energies [0, 1]

  float band_low = 0.0f;
  float band_low_mid = 0.0f;
  float band_presence = 0.0f;
  float band_high = 0.0f;

  float flux = 0.0f;               // Spectral novelty [0, ~1.2]
  float flux_smoothed = 0.0f;      // EMA of flux for slow modulation

  float tempo_bpm = 0.0f;          // Beats per minute
  float beat_phase = 0.0f;         // Phase [0, 1)
  float beat_strength = 0.0f;      // [0, 1]
  float tempo_confidence = 0.0f;   // [0, 1]
  float tempo_silence = 0.0f;      // [0, 1]
  bool  beat_flag = false;         // Rising edge flag
  bool  tempo_ready = false;       // Tempo engine warmed up

  float vu_peak = 0.0f;            // Peak level [0, 1]
  float vu_rms = 0.0f;             // RMS level [0, 1]

  // Waveform snapshot statistics (normalized to [-1, 1])
  float waveform_peak = 0.0f;      // Maximum sample value
  float waveform_trough = 0.0f;    // Minimum sample value
  float waveform_rms = 0.0f;       // RMS of waveform window
};

}  // namespace vp

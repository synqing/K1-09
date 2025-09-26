#pragma once

#include <array>

#include "constants.h"

namespace lc {

struct AudioMetrics {
  std::array<float, NUM_FREQS> spectrogram{};
  std::array<float, 12> chroma{};
  float silentScale = 1.0f;
  float currentPunch = 0.0f;
  float waveformPeak = 0.0f;
  bool silence = false;
  float brightness = 0.0f;
  float saturation = 0.0f;
};

void update_spectrogram_smoothing();
void update_chromagram_smoothing();

void collect_audio_metrics(AudioMetrics& metrics);

void apply_sb_config_to_lc(const AudioMetrics& metrics);

void set_latest_audio_metrics(const AudioMetrics& metrics);
const AudioMetrics& latest_audio_metrics();

}  // namespace lc


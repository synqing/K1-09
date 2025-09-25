#include "audio_bridge.h"

#include <algorithm>

#include "globals.h"
#include "lc/config/features.h"
#include "lc/core/RuntimeTunables.h"

namespace {

inline float clamp01(float value) {
  return std::clamp(value, 0.0f, 1.0f);
}

}  // namespace

// Externs from LC runtime globals
extern uint8_t brightnessVal;                // NOLINT(readability-identifier-naming)
extern uint8_t fadeAmount;                   // NOLINT(readability-identifier-naming)
extern uint8_t paletteSpeed;                 // NOLINT(readability-identifier-naming)
extern uint16_t fps;                         // NOLINT(readability-identifier-naming)

extern SQ15x16 audio_vu_level;

namespace lc {

void collect_audio_metrics(AudioMetrics& metrics) {
  for (size_t i = 0; i < NUM_FREQS; ++i) {
    metrics.spectrogram[i] = clamp01(static_cast<float>(spectrogram_smooth[i]));
  }

  for (size_t i = 0; i < metrics.chroma.size(); ++i) {
    metrics.chroma[i] = clamp01(static_cast<float>(chromagram_smooth[i]));
  }

  metrics.silentScale = silent_scale;
  metrics.currentPunch = clamp01(static_cast<float>(audio_vu_level));
  metrics.waveformPeak = std::max(max_waveform_val, max_waveform_val_follower);
  metrics.silence = silence;
  metrics.brightness = CONFIG.PHOTONS;
  metrics.saturation = CONFIG.SATURATION;
}

void apply_sb_config_to_lc(const AudioMetrics& metrics) {
  const float targetBrightness = std::max(0.08f, clamp01(metrics.brightness));
  brightnessVal = static_cast<uint8_t>(targetBrightness * 255.0f + 0.5f);

  const float sat = clamp01(metrics.saturation);
  rt_enable_palette_curation = (CONFIG.PALETTE_INDEX > 0);
  rt_curation_green_scale = static_cast<uint8_t>(sat * 255.0f);
  rt_curation_brown_green_scale = static_cast<uint8_t>(sat * 190.0f);

  // Map mood and punch onto LC speed/fade placeholders.
  const float mood = clamp01(CONFIG.MOOD);
  fadeAmount = static_cast<uint8_t>(mood * 200.0f + 20.0f);

  const float punch = clamp01(metrics.currentPunch);
  paletteSpeed = static_cast<uint8_t>(std::max(1.0f, punch * 255.0f));

  // Keep LC FPS in sync with SB frame target (use CONFIG.PRISM_COUNT as placeholder is odd).
  fps = 60;

  // Ensure optional tunables align with silence detection for later effects.
  rt_enable_auto_exposure = !metrics.silence;
  rt_ae_target_luma = static_cast<uint8_t>(brightnessVal);
}

}  // namespace lc

#include "audio_bridge.h"

#include <algorithm>
#include <Arduino.h>

#include "globals.h"
#include "audio/AudioSystem.h"
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

namespace lc {

namespace {
AudioMetrics g_latest_metrics{};
}  // namespace

void update_spectrogram_smoothing() {
  for (uint8_t bin = 0; bin < NUM_FREQS; ++bin) {
    const SQ15x16 note_brightness = spectrogram[bin];

    if (spectrogram_smooth[bin] < note_brightness) {
      const SQ15x16 distance = note_brightness - spectrogram_smooth[bin];
      spectrogram_smooth[bin] += distance * SQ15x16(0.5f);
    } else if (spectrogram_smooth[bin] > note_brightness) {
      const SQ15x16 distance = spectrogram_smooth[bin] - note_brightness;
      spectrogram_smooth[bin] -= distance * SQ15x16(0.25f);
    }
  }
}

void update_chromagram_smoothing() {
  for (SQ15x16& value : chromagram_smooth) {
    value = 0;
  }

  const uint16_t range = CONFIG.CHROMAGRAM_RANGE;
  if (range == 0) {
    return;
  }

  const SQ15x16 range_scale = SQ15x16(static_cast<float>(range) / 12.0f);
  for (uint16_t i = 0; i < range; ++i) {
    SQ15x16 note_magnitude = spectrogram_smooth[i];
    if (note_magnitude > SQ15x16(1.0f)) {
      note_magnitude = SQ15x16(1.0f);
    } else if (note_magnitude < SQ15x16(0.0f)) {
      note_magnitude = SQ15x16(0.0f);
    }

    const uint8_t chroma_bin = i % 12;
    chromagram_smooth[chroma_bin] += note_magnitude / range_scale;
  }

  static SQ15x16 max_peak = SQ15x16(0.001f);
  max_peak *= SQ15x16(0.999f);
  if (max_peak < SQ15x16(0.01f)) {
    max_peak = SQ15x16(0.01f);
  }

  for (SQ15x16& value : chromagram_smooth) {
    if (value > max_peak) {
      const SQ15x16 distance = value - max_peak;
      max_peak += distance * SQ15x16(0.05f);
    }
  }

  const SQ15x16 multiplier = SQ15x16(1.0f) / max_peak;
  for (SQ15x16& value : chromagram_smooth) {
    value *= multiplier;
  }
}

void collect_audio_metrics(AudioMetrics& metrics) {
  const auto& system_metrics = SensoryBridge::Audio::AudioSystem::instance().metrics();

  for (size_t i = 0; i < NUM_FREQS; ++i) {
    metrics.spectrogram[i] = clamp01(system_metrics.spectrogram[i]);
  }

  for (size_t i = 0; i < metrics.chroma.size(); ++i) {
    metrics.chroma[i] = clamp01(system_metrics.chroma[i]);
  }

  metrics.silentScale = clamp01(system_metrics.silentScale);
  metrics.currentPunch = clamp01(system_metrics.vuLevel);
  metrics.waveformPeak = system_metrics.waveformPeak;
  metrics.silence = system_metrics.silence;
  metrics.brightness = clamp01(system_metrics.brightness);
  metrics.saturation = clamp01(system_metrics.saturation);

  set_latest_audio_metrics(metrics);

  if (debug_mode) {
    static uint32_t lastLog = 0;
    const uint32_t now = millis();
    if (now - lastLog > 750) {
      Serial.printf("[LC] punch=%.3f silent=%d silentScale=%.2f peak=%.1f bright=%.0f sat=%.2f\n",
                    metrics.currentPunch,
                    metrics.silence ? 1 : 0,
                    metrics.silentScale,
                    metrics.waveformPeak,
                    metrics.brightness * 255.0f,
                    metrics.saturation);
      Serial.print(F("[LC] spectrum[0..7]="));
      for (size_t i = 0; i < std::min<size_t>(8, metrics.spectrogram.size()); ++i) {
        Serial.print(metrics.spectrogram[i], 3);
        Serial.print(i < 7 ? ' ' : '\n');
      }
      lastLog = now;
    }
  }
}

void apply_sb_config_to_lc(const AudioMetrics& metrics) {
  const float targetBrightness = clamp01(metrics.brightness);
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

void set_latest_audio_metrics(const AudioMetrics& metrics) {
  g_latest_metrics = metrics;
}

const AudioMetrics& latest_audio_metrics() {
  return g_latest_metrics;
}

}  // namespace lc

#include "vp_renderer.h"

#include <Arduino.h>
#include <FastLED.h>

#include <algorithm>
#include <cmath>

#include "AP/audio_bus.h"
#include "debug/debug_flags.h"
#include "effect_registry.h"
#include "frame_context.h"
#include "led_driver.h"
#include "metrics_bridge.h"
#include "palette_manager.h"
#include "tunables.h"
#include "vp_config.h"

namespace vp_renderer {
namespace {

vp::MetricsBridge g_metrics;
vp::PaletteManager g_palettes;
vp::EffectRegistry g_effects;
vp::LedDriver g_driver;
vp::Tunables g_tunables{};
vp::FrameContext g_context{};

float g_flux_env = 0.0f;
float g_beat_env = 0.0f;
uint32_t g_last_debug_ms = 0;
bool g_ready = false;

constexpr float kFluxAlpha = 0.28f;
constexpr float kBeatDecay = 0.84f;
constexpr float kBeatImpulse = 1.0f;
constexpr float kMinSpeed = 0.10f;
constexpr float kMaxSpeed = 5.0f;

inline float clamp01(float v) {
  return std::clamp(v, 0.0f, 1.0f);
}

void update_strip_geometry() {
  const uint16_t length = g_driver.strip_length();
  g_context.strip_length = length;
  if (length >= 2) {
    g_context.center_left = static_cast<uint16_t>(length / 2u - 1u);
    g_context.center_right = static_cast<uint16_t>(g_context.center_left + 1u);
  } else {
    g_context.center_left = 0;
    g_context.center_right = 0;
  }
}

void ensure_init() {
  if (g_ready) {
    return;
  }

  g_palettes.init();
  const uint8_t initial_brightness = 140;
  g_driver.init(initial_brightness);

  g_tunables.brightness = std::clamp(initial_brightness / 255.0f, 0.0f, 1.0f);
  g_tunables.speed = 1.0f;
  g_tunables.saturation = 1.0f;
  g_tunables.sensitivity = 1.0f;
  g_tunables.flux_boost = 0.0f;
  g_tunables.beat_boost = 0.0f;

  g_context.epoch = 0;
  g_context.timestamp_ms = 0;
  g_context.time_seconds = 0.0f;
  g_context.brightness_scalar = g_tunables.brightness;
  g_context.saturation = g_tunables.saturation;
  g_context.palette = &g_palettes.current_palette();
  g_context.palette_blend = 1.0f;

  update_strip_geometry();

  g_ready = true;
}

void update_dynamic_envelopes(const vp_consumer::VPFrame& frame, const vp::AudioMetrics& metrics) {
  (void)frame;

  float flux_src = std::max(metrics.flux_smoothed, metrics.flux);
  g_flux_env = (1.0f - kFluxAlpha) * g_flux_env + kFluxAlpha * flux_src;
  g_tunables.flux_boost = clamp01(g_flux_env * 0.45f);

  if (metrics.beat_flag) {
    g_beat_env = kBeatImpulse;
  } else {
    g_beat_env *= kBeatDecay;
  }
  g_tunables.beat_boost = clamp01(std::max(g_beat_env, metrics.beat_strength) * 0.65f);
}

void prepare_frame_context(const vp_consumer::VPFrame& frame) {
  g_context.epoch = frame.epoch;
  g_context.timestamp_ms = frame.t_ms;
  g_context.time_seconds = static_cast<float>(frame.t_ms) * 0.001f;
  g_context.brightness_scalar = g_tunables.brightness;
  g_context.saturation = g_tunables.saturation;
  const float blend_speed = 0.012f + 0.01f * g_tunables.speed;
  g_context.palette = &g_palettes.update(blend_speed);
  g_context.palette_blend = 1.0f;  // Palette manager handles interpolation internally.
}

void maybe_print_debug(const vp_consumer::VPFrame& frame, const vp::AudioMetrics& metrics) {
  if (!debug_flags::enabled(debug_flags::kGroupVP)) {
    return;
  }

  const uint32_t now = millis();
  const auto& cfg = vp_config::get();
  if ((now - g_last_debug_ms) < cfg.debug_period_ms) {
    return;
  }
  g_last_debug_ms = now;

  const bool use_smooth = cfg.use_smoothed_spectrum;
  const q16* spectrum = use_smooth ? frame.af.smooth_spectral : frame.af.raw_spectral;

  uint16_t peak_bin = 0;
  q16 peak_val = 0;
  for (uint16_t i = 0; i < freq_bins; ++i) {
    if (spectrum[i] > peak_val) {
      peak_val = spectrum[i];
      peak_bin = i;
    }
  }

  const float bpm   = audio_frame_utils::q16_to_bpm(frame.af.tempo_bpm);
  const float phase = audio_frame_utils::q16_to_float(frame.af.beat_phase);
  const float str   = audio_frame_utils::q16_to_float(frame.af.beat_strength);
  const float conf  = audio_frame_utils::q16_to_float(frame.af.tempo_confidence);
  const float sil   = audio_frame_utils::q16_to_float(frame.af.tempo_silence);
  const float flux  = audio_frame_utils::q16_to_float(frame.af.flux);
  const float pk_hz = audio_frame_utils::freq_from_bin(peak_bin);

  const auto& effect = g_effects.current();

  Serial.printf("[vp] epoch=%lu t=%lums bpm=%.1f phase=%.2f conf=%.2f beat=%u str=%.2f sil=%.2f flux=%.3f pkbin=%u@%.0fHz mode=%u '%s'\n",
                static_cast<unsigned long>(frame.epoch),
                static_cast<unsigned long>(frame.t_ms),
                bpm,
                phase,
                conf,
                frame.af.beat_flag ? 1u : 0u,
                str,
                sil,
                flux,
                static_cast<unsigned>(peak_bin),
                pk_hz,
                static_cast<unsigned>(g_effects.index()),
                effect.name());

  Serial.printf("[vp] bands low=%.3f lowmid=%.3f pres=%.3f high=%.3f (%s)\n",
                metrics.band_low,
                metrics.band_low_mid,
                metrics.band_presence,
                metrics.band_high,
                use_smooth ? "smooth" : "raw");
  Serial.printf("[vp] tunables bright=%.2f speed=%.2f beat=%.2f flux=%.2f\n",
                g_tunables.brightness,
                g_tunables.speed,
                g_tunables.beat_boost,
                g_tunables.flux_boost);
}

}  // namespace

void init() {
  ensure_init();
}

void render(const vp_consumer::VPFrame& frame) {
  ensure_init();

  update_strip_geometry();

  vp::AudioMetrics metrics = g_metrics.collect(frame.af);
  update_dynamic_envelopes(frame, metrics);
  prepare_frame_context(frame);

  vp::LedFrame led_frame = g_driver.begin_frame();
  if (!led_frame.strip1) {
    return;
  }

  vp::Effect& effect = g_effects.current();
  effect.render(metrics, g_context, led_frame, g_tunables);
  g_driver.show();

  maybe_print_debug(frame, metrics);
}

void adjust_brightness(int delta) {
  ensure_init();
  int value = static_cast<int>(g_driver.brightness()) + delta;
  value = std::clamp(value, 0, 255);
  g_driver.set_brightness(static_cast<uint8_t>(value));
  g_tunables.brightness = std::clamp(static_cast<float>(value) / 255.0f, 0.0f, 1.0f);
}

void adjust_speed(float factor) {
  ensure_init();
  if (factor <= 0.0f) {
    return;
  }
  g_tunables.speed = std::clamp(g_tunables.speed * factor, kMinSpeed, kMaxSpeed);
}

void next_mode() {
  ensure_init();
  g_effects.next();
}

void prev_mode() {
  ensure_init();
  g_effects.prev();
}

Status status() {
  ensure_init();
  return Status{g_driver.brightness(), g_tunables.speed, g_effects.index()};
}

}  // namespace vp_renderer

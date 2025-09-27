#include "effect_registry.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "FastLED.h"

namespace vp {

namespace {

inline CRGB sample_palette(const CRGBPalette16* palette, uint8_t index, uint8_t value, float saturation) {
  if (!palette) {
    return CHSV(index, static_cast<uint8_t>(saturation * 255.0f), value);
  }
  CRGB color = ColorFromPalette(*palette, index, value, LINEARBLEND);
  if (saturation < 0.999f) {
    float sat = std::clamp(saturation, 0.0f, 1.0f);
    uint8_t target_sat = static_cast<uint8_t>(sat * 255.0f);
    CHSV hsv = rgb2hsv_approximate(color);
    hsv.s = target_sat;
    hsv.v = value;
    color = hsv;
  }
  return color;
}

inline void add_symmetric_pixel(LedFrame& frame, const FrameContext& ctx, uint16_t distance, const CRGB& color) {
  if (!frame.strip1) {
    return;
  }

  if (distance > ctx.center_left || (ctx.center_right + distance) >= ctx.strip_length) {
    return;
  }

  uint16_t left_index = ctx.center_left - distance;
  uint16_t right_index = ctx.center_right + distance;

  frame.strip1[left_index] += color;
  frame.strip1[right_index] += color;
  if (frame.strip2) {
    frame.strip2[left_index] += color;
    frame.strip2[right_index] += color;
  }
}

inline uint8_t scale_value(float value, float brightness_scalar) {
  float scaled = std::clamp(value * brightness_scalar, 0.0f, 1.0f);
  return static_cast<uint8_t>(scaled * 255.0f);
}

class WaveformEffect final : public Effect {
 public:
  const char* name() const override { return "Waveform"; }

  void render(const AudioMetrics& metrics,
              const FrameContext& ctx,
              LedFrame& frame,
              const Tunables& tunables) override {
    const uint16_t length = ctx.strip_length;
    if (length == 0u || !frame.strip1) {
      frame.clear();
      return;
    }

    ensure_buffer(length);

    constexpr float kAmpAlpha = 0.12f;
    const float pos_peak = std::clamp(metrics.waveform_peak, -1.0f, 1.0f);
    const float neg_peak = std::clamp(metrics.waveform_trough, -1.0f, 0.0f);
    float signed_amp = (std::fabs(pos_peak) >= std::fabs(neg_peak)) ? pos_peak : neg_peak;
    signed_amp = std::clamp(signed_amp, -1.0f, 1.0f);

    smoothed_signed_amp_ = smoothed_signed_amp_ * (1.0f - kAmpAlpha) + signed_amp * kAmpAlpha;

    float abs_amp = std::fabs(smoothed_signed_amp_);
    if (abs_amp < 0.05f) {
      smoothed_signed_amp_ = 0.0f;
      abs_amp = 0.0f;
    }

    float fade_scalar = std::clamp(1.0f - 0.10f * abs_amp, 0.0f, 1.0f);
    uint8_t fade8 = static_cast<uint8_t>(fade_scalar * 255.0f);
    for (auto& px : buffer_) {
      px.nscale8_video(fade8);
    }

    for (int i = static_cast<int>(buffer_.size()) - 1; i > 0; --i) {
      buffer_[static_cast<size_t>(i)] = buffer_[static_cast<size_t>(i - 1)];
    }
    buffer_[0] = CRGB::Black;

    const bool palette_active = ctx.palette != nullptr;
    CRGB colour = compute_colour(metrics, ctx, tunables, palette_active);
    if (!(colour.r | colour.g | colour.b)) {
      colour = last_colour_;
      colour.nscale8_video(230);
    } else {
      last_colour_ = colour;
    }

    float overlays = std::clamp(tunables.flux_boost * 0.4f + tunables.beat_boost * 0.5f, 0.0f, 1.0f);
    uint8_t level = static_cast<uint8_t>(std::clamp(tunables.brightness + overlays, 0.0f, 1.0f) * 255.0f);
    colour.nscale8_video(level);

    const float sensitivity = std::max(tunables.sensitivity, 0.1f);
    float motion = smoothed_signed_amp_ * (0.7f / sensitivity);
    motion = std::clamp(motion, -1.0f, 1.0f);

    const int center_index = static_cast<int>(ctx.center_left);
    const float span = static_cast<float>(ctx.center_left + 1u);
    int pos = static_cast<int>(std::round(center_index + motion * span));
    pos = std::clamp(pos, 0, static_cast<int>(buffer_.size()) - 1);

    buffer_[static_cast<size_t>(pos)] = colour;
    int mirror = static_cast<int>(buffer_.size()) - 1 - pos;
    buffer_[static_cast<size_t>(mirror)] = colour;

    std::copy(buffer_.begin(), buffer_.end(), frame.strip1);
    if (frame.strip2) {
      std::copy(buffer_.begin(), buffer_.end(), frame.strip2);
    }
  }

 private:
  void ensure_buffer(uint16_t length) {
    if (buffer_.size() != length) {
      buffer_.assign(length, CRGB::Black);
    }
  }

  CRGB compute_colour(const AudioMetrics& metrics,
                      const FrameContext& ctx,
                      const Tunables& tunables,
                      bool palette_active) {
    size_t best_index = metrics.chroma.size();
    float best_value = 0.0f;
    for (size_t i = 0; i < metrics.chroma.size(); ++i) {
      float bin = std::clamp(metrics.chroma[i], 0.0f, 1.0f);
      if (bin > best_value) {
        best_value = bin;
        best_index = i;
      }
    }

    CRGB colour;
    if (best_index == metrics.chroma.size() || best_value <= 0.01f) {
      uint8_t idx = static_cast<uint8_t>(std::fmod(ctx.time_seconds * 42.0f, 256.0f));
      uint8_t value = static_cast<uint8_t>(std::clamp(tunables.brightness * 200.0f, 16.0f, 220.0f));
      colour = sample_palette(ctx.palette, idx, value, tunables.saturation);
    } else {
      float bright = std::pow(best_value, 1.25f);
      bright = std::clamp(bright, 0.12f, 1.0f);
      if (tunables.brightness < 0.25f) {
        bright *= 0.75f;
      }

      uint8_t palette_index = static_cast<uint8_t>((best_index * 256u) / metrics.chroma.size());
      uint8_t value = scale_value(bright, std::max(tunables.brightness, 0.25f));
      colour = sample_palette(ctx.palette, palette_index, value, tunables.saturation);
    }

    if (palette_active) {
      const uint8_t palette_floor = static_cast<uint8_t>(0.20f * 255.0f);
      uint8_t max_component = std::max({colour.r, colour.g, colour.b});
      if (max_component < palette_floor) {
        if (max_component > 0) {
          float scale = static_cast<float>(palette_floor) / static_cast<float>(max_component);
          colour.r = static_cast<uint8_t>(std::min(255.0f, colour.r * scale));
          colour.g = static_cast<uint8_t>(std::min(255.0f, colour.g * scale));
          colour.b = static_cast<uint8_t>(std::min(255.0f, colour.b * scale));
        } else {
          colour = sample_palette(ctx.palette, 0, palette_floor, tunables.saturation);
        }
      }
    }

    return colour;
  }

  std::vector<CRGB> buffer_{};
  float smoothed_signed_amp_ = 0.0f;
  CRGB last_colour_{0, 0, 0};
};

inline float clamp01(float v) {
  return std::clamp(v, 0.0f, 1.0f);
}

inline float loudness_gate(const AudioMetrics& metrics) {
  const float silence_component = clamp01(1.0f - metrics.tempo_silence);
  const float vu_component = clamp01((metrics.vu_peak - 0.08f) * 1.6f);
  return clamp01(std::max(silence_component, vu_component));
}

inline float map_band_energy(const AudioMetrics& metrics, float raw) {
  const float loudness = loudness_gate(metrics);
  const float noise_floor = 0.32f + 0.38f * (1.0f - loudness);
  float lifted = std::max(0.0f, raw - noise_floor);
  float gain = 1.55f + 1.45f * loudness;
  float shaped = std::pow(lifted * gain, 1.22f);
  return clamp01(shaped);
}

inline float overall_intensity(const AudioMetrics& metrics, const Tunables& tunables) {
  float mix = metrics.vu_peak * 1.8f + metrics.flux * 0.9f + metrics.flux_smoothed * 0.6f + tunables.flux_boost * 0.8f;
  return clamp01(mix * tunables.sensitivity);
}

class BandSegmentsEffect final : public Effect {
 public:
  const char* name() const override { return "Band Segments"; }

  void render(const AudioMetrics& metrics,
              const FrameContext& ctx,
              LedFrame& frame,
              const Tunables& tunables) override {
    frame.clear();
    const CRGBPalette16* palette = ctx.palette;
    const float energies[4] = {
        clamp01(metrics.band_low),
        clamp01(metrics.band_low_mid),
        clamp01(metrics.band_presence),
        clamp01(metrics.band_high),
    };

    const uint8_t palette_steps[4] = {0, 64, 128, 192};
    const uint16_t radius = static_cast<uint16_t>(ctx.center_left + 1u);
    if (radius == 0u) {
      return;
    }
    const float overall = overall_intensity(metrics, tunables);
    const uint16_t segment = std::max<uint16_t>(1u, static_cast<uint16_t>(radius / 4u));
    for (uint8_t band = 0; band < 4; ++band) {
      float base = map_band_energy(metrics, energies[band]);
      float overlays = clamp01(0.55f * (tunables.flux_boost + tunables.beat_boost));
      float energy = clamp01((base + overlays) * clamp01(0.25f + 1.25f * overall));
      uint16_t target_len = static_cast<uint16_t>(energy * static_cast<float>(segment));
      uint16_t band_start = static_cast<uint16_t>(band) * segment;
      if (band_start >= radius) {
        continue;
      }
      uint16_t band_end = static_cast<uint16_t>(std::min<uint32_t>(band_start + segment, radius));
      uint16_t max_fill = static_cast<uint16_t>(std::min<uint32_t>(band_start + target_len, band_end));
      for (uint16_t distance = band_start; distance < max_fill; ++distance) {
        uint16_t distance_in_band = static_cast<uint16_t>(distance - band_start);
        uint8_t palette_idx = static_cast<uint8_t>(palette_steps[band] +
            (distance_in_band * 255u) / std::max<uint16_t>(1u, static_cast<uint16_t>(band_end - band_start)));
        float channel_gain = clamp01(0.35f + 0.9f * overall);
        uint8_t value = scale_value(energy * channel_gain, tunables.brightness);
        CRGB color = sample_palette(palette, palette_idx, value, tunables.saturation);
        add_symmetric_pixel(frame, ctx, distance, color);
      }
    }
  }
};

class CenterWaveEffect final : public Effect {
 public:
  explicit CenterWaveEffect(bool outward) : outward_(outward) {}

  const char* name() const override { return outward_ ? "Center Wave" : "Edge Wave"; }

  void render(const AudioMetrics& metrics,
              const FrameContext& ctx,
              LedFrame& frame,
              const Tunables& tunables) override {
    frame.clear();
    const CRGBPalette16* palette = ctx.palette;
    const uint16_t radius = static_cast<uint16_t>(ctx.center_left + 1u);
    if (radius == 0u) {
      return;
    }
    float time = ctx.time_seconds * tunables.speed;
    const float loudness = loudness_gate(metrics);
    const float overall = overall_intensity(metrics, tunables);
    const float two_pi = 6.2831853f;

    for (uint16_t distance = 0; distance < radius; ++distance) {
      float norm = static_cast<float>(distance) / static_cast<float>(radius);
      float phase = outward_ ? norm : (1.0f - norm);
      float arg = two_pi * (phase + time);
      float s = 0.5f + 0.5f * sinf(arg);
      s = std::pow(std::clamp(s, 0.0f, 1.0f), 1.6f);
      float motion = s * clamp01(0.25f + 1.6f * overall);
      float overlays = metrics.beat_strength * (0.25f + 0.55f * loudness) +
                       metrics.flux_smoothed * (0.18f + 0.4f * loudness) +
                       tunables.flux_boost * 0.35f + tunables.beat_boost * 0.32f;
      float energy = clamp01(motion + overlays);
      uint8_t palette_idx = static_cast<uint8_t>(norm * 255.0f);
      float channel_gain = clamp01(0.35f + 0.7f * overall);
      uint8_t value = scale_value(energy * channel_gain, tunables.brightness);
      CRGB color = sample_palette(palette, palette_idx, value, tunables.saturation);
      add_symmetric_pixel(frame, ctx, distance, color);
    }
  }

 private:
  bool outward_ = true;
};

class CenterPulseEffect final : public Effect {
 public:
  const char* name() const override { return "Center Pulse"; }

  void render(const AudioMetrics& metrics,
              const FrameContext& ctx,
              LedFrame& frame,
              const Tunables& tunables) override {
    frame.clear();
    const CRGBPalette16* palette = ctx.palette;
    const uint16_t radius = static_cast<uint16_t>(ctx.center_left + 1u);
    if (radius == 0u) {
      return;
    }
    float cycle = fmodf(ctx.time_seconds * tunables.speed, 1.0f);
    float center = cycle * static_cast<float>(radius);
    float sigma = static_cast<float>(radius) * 0.18f;
    if (sigma < 1.0f) sigma = 1.0f;
    const float loudness = loudness_gate(metrics);
    const float overall = overall_intensity(metrics, tunables);

    for (uint16_t distance = 0; distance < radius; ++distance) {
      float x = static_cast<float>(distance) - center;
      float gauss = std::exp(-(x * x) / (2.0f * sigma * sigma));
      float base = gauss * clamp01(0.25f + 1.8f * overall);
      float overlays = metrics.beat_strength * (0.4f + 0.6f * loudness) +
                       metrics.flux_smoothed * (0.25f + 0.4f * loudness) +
                       tunables.beat_boost * (0.3f + 0.25f * overall);
      float energy = clamp01(base + overlays);
      uint16_t denom = std::max<uint16_t>(1u, radius - 1u);
      uint8_t palette_idx = static_cast<uint8_t>((distance * 255u) / denom);
      float channel_gain = clamp01(0.4f + 0.7f * overall);
      uint8_t value = scale_value(energy * channel_gain, tunables.brightness);
      CRGB color = sample_palette(palette, palette_idx, value, tunables.saturation);
      add_symmetric_pixel(frame, ctx, distance, color);
    }
  }
};

class BilateralCometsEffect final : public Effect {
 public:
  const char* name() const override { return "Bilateral Comets"; }

  void render(const AudioMetrics& metrics,
              const FrameContext& ctx,
              LedFrame& frame,
              const Tunables& tunables) override {
    frame.clear();
    const CRGBPalette16* palette = ctx.palette;
    const uint16_t radius = static_cast<uint16_t>(ctx.center_left + 1u);
    if (radius == 0u) {
      return;
    }
    const float loudness = loudness_gate(metrics);
    float head_pos = fmodf(ctx.time_seconds * tunables.speed, 1.0f) * static_cast<float>(radius);
    uint16_t head_idx = static_cast<uint16_t>(head_pos);
    const uint16_t tail = std::max<uint16_t>(8u, static_cast<uint16_t>(radius / 4u));

    for (uint16_t offset = 0; offset <= head_idx; ++offset) {
      uint16_t distance = head_idx - offset;
      if (distance > ctx.center_left) break;
      float fade = 1.0f - (static_cast<float>(offset) / static_cast<float>(tail));
      if (fade < 0.0f) break;
      float lift = metrics.flux_smoothed * (0.25f + 0.45f * loudness) +
                   metrics.beat_strength * (0.3f + 0.5f * loudness) +
                   tunables.flux_boost * 0.35f;
      float energy = clamp01(fade * (0.35f + 0.75f * loudness) + lift);
      uint16_t denom = std::max<uint16_t>(1u, radius - 1u);
      uint8_t palette_idx = static_cast<uint8_t>((distance * 255u) / denom);
      float channel_gain = clamp01(0.35f + 0.7f * loudness);
      uint8_t value = scale_value(energy * channel_gain, tunables.brightness);
      CRGB color = sample_palette(palette, palette_idx, value, tunables.saturation);
      add_symmetric_pixel(frame, ctx, distance, color);
    }
  }
};

class FluxSparklesEffect final : public Effect {
 public:
  const char* name() const override { return "Flux Sparkles"; }

  void render(const AudioMetrics& metrics,
              const FrameContext& ctx,
              LedFrame& frame,
              const Tunables& tunables) override {
    if (!initialized_) {
      sparkles_.resize(28);
      initialized_ = true;
    }

    frame.clear();
    const uint16_t radius = static_cast<uint16_t>(ctx.center_left + 1u);
    if (radius == 0u) {
      return;
    }

    const float loudness = loudness_gate(metrics);

    // Decay existing sparkles faster when audio is quiet.
    for (auto& spark : sparkles_) {
      if (!spark.active) continue;
      spark.value -= 0.06f + 0.10f * (1.0f - loudness);
      if (spark.value <= 0.02f) {
        spark.active = false;
      }
    }

    // Spawn new sparkles proportional to flux and loudness.
    float spawn_rate = metrics.flux * (6.0f + 6.0f * loudness) + loudness * 2.8f + tunables.flux_boost * 4.0f;
    spawn_rate = std::clamp(spawn_rate, 0.0f, 10.0f);
    uint8_t spawns = static_cast<uint8_t>(spawn_rate);
    float fractional = spawn_rate - static_cast<float>(spawns);
    if (fractional > 0.0f && random8() < static_cast<uint8_t>(fractional * 255.0f)) {
      spawns++;
    }

    for (uint8_t i = 0; i < spawns; ++i) {
      auto it = std::find_if(sparkles_.begin(), sparkles_.end(), [](const Sparkle& s) { return !s.active; });
      if (it == sparkles_.end()) break;
      it->distance = random16(radius);
      float base = clamp01(metrics.vu_peak * 1.1f + loudness * 0.6f);
      it->value = 0.25f + base;
      it->hue = random8();
      it->active = true;
    }

    const CRGBPalette16* palette = ctx.palette;
    for (const auto& spark : sparkles_) {
      if (!spark.active) continue;
      float energy = clamp01(spark.value + metrics.beat_strength * (0.25f + 0.4f * loudness));
      float channel_gain = clamp01(0.35f + 0.7f * loudness);
      uint8_t value = scale_value(energy * channel_gain, tunables.brightness);
      CRGB color = sample_palette(palette, spark.hue, value, tunables.saturation);
      add_symmetric_pixel(frame, ctx, spark.distance, color);
    }
  }

 private:
  struct Sparkle {
    uint16_t distance = 0;
    float value = 0.0f;
    uint8_t hue = 0;
    bool active = false;
  };

  bool initialized_ = false;
  std::vector<Sparkle> sparkles_;
};

class BeatStrobeEffect final : public Effect {
 public:
  const char* name() const override { return "Beat Strobe"; }

  void render(const AudioMetrics& metrics,
              const FrameContext& ctx,
              LedFrame& frame,
              const Tunables& tunables) override {
    frame.clear();
    const uint16_t radius = static_cast<uint16_t>(ctx.center_left + 1u);
    if (radius == 0u) {
      return;
    }
    const float loudness = loudness_gate(metrics);
    float envelope = clamp01((metrics.beat_strength + tunables.beat_boost) * (0.6f + 0.8f * loudness));
    uint16_t half = static_cast<uint16_t>(envelope * std::max<uint16_t>(1u, radius));
    const CRGBPalette16* palette = ctx.palette;
    uint8_t base_hue = static_cast<uint8_t>(metrics.beat_phase * 255.0f);

    for (uint16_t distance = 0; distance <= ctx.center_left; ++distance) {
      if (distance > half) break;
      float falloff = 1.0f - (static_cast<float>(distance) / static_cast<float>(half + 1));
      float energy = clamp01(falloff * (0.4f + 0.8f * loudness) + metrics.flux_smoothed * (0.15f + 0.35f * loudness));
      float channel_gain = clamp01(0.45f + 0.7f * loudness);
      uint8_t hue = base_hue + static_cast<uint8_t>((distance * 10) & 0xFF);
      uint8_t value = scale_value(energy * channel_gain, tunables.brightness);
      CRGB color = sample_palette(palette, hue, value, tunables.saturation);
      add_symmetric_pixel(frame, ctx, distance, color);
    }
  }
};

}  // namespace

EffectRegistry::EffectRegistry() {
  register_default_effects();
}

Effect& EffectRegistry::current() {
  return *effects_[current_index_ % effects_.size()];
}

const Effect& EffectRegistry::current() const {
  return *effects_[current_index_ % effects_.size()];
}

void EffectRegistry::next() {
  if (effects_.empty()) return;
  current_index_ = static_cast<uint8_t>((current_index_ + 1) % effects_.size());
}

void EffectRegistry::prev() {
  if (effects_.empty()) return;
  current_index_ = static_cast<uint8_t>((current_index_ + effects_.size() - 1) % effects_.size());
}

void EffectRegistry::set(uint8_t index) {
  if (effects_.empty()) return;
  current_index_ = static_cast<uint8_t>(index % effects_.size());
}

void EffectRegistry::register_default_effects() {
  effects_.clear();
  effects_.push_back(std::make_unique<WaveformEffect>());
  effects_.push_back(std::make_unique<BandSegmentsEffect>());
  effects_.push_back(std::make_unique<CenterWaveEffect>(true));
  effects_.push_back(std::make_unique<CenterWaveEffect>(false));
  effects_.push_back(std::make_unique<CenterPulseEffect>());
  effects_.push_back(std::make_unique<BilateralCometsEffect>());
  effects_.push_back(std::make_unique<FluxSparklesEffect>());
  effects_.push_back(std::make_unique<BeatStrobeEffect>());
}

}  // namespace vp

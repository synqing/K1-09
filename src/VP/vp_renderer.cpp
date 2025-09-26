#include "vp_renderer.h"

#ifndef FASTLED_INTERNAL
#define FASTLED_INTERNAL
#endif

#include <Arduino.h>
#include <FastLED.h>
#include <cmath>
#include "vp_config.h"
#include "vp_utils.h"
#include "debug/debug_flags.h"

namespace vp_renderer {

namespace {

constexpr uint8_t kPrimaryDataPin = 9;    // GPIO 9 (Strip 1)
constexpr uint8_t kSecondaryDataPin = 10; // GPIO 10 (Strip 2)
constexpr uint16_t kStrip1Leds = 160;
constexpr uint16_t kStrip2Leds = 160;
constexpr uint16_t kLedCount = kStrip1Leds + kStrip2Leds;
constexpr uint8_t kBaseBrightness = 140;  // out of 255

static_assert(kStrip1Leds > 0, "Strip 1 must have LEDs defined");
static_assert(kLedCount > 0, "Total LED count must be positive");

CRGB g_leds[kLedCount];
bool g_hw_ready = false;

uint32_t g_last_debug_ms = 0;
float g_band_env[4] = {0.f, 0.f, 0.f, 0.f};
float g_flux_env = 0.0f;
float g_beat_env = 0.0f;
uint8_t g_brightness = kBaseBrightness;
float   g_speed = 1.0f;   // multiplier on time-based animations
uint8_t g_mode = 0;       // 0=band segments, 1=center wave, 2=edge-in wave, 3=center pulse, 4=comets, 5=flux sparkles, 6=beat strobe

// Simple, non-rainbow palette (blue-magenta-pink tones)
static const CRGBPalette16 kPalette = CRGBPalette16(
  CRGB(  0,  0, 32), CRGB( 10,  0, 64), CRGB( 32,  0, 96), CRGB( 64,  0,128),
  CRGB( 96,  0,160), CRGB(128,  0,200), CRGB(160,  0,240), CRGB(200,  0,255),
  CRGB(200,  0,255), CRGB(160,  0,240), CRGB(128,  0,200), CRGB( 96,  0,160),
  CRGB( 64,  0,128), CRGB( 32,  0, 96), CRGB( 10,  0, 64), CRGB(  0,  0, 32)
);

constexpr uint16_t kSegmentLength = kLedCount / 4u;

inline uint16_t segment_length(uint8_t /*segment_index*/) {
  return kSegmentLength;
}

inline void clear_all() {
  fill_solid(g_leds, kLedCount, CRGB::Black);
}

// Draw center-origin bands on one strip (len=160), mirrored about indices 79/80
// Radius = 80 per side, split into 4 equal radial segments of 20px per band.
void render_center_bands_strip(CRGB* strip, const float band_env[4], float beat_add, float flux_add) {
  constexpr uint16_t kStripLen = kStrip1Leds;     // 160
  constexpr uint16_t kCenterL = 79;               // left center index
  constexpr uint16_t kCenterR = 80;               // right center index
  constexpr uint16_t kRadius  = 80;               // pixels per side
  constexpr uint16_t kSegLen  = kRadius / 4;      // 20 per band
  constexpr uint8_t  kBandHues[4] = {0, 32, 96, 160};

  for (uint8_t b = 0; b < 4; ++b) {
    const float gamma = std::sqrt(vp_utils::clamp(band_env[b], 0.0f, 1.0f));
    const float value_f = vp_utils::clamp(gamma + beat_add + flux_add, 0.0f, 1.0f);
    const uint8_t value = static_cast<uint8_t>(value_f * 255.0f);
    const CHSV hsv(kBandHues[b], 240, value);
    const CRGB rgb = hsv;

    const uint16_t seg_start = static_cast<uint16_t>(b) * kSegLen;      // 0,20,40,60 from center
    // Length within this band segment based on band energy
    const uint16_t len = static_cast<uint16_t>(vp_utils::clamp(band_env[b], 0.0f, 1.0f) * kSegLen + 0.5f);

    for (uint16_t i = 0; i < len; ++i) {
      const int32_t off = static_cast<int32_t>(seg_start + i);
      const int32_t li = static_cast<int32_t>(kCenterL) - off;
      const int32_t ri = static_cast<int32_t>(kCenterR) + off;
      if (li >= 0 && li < kStripLen) strip[li] = rgb;
      if (ri >= 0 && ri < kStripLen) strip[ri] = rgb;
    }
  }
}

// Center-origin travelling wave (outward) using palette; speed controls phase velocity.
void render_center_wave_strip(CRGB* strip, float phase01, bool outward) {
  constexpr uint16_t kStripLen = kStrip1Leds;     // 160
  constexpr uint16_t kCenterL = 79;
  constexpr uint16_t kCenterR = 80;
  constexpr uint16_t kRadius  = 80;
  const float two_pi = 6.2831853f;
  // wave length ~ radius/2; advance with time
  for (uint16_t d = 0; d < kRadius; ++d) {
    float pos = outward ? (float)d : (float)(kRadius - 1 - d);
    float arg = two_pi * (phase01 - pos / (float)(kRadius * 0.75f));
    float s = 0.5f + 0.5f * sinf(arg);
    uint8_t v = static_cast<uint8_t>(s * 255.0f);
    // Subtle palette hue shift across radius, no rainbow
    uint8_t idx = static_cast<uint8_t>(16 + (pos * 2) ) & 0xFF;
    CRGB c = ColorFromPalette(kPalette, idx, v, LINEARBLEND);
    int32_t li = (int32_t)kCenterL - (int32_t)d;
    int32_t ri = (int32_t)kCenterR + (int32_t)d;
    if (li >= 0 && li < kStripLen) strip[li] = c;
    if (ri >= 0 && ri < kStripLen) strip[ri] = c;
  }
}

// Center-origin radial pulse propagating outward (or inward if outward=false)
void render_center_pulse_strip(CRGB* strip, float phase01, bool outward) {
  constexpr uint16_t kStripLen = kStrip1Leds;
  constexpr uint16_t kCenterL = 79, kCenterR = 80, kRadius = 80;
  // position of pulse center in [0, kRadius)
  float pos = (outward ? phase01 : (1.0f - phase01));
  float ring = pos * (float)(kRadius - 1);
  const float sigma = 6.0f; // width of pulse
  for (uint16_t d = 0; d < kRadius; ++d) {
    float x = (float)d - ring;
    float gauss = expf(-(x*x) / (2.0f * sigma * sigma));
    uint8_t v = (uint8_t)(gauss * 255.0f);
    CRGB c = ColorFromPalette(kPalette, (uint8_t)(32 + d), v, LINEARBLEND);
    int32_t li = (int32_t)kCenterL - (int32_t)d;
    int32_t ri = (int32_t)kCenterR + (int32_t)d;
    if (li >= 0 && li < kStripLen) strip[li] = c;
    if (ri >= 0 && ri < kStripLen) strip[ri] = c;
  }
}

// Bilateral comets moving outward from center with fading tails
void render_bilateral_comets_strip(CRGB* strip, float phase01) {
  constexpr uint16_t kStripLen = kStrip1Leds;
  constexpr uint16_t kCenterL = 79, kCenterR = 80, kRadius = 80;
  constexpr uint16_t kTail = 18;
  // head distance from center
  uint16_t head = (uint16_t)(phase01 * (float)(kRadius - 1));
  for (uint16_t d = 0; d <= head; ++d) {
    uint16_t t = (head - d);
    if (t > kTail) break;
    uint8_t v = (uint8_t)((255 * (kTail - t)) / kTail);
    CRGB c = ColorFromPalette(kPalette, (uint8_t)(48 + d), v, LINEARBLEND);
    int32_t li = (int32_t)kCenterL - (int32_t)d;
    int32_t ri = (int32_t)kCenterR + (int32_t)d;
    if (li >= 0 && li < kStripLen) strip[li] = c;
    if (ri >= 0 && ri < kStripLen) strip[ri] = c;
  }
}

// Flux-driven sparkles (symmetric). Keeps a small pool of sparkles that decay.
struct Spark { uint8_t d; uint8_t v; bool live; };
constexpr uint8_t kMaxSparkles = 20;
Spark g_spark[kMaxSparkles] = {};

inline void decay_sparkles() {
  for (auto &s : g_spark) {
    if (!s.live) continue;
    s.v = (uint8_t)((uint16_t)s.v * 200u / 256u); // ~0.78 decay
    if (s.v < 8) s.live = false;
  }
}

inline void spawn_sparkle(uint8_t d, uint8_t v) {
  for (auto &s : g_spark) {
    if (!s.live) { s = Spark{d, v, true}; return; }
  }
}

void render_flux_sparkles_strip(CRGB* strip) {
  constexpr uint16_t kStripLen = kStrip1Leds;
  constexpr uint16_t kCenterL = 79, kCenterR = 80;
  for (const auto &s : g_spark) {
    if (!s.live) continue;
    CRGB c = ColorFromPalette(kPalette, (uint8_t)(64 + s.d), s.v, LINEARBLEND);
    int32_t li = (int32_t)kCenterL - (int32_t)s.d;
    int32_t ri = (int32_t)kCenterR + (int32_t)s.d;
    if (li >= 0 && li < kStripLen) strip[li] = c;
    if (ri >= 0 && ri < kStripLen) strip[ri] = c;
  }
}

// Beat strobe around the center, width scales with beat envelope
void render_beat_strobe_strip(CRGB* strip, float beat_env) {
  constexpr uint16_t kStripLen = kStrip1Leds;
  constexpr uint16_t kCenterL = 79, kCenterR = 80;
  uint16_t half = (uint16_t)(1 + beat_env * 10.0f);
  if (half > 20) half = 20;
  for (uint16_t i = 0; i <= half; ++i) {
    uint8_t v = (uint8_t)(255 - (i * 10));
    CRGB c = ColorFromPalette(kPalette, (uint8_t)(96 + i * 4), v, LINEARBLEND);
    int32_t li = (int32_t)kCenterL - (int32_t)i;
    int32_t ri = (int32_t)kCenterR + (int32_t)i;
    if (li >= 0 && li < kStripLen) strip[li] = c;
    if (ri >= 0 && ri < kStripLen) strip[ri] = c;
  }
}

// Update envelopes shared across modes (flux, beat)
void update_envelopes(const vp_consumer::VPFrame& f) {
  constexpr float kFluxAlpha = 0.25f;
  constexpr float kBeatDecay = 0.82f;
  const float flux_lin = vp_utils::clamp(vp_utils::q16_to_float(f.af.flux), 0.0f, 2.0f);
  g_flux_env = (1.0f - kFluxAlpha) * g_flux_env + kFluxAlpha * flux_lin;
  if (f.af.beat_flag) g_beat_env = 1.0f; else g_beat_env *= kBeatDecay;
}

void render_leds(const vp_consumer::VPFrame& f) {
  if (!g_hw_ready) {
    return;
  }

  // Smoothing + envelopes
  constexpr float kBandAlpha = 0.35f;
  constexpr float kFluxAlpha = 0.25f;
  constexpr float kBeatDecay = 0.82f;
  constexpr float kBeatBoost = 0.45f;
  constexpr float kFluxBoost = 0.35f;

  const float band_targets[4] = {
      vp_utils::clamp(vp_utils::q16_to_float(f.af.band_low), 0.0f, 1.5f),
      vp_utils::clamp(vp_utils::q16_to_float(f.af.band_low_mid), 0.0f, 1.5f),
      vp_utils::clamp(vp_utils::q16_to_float(f.af.band_presence), 0.0f, 1.5f),
      vp_utils::clamp(vp_utils::q16_to_float(f.af.band_high), 0.0f, 1.5f)};

  for (uint8_t i = 0; i < 4; ++i) {
    g_band_env[i] = (1.0f - kBandAlpha) * g_band_env[i] + kBandAlpha * band_targets[i];
  }

  const float flux_lin = vp_utils::clamp(vp_utils::q16_to_float(f.af.flux), 0.0f, 2.0f);
  g_flux_env = (1.0f - kFluxAlpha) * g_flux_env + kFluxAlpha * flux_lin;

  if (f.af.beat_flag) {
    g_beat_env = 1.0f;
  } else {
    g_beat_env *= kBeatDecay;
  }

  const float beat_add = kBeatBoost * g_beat_env;
  const float flux_add = kFluxBoost * vp_utils::clamp(g_flux_env, 0.0f, 1.0f);

  // Clear frame, then render both strips identically with center-origin mapping
  clear_all();
  CRGB* strip1 = g_leds;
  CRGB* strip2 = g_leds + kStrip1Leds;
  render_center_bands_strip(strip1, g_band_env, beat_add, flux_add);
  render_center_bands_strip(strip2, g_band_env, beat_add, flux_add);

  FastLED.show();
}

} // namespace

void init() {
  FastLED.addLeds<WS2812B, kPrimaryDataPin, GRB>(g_leds, 0, kStrip1Leds);
  if constexpr (kStrip2Leds > 0) {
    FastLED.addLeds<WS2812B, kSecondaryDataPin, GRB>(g_leds + kStrip1Leds, kStrip2Leds);
  }
  FastLED.setBrightness(g_brightness);
  FastLED.clear(true);
  FastLED.show();
  g_hw_ready = true;
}

void render(const vp_consumer::VPFrame& f) {
  update_envelopes(f);
  const float t = (millis() * 0.001f) * g_speed;  // seconds * speed
  const float phase01 = t - floorf(t);
  const float beat_env = g_beat_env;
  const float flux_env = vp_utils::clamp(g_flux_env, 0.0f, 1.0f);
  // Sparkle spawning rate scales with flux
  if (g_mode == 5) {
    decay_sparkles();
    uint8_t spawns = (uint8_t)(flux_env * 4.0f);
    while (spawns--) {
      uint8_t d = random8(0, 80); // 0..79
      uint8_t v = (uint8_t)(128 + random8(0, 128));
      spawn_sparkle(d, v);
    }
  }
  // Dispatch by lightshow mode
  switch (g_mode) {
    case 0:
      render_leds(f); // band segments
      break;
    case 1: {
      // center-out wave
      fill_solid(g_leds, kLedCount, CRGB::Black);
      CRGB* strip1 = g_leds;
      CRGB* strip2 = g_leds + kStrip1Leds;
      render_center_wave_strip(strip1, phase01, /*outward=*/true);
      render_center_wave_strip(strip2, phase01, /*outward=*/true);
      // beat lifts brightness subtly
      nscale8_video(g_leds, kLedCount, (uint8_t)(220 + 35 * beat_env));
      FastLED.show();
      break;
    }
    case 2: {
      // edge-in wave (invert direction)
      fill_solid(g_leds, kLedCount, CRGB::Black);
      CRGB* strip1 = g_leds;
      CRGB* strip2 = g_leds + kStrip1Leds;
      render_center_wave_strip(strip1, phase01, /*outward=*/false);
      render_center_wave_strip(strip2, phase01, /*outward=*/false);
      nscale8_video(g_leds, kLedCount, (uint8_t)(220 + 35 * beat_env));
      FastLED.show();
      break;
    }
    case 3: {
      // center pulse
      fill_solid(g_leds, kLedCount, CRGB::Black);
      CRGB* strip1 = g_leds;
      CRGB* strip2 = g_leds + kStrip1Leds;
      render_center_pulse_strip(strip1, phase01, /*outward=*/true);
      render_center_pulse_strip(strip2, phase01, /*outward=*/true);
      FastLED.show();
      break;
    }
    case 4: {
      // bilateral comets
      fill_solid(g_leds, kLedCount, CRGB::Black);
      CRGB* strip1 = g_leds;
      CRGB* strip2 = g_leds + kStrip1Leds;
      render_bilateral_comets_strip(strip1, phase01);
      render_bilateral_comets_strip(strip2, phase01);
      FastLED.show();
      break;
    }
    case 5: {
      // flux sparkles
      fadeToBlackBy(g_leds, kLedCount, 32); // soft trail
      CRGB* strip1 = g_leds;
      CRGB* strip2 = g_leds + kStrip1Leds;
      render_flux_sparkles_strip(strip1);
      render_flux_sparkles_strip(strip2);
      FastLED.show();
      break;
    }
    case 6: {
      // beat strobe
      fill_solid(g_leds, kLedCount, CRGB::Black);
      CRGB* strip1 = g_leds;
      CRGB* strip2 = g_leds + kStrip1Leds;
      render_beat_strobe_strip(strip1, beat_env);
      render_beat_strobe_strip(strip2, beat_env);
      FastLED.show();
      break;
    }
    default:
      render_leds(f);
      break;
  }

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
  const q16* spec = use_smooth ? f.af.smooth_spectral : f.af.raw_spectral;

  uint16_t peak_bin = 0;
  q16 peak_val = 0;
  for (uint16_t i = 0; i < freq_bins; ++i) {
    if (spec[i] > peak_val) {
      peak_val = spec[i];
      peak_bin = i;
    }
  }

  const float bpm   = vp_utils::q16_to_float(f.af.tempo_bpm);
  const float phase = vp_utils::q16_to_float(f.af.beat_phase);
  const float str   = vp_utils::q16_to_float(f.af.beat_strength);
  const float conf  = vp_utils::q16_to_float(f.af.tempo_confidence);
  const float sil   = vp_utils::q16_to_float(f.af.tempo_silence);
  const float flux  = vp_utils::q16_to_float(f.af.flux);
  const float pkbin_hz = audio_frame_utils::freq_from_bin(peak_bin);

  Serial.printf("[vp] epoch=%lu t=%lums bpm=%.1f phase=%.2f conf=%.2f beat=%u str=%.2f sil=%.2f flux=%.3f pkbin=%u@%.0fHz\n",
                static_cast<unsigned long>(f.epoch),
                static_cast<unsigned long>(f.t_ms),
                bpm, phase, conf,
                static_cast<unsigned>(f.af.beat_flag),
                str, sil, flux,
                static_cast<unsigned>(peak_bin), pkbin_hz);

  const float bL  = vp_utils::q16_to_float(f.af.band_low);
  const float bLM = vp_utils::q16_to_float(f.af.band_low_mid);
  const float bP  = vp_utils::q16_to_float(f.af.band_presence);
  const float bH  = vp_utils::q16_to_float(f.af.band_high);
  Serial.printf("[vp] bands low=%.3f lowmid=%.3f pres=%.3f high=%.3f (%s)\n",
                bL, bLM, bP, bH,
                use_smooth ? "smooth" : "raw");
}

} // namespace vp_renderer

// ---------- HMI controls ----------
namespace vp_renderer {

void adjust_brightness(int delta) {
  int v = (int)g_brightness + delta;
  if (v < 0) v = 0; if (v > 255) v = 255;
  g_brightness = (uint8_t)v;
  FastLED.setBrightness(g_brightness);
}

void adjust_speed(float factor) {
  if (factor <= 0.0f) return;
  g_speed *= factor;
  if (g_speed < 0.1f) g_speed = 0.1f;
  if (g_speed > 5.0f) g_speed = 5.0f;
}

void next_mode() {
  g_mode = static_cast<uint8_t>((g_mode + 1) % 7);
}

void prev_mode() {
  g_mode = static_cast<uint8_t>((g_mode + 7 - 1) % 7);
}

Status status() {
  return Status{g_brightness, g_speed, g_mode};
}

} // namespace vp_renderer

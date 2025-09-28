#include "vp_bars24.h"
#include <math.h>
#include <string.h>
#include "perceptual_map24.h"
#include "ws2812_dual_rmt_ng.h"
#include "downbeat_lane.h"     // NEW: downbeat-aware lane

namespace vp_bars24 {

static Config    S;
static bool      g_ready = false;

static CRGB*     g_line0 = nullptr;
static CRGB*     g_line1 = nullptr;

static float     g_env24[24];
static float     g_alpha_att = 0.25f;
static float     g_alpha_rel = 0.06f;

// A small extra envelope to smooth the downbeat kick over a few pixels
static float     g_kick_env = 0.0f;

static inline float clamp01(float x){ return (x<0.0f?0.0f:(x>1.0f?1.0f:x)); }
static inline uint8_t u8(float x){ x = clamp01(x); return (uint8_t)lrintf(x * 255.0f); }

static void recompute_alphas() {
  const float F = (S.fps_target_us > 0) ? (1e6f / (float)S.fps_target_us) : 100.0f;
  auto alpha_from_hz = [&](float hz)->float {
    if (hz < 0.1f) hz = 0.1f;
    return 1.0f - expf(-2.0f * (float)M_PI * hz / F);
  };
  g_alpha_att = alpha_from_hz(S.ar_attack_hz);
  g_alpha_rel = alpha_from_hz(S.ar_release_hz);
}

static CRGB hue_to_rgb(float h, float v){
  h = fmodf(h, 1.0f); if (h < 0.0f) h += 1.0f;
  float s = 1.0f;
  float i = floorf(h*6.0f);
  float f = h*6.0f - i;
  float p = v*(1.0f - s);
  float q = v*(1.0f - s*f);
  float t = v*(1.0f - s*(1.0f - f));
  int ii = ((int)i) % 6;
  float r,g,b;
  switch(ii){
    case 0: r=v; g=t; b=p; break;
    case 1: r=q; g=v; b=p; break;
    case 2: r=p; g=v; b=t; break;
    case 3: r=p; g=q; b=v; break;
    case 4: r=t; g=p; b=v; break;
    default:r=v; g=p; b=q; break;
  }
  return CRGB{ u8(r), u8(g), u8(b) };
}

static inline float gamma_luma(float x){ return powf(clamp01(x), S.bar_gamma); }

static void clear_lines() {
  memset(g_line0, 0, sizeof(CRGB) * S.leds_per_line);
  memset(g_line1, 0, sizeof(CRGB) * S.leds_per_line);
}

bool init(const Config& cfg) {
  S = cfg;
  if (S.leds_per_line <= 0) return false;

  g_line0 = (CRGB*)heap_caps_malloc(sizeof(CRGB) * S.leds_per_line, MALLOC_CAP_INTERNAL);
  g_line1 = (CRGB*)heap_caps_malloc(sizeof(CRGB) * S.leds_per_line, MALLOC_CAP_INTERNAL);
  if (!g_line0 || !g_line1) return false;

  memset(g_env24, 0, sizeof(g_env24));
  g_kick_env = 0.0f;
  recompute_alphas();

  pmap24::init(pmap24::Scale::Mel, 30.0f, 8000.0f);
  downbeat::init(4);                // default 4/4
  downbeat::set_conf_threshold(0.60f, 0.42f);
  downbeat::set_env_decay(0.94f);   // a touch slower decay for visible kick

  g_ready = true;
  return true;
}

void set_gain(float g){ if (g < 0.2f) g = 0.2f; if (g > 3.0f) g = 3.0f; S.max_gain = g; }
void set_gamma(float g){ if (g < 1.0f) g = 1.0f; if (g > 3.5f) g = 3.5f; S.bar_gamma = g; }
void set_fps_us(int us){ if (us < 4000) us = 4000; if (us > 25000) us = 25000; S.fps_target_us = us; recompute_alphas(); }

// Convert Q16.16 → linear
static inline float q16_lin(int32_t q){ return (q <= 0) ? 0.0f : ((float)q / 65536.0f); }

void render(const AudioFrame& f) {
  if (!g_ready) return;

  // --- Downbeat estimator consumes the frame ---
  downbeat::ingest(f);
  const bool  is_db   = downbeat::downbeat_pulse();
  const float accent  = downbeat::accent();              // 0..1
  const float barph   = q16_lin(downbeat::bar_phase_q16()); // 0..1
  const uint8_t bidx  = downbeat::bar_index();

  // Track a visible kick envelope (slightly separate from downbeat::accent)
  if (is_db) g_kick_env = fmaxf(g_kick_env, 0.95f);
  else       g_kick_env *= 0.92f;

  // 1) Map 64 processed bins -> 24 bands
  int32_t bands24_q16[24];
  pmap24::map64to24(f.smooth_spectral, bands24_q16);

  // 2) Convert to linear and global gain
  float v24[24];
  for (int i=0;i<24;++i) v24[i] = clamp01(S.max_gain * q16_lin(bands24_q16[i]));

  // 3) Per-band AR
  for (int i=0;i<24;++i){
    float x = v24[i];
    float y = g_env24[i];
    float a = (x > y) ? g_alpha_att : g_alpha_rel;
    y += a * (x - y);
    g_env24[i] = y;
  }

  // 4) Precompute per-frame color modulation with bar-phase (slow shift)
  // Slight hue drift across the bar to give a musical cycle sense
  const float phase_hue_push = 0.03f * sinf(2.0f*(float)M_PI*barph);   // ±0.03 hue
  const float db_hue_push    = is_db ? 0.07f : 0.0f;                   // kick hue push
  const float hue_push       = phase_hue_push + db_hue_push;

  // 5) Draw bars
  clear_lines();
  const int gap = (int)S.bar_gap_px;
  const int W = S.leds_per_line;
  const int bands = 24;
  const int seg_w = (W - gap*(bands-1)) / bands;
  const float floor = S.bar_floor;

  // a slightly stronger luma boost on downbeat, softer on other beats
  float luma_kick = 1.0f + 0.55f * g_kick_env + 0.25f * accent;

  for (int b=0;b<bands;++b){
    int x0 = b*(seg_w + gap);
    int x1 = x0 + seg_w;
    if (x0 >= W) break;
    if (x1 > W) x1 = W;

    // Base hue across bands (low→high), plus frame-wide push
    float base_h = S.hue_lo + (S.hue_hi - S.hue_lo) * ((float)b / (float)(bands - 1));
    float h = base_h + hue_push;

    // Luminance with gamma + floor and beat-driven kick
    float lum0 = floor + (1.0f - floor) * gamma_luma(g_env24[b]);
    float lum  = clamp01(lum0 * luma_kick);

    // Soft edge
    for (int x=x0; x<x1; ++x){
      float edge = 1.0f;
      if (seg_w >= 4) {
        int ix = x - x0;
        if (ix < 1) edge = 0.65f;
        else if (ix == 1) edge = 0.85f;
        else if (ix == seg_w-2) edge = 0.85f;
        else if (ix == seg_w-1) edge = 0.65f;
      }
      CRGB c = hue_to_rgb(h, clamp01(lum * edge));
      g_line0[x] = c;
      g_line1[x] = c;
    }
  }

  // Optional: a subtle bar-start white blink as a “metronome” (very short)
  if (is_db) {
    // Paint 2px at both ends a gentle white tint
    for (int i=0;i<2 && i<S.leds_per_line; ++i){
      uint8_t w = (uint8_t)lrintf(85.0f + 60.0f * accent); // ~1/3 white
      g_line0[i] = CRGB{w,w,w};
      g_line0[S.leds_per_line-1-i] = CRGB{w,w,w};
      g_line1[i] = CRGB{w,w,w};
      g_line1[S.leds_per_line-1-i] = CRGB{w,w,w};
    }
  }

  // 6) Output paced
  (void)ws2812_ng_show_dual_paced(g_line0, g_line1, (uint32_t)S.fps_target_us);
}

} // namespace vp_bars24

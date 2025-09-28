#pragma once
#include <stdint.h>
#include "audio_bus.h"                // for AudioFrame
#include "perceptual_map24.h"         // 64 -> 24 mapper (Mel/Bark)
#include "ws2812_dual_rmt_ng.h"       // new RMT dual WS2812 driver (IDF 5.x)

// ================== VP Bars24 — public API ==================
//
// 1) Call vp_bars24::init(cfg) once (after ws2812_ng_init).
// 2) Each frame, call vp_bars24::render(*acquire_spectral_frame()).
//
// All state internal. No RTOS tasks, single-core safe.

namespace vp_bars24 {

struct Config {
  // LED layout
  int leds_per_line = 160;       // physical per strip (both strips same)
  int fps_target_us = 8000;      // 8ms (125 fps) or 10ms (100 fps) for ultra-even cadence
  // Visual style
  uint8_t bar_gap_px = 1;        // pixels of gap between bars
  float   bar_gamma  = 2.2f;     // perceptual brightness curve (applied to luminance)
  float   bar_floor  = 0.02f;    // min visible level (post AR) to avoid complete black
  float   max_gain   = 1.6f;     // global gain on 24-band energy before clipping [0..1]
  // AR on top of upstream smoothing (fast attack, slower release)
  float   ar_attack_hz  = 45.0f;
  float   ar_release_hz = 8.0f;
  // Hue banding
  float   hue_lo = 0.02f;        // 0..1 (approx red->orange)
  float   hue_hi = 0.75f;        // 0..1 (approx violet)
};

// Call once. Requires ws2812_ng_init() to have succeeded.
bool init(const Config& cfg);

// Render one frame (bars from processed 64-bin spectrum).
// Uses AudioFrame::smooth_spectral as input (Q16.16 linear).
void render(const AudioFrame& f);

// Live tuning (optional)
void set_gain(float g);
void set_gamma(float g);
void set_fps_us(int us);

} // namespace vp_bars24

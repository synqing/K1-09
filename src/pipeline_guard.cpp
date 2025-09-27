#include "pipeline_guard.h"

#include <Arduino.h>

#include "debug/debug_flags.h"
#include "AP/sph0645.h"

namespace {

struct GuardState {
  pipeline_guard::Stats stats{};
  uint32_t last_report_ms = 0;
  uint32_t last_audio_notice_ms = 0;
  uint32_t last_vp_notice_ms = 0;
  bool logged_i2s_rate = false;
};

GuardState g_state{};

constexpr uint32_t kReportPeriodMs = 1000u;
constexpr uint32_t kAudioWarnLoopThreshold = 4u;
constexpr uint32_t kVpDropWarnThreshold = 8u;
constexpr uint32_t kNoticeCooldownMs = 500u;

void print_summary(uint32_t now_ms) {
  // Always print the guard summary regardless of debug group toggles so
  // frame cadence diagnostics are available at boot.
  if ((now_ms - g_state.last_report_ms) < kReportPeriodMs) {
    return;
  }

  const auto& s = g_state.stats;
  const uint32_t ms_since_audio = now_ms - s.last_audio_ms;
  const uint32_t ms_since_render = now_ms - s.last_vp_render_ms;

  Serial.printf("[guard] loop=%lu audio=%lu stalls=%lu(run=%u %lums) vp=%lu renders=%lu(drop=%u %lums)\n",
                static_cast<unsigned long>(s.loop_count),
                static_cast<unsigned long>(s.audio_chunk_count),
                static_cast<unsigned long>(s.audio_chunk_stalls),
                static_cast<unsigned>(s.consecutive_audio_stalls),
                static_cast<unsigned long>(ms_since_audio),
                static_cast<unsigned long>(s.vp_tick_count),
                static_cast<unsigned long>(s.vp_render_count),
                static_cast<unsigned>(s.consecutive_vp_drops),
                static_cast<unsigned long>(ms_since_render));

  if (!g_state.logged_i2s_rate) {
    const float sr = K1Lightwave::Audio::Sph0645::read_sample_rate_hz();
    if (sr > 0.0f) {
      Serial.printf("[guard] i2s_rate=%.2fHz\n", static_cast<double>(sr));
      g_state.logged_i2s_rate = true;
    }
  }

  g_state.last_report_ms = now_ms;
}

void maybe_warn_audio(uint32_t now_ms) {
  if (!debug_flags::enabled(debug_flags::kGroupVP)) {
    return;
  }
  const auto& s = g_state.stats;
  if (s.consecutive_audio_stalls < kAudioWarnLoopThreshold) {
    return;
  }
  if ((now_ms - g_state.last_audio_notice_ms) < kNoticeCooldownMs) {
    return;
  }

  Serial.printf("[guard] audio stalled %u loops (~%lums since chunk)\n",
                static_cast<unsigned>(s.consecutive_audio_stalls),
                static_cast<unsigned long>(now_ms - s.last_audio_ms));
  g_state.last_audio_notice_ms = now_ms;
}

void maybe_warn_vp(uint32_t now_ms) {
  if (!debug_flags::enabled(debug_flags::kGroupVP)) {
    return;
  }
  const auto& s = g_state.stats;
  if (s.consecutive_vp_drops < kVpDropWarnThreshold) {
    return;
  }
  if ((now_ms - g_state.last_vp_notice_ms) < kNoticeCooldownMs) {
    return;
  }

  Serial.printf("[guard] vp dropped %u frames (~%lums since render)\n",
                static_cast<unsigned>(s.consecutive_vp_drops),
                static_cast<unsigned long>(now_ms - s.last_vp_render_ms));
  g_state.last_vp_notice_ms = now_ms;
}

}  // namespace

namespace pipeline_guard {

void reset(uint32_t now_ms) {
  g_state = GuardState{};
  g_state.stats.last_loop_ms = now_ms;
  g_state.stats.last_audio_ms = now_ms;
  g_state.stats.last_vp_tick_ms = now_ms;
  g_state.stats.last_vp_render_ms = now_ms;
  g_state.last_report_ms = now_ms;
  g_state.last_audio_notice_ms = now_ms;
  g_state.last_vp_notice_ms = now_ms;
}

void loop_begin(uint32_t now_ms) {
  auto& s = g_state.stats;
  s.loop_count += 1u;
  s.last_loop_ms = now_ms;
}

void notify_audio_chunk(bool chunk_ready, uint32_t now_ms) {
  auto& s = g_state.stats;
  if (chunk_ready) {
    s.audio_chunk_count += 1u;
    s.consecutive_audio_stalls = 0u;
    s.last_audio_ms = now_ms;
  } else {
    s.audio_chunk_stalls += 1u;
    s.consecutive_audio_stalls += 1u;
  }
}

void notify_vp_tick(bool rendered, uint32_t now_ms) {
  auto& s = g_state.stats;
  s.vp_tick_count += 1u;
  s.last_vp_tick_ms = now_ms;
  if (rendered) {
    s.vp_render_count += 1u;
    s.consecutive_vp_drops = 0u;
    s.last_vp_render_ms = now_ms;
  } else {
    s.consecutive_vp_drops += 1u;
  }
}

void loop_end(uint32_t now_ms) {
  print_summary(now_ms);
  maybe_warn_audio(now_ms);
  maybe_warn_vp(now_ms);
}

Stats snapshot() {
  return g_state.stats;
}

}  // namespace pipeline_guard

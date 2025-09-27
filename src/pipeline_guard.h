#pragma once

#include <stdint.h>

namespace pipeline_guard {

struct Stats {
  uint32_t loop_count;
  uint32_t audio_chunk_count;
  uint32_t audio_chunk_stalls;
  uint32_t consecutive_audio_stalls;
  uint32_t vp_tick_count;
  uint32_t vp_render_count;
  uint32_t consecutive_vp_drops;
  uint32_t last_loop_ms;
  uint32_t last_audio_ms;
  uint32_t last_vp_tick_ms;
  uint32_t last_vp_render_ms;
};

// Resets the guard state and seeds baseline timestamps.
void reset(uint32_t now_ms);

// Called at the top of each firmware loop iteration.
void loop_begin(uint32_t now_ms);

// Called after the audio producer either publishes a chunk or reports a stall.
void notify_audio_chunk(bool chunk_ready, uint32_t now_ms);

// Called immediately after vp::tick() to observe render behavior and cadence.
void notify_vp_tick(bool rendered, uint32_t now_ms);

// Called at the end of each firmware loop iteration to surface telemetry.
void loop_end(uint32_t now_ms);

// Returns a copy of the current guard statistics for inspection or tests.
Stats snapshot();

}  // namespace pipeline_guard


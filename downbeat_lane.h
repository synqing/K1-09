#pragma once
#include <stdint.h>
#include "audio_bus.h"   // for AudioFrame (beat_flag, beat_strength, tempo_confidence, tempo_bpm, beat_phase, tempo_silence, band_low,...)

/* =============================================================================
   downbeat_lane.h  —  Lightweight downbeat estimator (4/4 biased)
   - No new fields on AudioFrame. Consumes existing tempo/beat fields.
   - Exposes:
       * downbeat_pulse()     : true on frames that land on a downbeat
       * bar_phase_q16()      : Q16.16 in [0,1) since the last downbeat
       * bar_index()          : 0..(beats_per_bar-1) (default 4)
       * accent()             : 0..1, a short decaying envelope for visual kicks
   - Call downbeat::init(beats_per_bar=4) once.
   - Call downbeat::ingest(const AudioFrame&) each visual frame.
   ============================================================================= */

namespace downbeat {

// Configure expected beats per bar (default 4)
bool init(uint8_t beats_per_bar = 4);

// Feed the latest audio frame
void ingest(const AudioFrame& f);

// Signals for the current visual frame
bool     downbeat_pulse();   // edge pulse at the bar start
uint8_t  bar_index();        // 0..B-1 (B=beats_per_bar)
int32_t  bar_phase_q16();    // Q16.16 in [0,1)
float    accent();           // decaying envelope 0..1

// Optional live tuning
void set_conf_threshold(float on, float off);   // default 0.6 / 0.42
void set_env_decay(float per_frame);            // default 0.92 (8% per frame)

} // namespace downbeat

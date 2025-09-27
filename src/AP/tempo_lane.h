#pragma once
#include <stdint.h>
#include "audio_config.h"
#include "audio_bus.h"  // for q16

/* ================= Tempo Lane (single core, static, linear) =================
   Consumes: flux (linear Q16.16) each AP tick.
   Produces: tempo_bpm (Q16.16 BPM), beat_phase (Q16.16 [0,1)),
             beat_strength (Q16.16 [0,1]), beat_flag (0/1 pulse on wrap).
   CPU bounded via interlacing small number of tempo bins per tick.
============================================================================= */

namespace tempo_lane {

// ---- Compile-time knobs ----
#ifndef TEMPO_MIN_BPM
#define TEMPO_MIN_BPM  90u
#endif
#ifndef TEMPO_MAX_BPM
#define TEMPO_MAX_BPM  180u
#endif
#ifndef TEMPO_NUM_BINS
#define TEMPO_NUM_BINS 60u     // 1 BPM steps (inclusive endpoints)
#endif

// Novelty ring: Fs_nov = audio_sample_rate / chunk_size  (~125 Hz)
#ifndef TEMPO_HIST_LEN
#define TEMPO_HIST_LEN 512u    // ~4.1 s @ 125 Hz
#endif

#ifndef TEMPO_BINS_PER_TICK
#define TEMPO_BINS_PER_TICK 2u // interlacing
#endif

// Magnitude smoothing (EMA on bin magnitudes, linear)
#ifndef TEMPO_MAG_EMA_ALPHA
#define TEMPO_MAG_EMA_ALPHA 0.20  // snappy tracking
#endif

// Initialize static state (rings, bin coefficients/window)
bool tempo_init();

// Feed one AP tick worth of raw audio (Q24 chunk of length chunk_size).
void tempo_ingest(const int32_t* q24_chunk);

// Update a small number of tempo bins (interlaced). Call once per AP tick.
// Writes results to the provided outputs.
void tempo_update(q16& out_tempo_bpm_q16,
                  q16& out_beat_phase_q16,
                  q16& out_beat_strength_q16,
                  uint8_t& out_beat_flag,
                  q16& out_tempo_confidence_q16,
                  q16& out_silence_q16);

// True once the novelty history is full and heavy processing is valid.
bool tempo_ready();

// Helpers for downstream gating / diagnostics.
bool tempo_is_silent();
q16 tempo_confidence_q16();
q16 tempo_silence_q16();

} // namespace tempo_lane

#pragma once
#include <stdint.h>
#include "audio_bus.h"

/* ===================== Audio Producer (single-core) =====================
   Call audio_pipeline_init() once at boot.
   Call audio_pipeline_tick() exactly once per full Layer-1 chunk.
============================================================================ */

bool audio_pipeline_init();  // builds window, inits backends/states

// Input: q24_chunk = int32_t[chunk_size], already arithmetic >>8 (Q24 domain)
// t_ms   = producer time (ms since boot)
void audio_pipeline_tick(const int32_t* q24_chunk, uint32_t t_ms);
/* ===================== End of audio_producer.h ===================== */   




/*
sfa_tick() sequence (deterministic, no heap):

Waveform: convert Q24 → Q15 into local staging frame.waveform[] (or omit if not needed).

Levels: compute vu_peak and vu_rms (RMS after per-chunk mean removal).

Backend: compute_bins(q24_chunk, tmp_bins[]) (Goertzel today, FFT later).

tmp_bins[] in linear amplitude (or power; choose one and stick to it—recommend amplitude).

Smoothing: EMA into spec_smooth[] with fixed alpha (compile-time in sfa_config.h).

Chroma: accumulate pitch classes from spec_raw[] (static mapping table).

Bands: sum low/mid/high ranges (static index lists).

Flux: max(0, Σ(max(0, raw[k] - raw_prev[k]))) (store raw_prev[] statically).

Publish: memcpy(staging → public_frame); sfa_epoch++.
*/


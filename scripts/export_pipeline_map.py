#!/usr/bin/env python3
"""Export authoritative audio/visual pipeline map as JSON.

The data model mirrors `docs/firmware/pipelines.md` so that CI can verify
changes programmatically and future tooling (Task-Master, dashboards) can consume
structured metadata.
"""
from __future__ import annotations

import json
from pathlib import Path

PIPELINE_MAP = {
    "audio_pipeline": [
        {
            "stage": "I2S std channel read (IDF5)",
            "producer": "K1Lightwave::Audio::Sph0645::read_q24_chunk",
            "output": "q24_chunk[128]",
            "type": "int32_t[128] (Q24)",
            "range": "±(2^23-1) after >>8",
            "consumers": ["AP tick"],
            "notes": {
                "dma_desc_num": 2,
                "dma_frame_num": 128,
                "queue": "latest-only (xQueueOverwrite)",
                "core": 1
            },
        },
        {
            "stage": "DC removal + waveform snapshot",
            "producer": "audio_pipeline_tick",
            "output": "centered_q24[128] / waveform[128]",
            "type": "int32_t / int16_t (Q15)",
            "range": "Q24→Q15 clamped",
            "consumers": ["levels", "Goertzel"],
            "notes": {"mean_per_chunk": True},
        },
        {
            "stage": "Levels (peak/RMS)",
            "producer": "levels::{peak,rms}_q16_from_q24",
            "output": "vu_peak / vu_rms",
            "type": "Q16.16",
            "range": "0..1",
            "consumers": ["metrics"],
        },
        {
            "stage": "Goertzel (R-SPEC, Hann-aware)",
            "producer": "goertzel_backend::compute_bins",
            "output": "raw_spectral[64]",
            "type": "Q16.16",
            "range": "0..1",
            "consumers": ["melproc", "chroma", "flux"],
            "notes": {"window": "Hann LUT", "bins": 64},
        },
        {
            "stage": "Perceptual filterbank",
            "producer": "melproc::melproc_process64",
            "output": "smooth_spectral[64]",
            "type": "Q16.16",
            "range": "0..1",
            "consumers": ["bands", "VP"],
            "notes": {"A-weight": True, "floor": True, "softknee": True, "AR": True},
        },
        {
            "stage": "Chroma accumulation",
            "producer": "chroma_map::accumulate",
            "output": "chroma[12]",
            "type": "Q16.16",
            "range": "0..1",
            "consumers": ["VP"],
        },
        {
            "stage": "Spectral flux",
            "producer": "flux::compute",
            "output": "flux",
            "type": "Q16.16",
            "range": "0..~1.25 (clamped)",
            "consumers": ["tempo_lane", "VP"],
        },
        {
            "stage": "Tempo lane (Beat-R1)",
            "producer": "tempo_lane::{ingest,update}",
            "output": "tempo_bpm, beat_phase, beat_strength, beat_flag, tempo_confidence, tempo_silence",
            "type": "Q16.16 + flags",
            "range": "phase∈[0,1), bpm 80..180",
            "consumers": ["downbeat_lane", "VP"],
            "notes": {"decimate_N": 4},
        },
        {
            "stage": "Publish AudioFrame",
            "producer": "audio_pipeline_tick",
            "output": "AudioFrame (epoch++)",
            "type": "POD struct",
            "range": "N/A",
            "consumers": ["VP consumer"],
        },
    ],
    "visual_pipeline": [
        {
            "stage": "LED init (new RMT)",
            "producer": "ws2812_ng_init",
            "output": "dual RMT channels",
            "type": "RMT (IDF5)",
            "range": "800kHz WS2812",
            "consumers": ["vp_bars24"],
            "notes": {"leds_per_line": 160},
        },
        {
            "stage": "Bars24 init",
            "producer": "vp_bars24::init",
            "output": "internal state",
            "type": "config",
            "range": "N/A",
            "consumers": ["vp_bars24::render"],
            "notes": {"fps_us": 8000, "gain": 1.6, "gamma": 2.2},
        },
        {
            "stage": "Downbeat lane",
            "producer": "downbeat::ingest",
            "output": "downbeat_pulse, accent, bar_phase",
            "type": "bool/float/Q16.16",
            "range": "accent 0..1",
            "consumers": ["vp_bars24"],
        },
        {
            "stage": "64→24 perceptual map",
            "producer": "pmap24::map64to24",
            "output": "bands24[24]",
            "type": "Q16.16",
            "range": "0..1",
            "consumers": ["vp_bars24"],
        },
        {
            "stage": "AR + draw bars",
            "producer": "vp_bars24::render",
            "output": "line0/line1 CRGB[160]",
            "type": "8-bit RGB",
            "range": "0..255 per channel",
            "consumers": ["RMT TX"],
        },
        {
            "stage": "Paced dual send",
            "producer": "ws2812_ng_show_dual_paced",
            "output": "LED frame",
            "type": "timed TX",
            "range": "frame spacing = fps_us",
            "consumers": ["LED strips"],
            "notes": {"125fps": True},
        },
    ],
    "magic_numbers": {
        "chunk_size": {
            "value": 128,
            "location": "audio_config.h",
            "rationale": "8 ms @ 16 kHz"
        },
        "audio_sample_rate": {
            "value": 16000,
            "location": "audio_config.h",
            "rationale": "Low-latency, tempo-friendly"
        },
        "TEMPO_DECIMATE_N": {
            "value": 4,
            "location": "src/AP/tempo_lane.h",
            "rationale": "Limit heavy work to ≤8–10 ms"
        },
        "fps_target_us": {
            "value": 8000,
            "location": "vp_bars24::Config",
            "rationale": "125 fps exact pacing"
        },
        "leds_per_line": {
            "value": 160,
            "location": "LED geometry",
            "rationale": "Physical strip length"
        }
    },
}


def main() -> None:
    out_dir = Path("analysis")
    out_dir.mkdir(exist_ok=True)
    dest = out_dir / "pipeline_map.json"
    dest.write_text(json.dumps(PIPELINE_MAP, indent=2, sort_keys=True))
    print(f"wrote {dest}")


if __name__ == "__main__":
    main()

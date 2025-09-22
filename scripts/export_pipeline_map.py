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
            "stage": "I2S DMA read",
            "producer": "i2s_audio::acquire_sample_chunk",
            "output": "audio_raw_state.getRawSamples()",
            "type": "int32_t[SAMPLES_PER_CHUNK]",
            "range": "Left-justified 32-bit PCM (±8M)",
            "consumers": ["Scaling stage"],
            "notes": {
                "bytes_expected": "CONFIG.SAMPLES_PER_CHUNK * 4",
                "timeout": "portMAX_DELAY to forbid partial reads"
            },
        },
        {
            "stage": "Linear scaling & DC removal",
            "producer": "i2s_audio::acquire_sample_chunk",
            "output": "waveform / audio_processed_state",
            "type": "int16_t[256]",
            "range": "±32767 (clamped)",
            "consumers": ["audio_processed_state", "sample_window"],
            "notes": {
                "sensitivity": "CONFIG.SENSITIVITY (default 1.0)",
                "dc_offset": "CONFIG.DC_OFFSET (-14800)",
                "min_state_duration_ms": 1500,
            },
        },
        {
            "stage": "Peak tracking & AGC",
            "producer": "i2s_audio::acquire_sample_chunk",
            "output": "max_waveform_val_*",
            "type": "float",
            "range": "0-32767",
            "consumers": ["AGC", "sweet spot", "LED gating"],
            "notes": {
                "attack": 0.5,
                "decay": 0.02,
                "floor": "CONFIG.SWEET_SPOT_MIN_LEVEL (>=750)",
            },
        },
        {
            "stage": "Sample history",
            "producer": "i2s_audio::acquire_sample_chunk",
            "output": "audio_raw_state.waveform_history_",
            "type": "int16_t[4][1024]",
            "range": "Clamped waveform",
            "consumers": ["process_GDFT"],
            "notes": {
                "guard": "0xABCD1234",
            },
        },
        {
            "stage": "Sliding window",
            "producer": "i2s_audio::acquire_sample_chunk",
            "output": "sample_window",
            "type": "int16_t[4096]",
            "range": "±32767",
            "consumers": ["process_GDFT"],
            "notes": {
                "history_length": 4096,
            },
        },
        {
            "stage": "Goertzel transform",
            "producer": "process_GDFT",
            "output": "magnitudes_normalized",
            "type": "float[NUM_FREQS]",
            "range": ">=0",
            "consumers": ["spectrogram", "noise_calibration", "lightshow_modes"],
            "notes": {
                "num_freqs": 64,
                "fast_sqrt_constant": "0x5f375a86",
            },
        },
        {
            "stage": "Noise calibration",
            "producer": "process_GDFT",
            "output": "noise_samples",
            "type": "SQ15x16[NUM_FREQS]",
            "range": "0-1",
            "consumers": ["process_GDFT"],
            "notes": {
                "iterations": 256,
            },
        },
        {
            "stage": "Spectrogram smoothing",
            "producer": "process_GDFT",
            "output": "spectrogram / chromagram",
            "type": "SQ15x16[]",
            "range": "0-1",
            "consumers": ["lightshow_modes", "serial_menu"],
            "notes": {
                "ema_factor": 0.3,
            },
        },
    ],
    "visual_pipeline": [
        {
            "stage": "Frame initialization",
            "producer": "led_thread",
            "output": "leds_16 / leds_16_fx / leds_16_ui",
            "type": "CRGB16[LED_COUNT]",
            "range": "0-1 (Q8.8)",
            "consumers": ["lightshow_modes", "led_utilities"],
            "notes": {
                "frame_seq": ["g_frame_seq_write", "g_frame_seq_ready", "g_frame_seq_shown"],
            },
        },
        {
            "stage": "Palette prep",
            "producer": "led_utilities::update_palette_buffers",
            "output": "palette LUTs",
            "type": "CRGB16[]",
            "range": "0-1",
            "consumers": ["lightshow_modes"],
            "notes": {
                "dither": "dither_table[8]",
            },
        },
        {
            "stage": "Mode dispatch",
            "producer": "lightshow_modes::render_active_mode",
            "output": "mode buffers",
            "type": "CRGB16[]",
            "range": "0-1",
            "consumers": ["led_utilities"],
            "notes": {
                "selector": "CONFIG.LIGHTSHOW_MODE",
            },
        },
        {
            "stage": "GDFT mode",
            "producer": "light_mode_gdft",
            "output": "leds_16_fx",
            "type": "CRGB16[160]",
            "range": "0-1",
            "consumers": ["compose_frame"],
            "notes": {
                "inputs": ["spectrogram_smooth", "hue_lookup"],
            },
        },
        {
            "stage": "Bloom mode",
            "producer": "light_mode_bloom",
            "output": "leds_16_fx / leds_16_prev_secondary",
            "type": "CRGB16[160]",
            "range": "0-1",
            "consumers": ["compose_frame"],
            "notes": {
                "decay": 0.78,
                "sparkle_threshold": 0.45,
            },
        },
        {
            "stage": "Quantum collapse",
            "producer": "light_mode_quantum_collapse",
            "output": "particle buffers / leds_16_fx",
            "type": "float[] / CRGB16[]",
            "range": "mode-specific",
            "consumers": ["compose_frame"],
            "notes": {
                "fluid_diffusion": 0.035,
                "particle_drag": 0.98,
            },
        },
        {
            "stage": "Waveform mode",
            "producer": "light_mode_waveform",
            "output": "leds_16_fx",
            "type": "CRGB16[160]",
            "range": "0-1",
            "consumers": ["compose_frame"],
            "notes": {
                "inputs": ["audio_processed_state.waveform"],
            },
        },
        {
            "stage": "Frame compositing",
            "producer": "led_utilities::compose_frame",
            "output": "leds_out",
            "type": "CRGB[LED_COUNT]",
            "range": "0-255",
            "consumers": ["FastLED.show"],
            "notes": {
                "mirror": "CONFIG.MIRROR_ENABLED",
                "current_limit_ma": 1500,
            },
        },
    ],
    "magic_numbers": {
        "CONFIG.SAMPLES_PER_CHUNK": {
            "value": 256,
            "location": "src/core/globals.cpp:24",
            "rationale": "Latency vs resolution trade-off; 16 ms window at 16 kHz",
        },
        "MIN_STATE_DURATION_MS": {
            "value": 1500,
            "location": "src/i2s_audio.h:51",
            "rationale": "Prevents AGC thrash between loud/silent states",
        },
        "fast_sqrt_constant": {
            "value": "0x5f375a86",
            "location": "src/GDFT.h:120",
            "rationale": "Fast inverse sqrt approximation for magnitude normalization",
        },
        "BLOOM_DECAY": {
            "value": 0.78,
            "location": "src/lightshow_modes.h:603",
            "rationale": "Trail persistence for bloom mode",
        },
        "CONFIG.MAX_CURRENT_MA": {
            "value": 1500,
            "location": "src/core/globals.cpp:31",
            "rationale": "LED power budget for 160 WS2812 pixels",
        },
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

# AudioFrame Contract

This note documents the public interface between the Level-2 audio producer and any downstream consumer (VP, lighting bridge, etc.).

## Snapshot semantics
- The producer updates a single `AudioFrame` instance (`g_public`) once per audio tick (~125 Hz).
- `audio_frame_epoch` increments *after* the frame copy completes, so consumers must treat it as a generation counter.
- Use `audio_frame_utils::snapshot_audio_frame(frame)` (see `src/audio/audio_bus.h`) to copy the frame safely. Internally it retries until `audio_frame_epoch` is stable; callers can increase the attempt budget if needed.
- When rolling your own loop, capture `start = frame->audio_frame_epoch`, copy the struct, then verify `start == copy.audio_frame_epoch`. Retry on mismatch.

## Fixed-point helpers
- All linear quantities in the frame use signed Q16.16 (`q16`). Convert with `audio_frame_utils::q16_to_float(v)`.
- Tempo BPM is stored in Q16.16 as well; `audio_frame_utils::q16_to_bpm(v)` returns the floating-point BPM.
- `tempo_ready` is a boolean byte set when the novelty history is full. Treat tempo/beat fields as invalid until this bit is set *and* `beat_strength` / `tempo_confidence` exceed your threshold.

## Frequency/bin utilities
- `audio_frame_utils::freq_from_bin(bin)` maps a spectral bin index to Hertz using the compile-time lookup in `audio_config.h`.
- Band summaries follow `K1Lightwave::Audio::Bands4_64` (Low, Low-Mid, Presence, High). Use `audio_frame_utils::band_low_hz(band)` / `band_high_hz(band)` to label UI elements or filter ranges.

## Tempo diagnostics (optional)
- Compile-time switches live in `src/audio/audio_config.h`:
  - `AUDIO_DIAG_TEMPO` (currently defined as 1 for bring-up) prints top tempo bins every `AUDIO_DIAG_TEMPO_PERIOD` ticks (default 32) via `Serial`.
  - `AUDIO_DIAG_TEMPO_TOPK` limits how many bins are reported (default 4).
  - `AUDIO_DIAG_FLUX` (currently defined as 1 for bring-up) aggregates a flux histogram and emits one line per `AUDIO_DIAG_FLUX_PERIOD_MS` (default 1000 ms).
- Disable these (`#define … 0`) for production builds to avoid spending ~30 kbps of the serial budget and to keep the AP tick path minimal.
- Runtime toggles: press `1`, `2`, or `3` on the serial console to flip AP input/AC, tempo/energy, or `[tempo]/[flux]` diagnostics respectively; `0` controls the DC/Drift block. Use `+` to enable all groups, `-` to disable all, and `?` for a quick help print. Status echoes as `[debug] …` without altering the colour formatting of the guarded prints. The tempo debug stream now emits `[acf]` (mix + HM top peaks), `[cand]` (lane scores/phases), and `[flux]` summaries when group 3 is enabled.

## Field summary

| Field | Units | Notes |
| --- | --- | --- |
| `waveform[]` | Q15 | Snapshot of the last chunk (post DC preservation).
| `raw_spectral[]` | Q16.16 | Instantaneous magnitudes.
| `smooth_spectral[]` | Q16.16 | EMA magnitudes; alpha is runtime tunable and persisted via NVS.
| `band_low` .. `band_high` | Q16.16 | Saturating sums of spectral regions (see band helpers above).
| `flux` | Q16.16 | Spectral flux (novelty) after clamp.
| `tempo_bpm` | Q16.16 | Current tempo estimate.
| `beat_phase` | Q16.16 | Fractional cycle in [0,1).
| `beat_strength` | Q16.16 | Winner contribution fraction; gate beats on this.
| `tempo_confidence` | Q16.16 | Power concentration in the leading tempo bin.
| `tempo_silence` | Q16.16 | Silence estimator in [0,1].
| `beat_flag` | bool | One-tick pulse when a beat boundary is detected.
| `tempo_ready` | bool | Set once the novelty ring is primed.

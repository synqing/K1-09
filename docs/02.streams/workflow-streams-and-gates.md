# Streams & Gates Workflow Overlay

This overlay decouples functional work from Phase milestones by organizing parallel Streams with explicit integration Gates. Streams use numeric IDs to scale beyond A/B/C.

## Streams (numeric)
- 01-tempo — Beat/tempo provider (contract, impl, validation)
- 02-router — Router FSM (consumes TempoProvider)
- 03-color — Color pipeline retrofit (HSV gamma, palettes, dither)
- 04-docs-qa — Docs, PR templates, CI, linting
- 05-limiter — Current limiter feature and metrics

Branch prefixes
- `stream/01-tempo/*`, `stream/02-router/*`, `stream/03-color/*`, `stream/04-docs-qa/*`, `stream/05-limiter/*`

Labels
- `stream:01-tempo`, `stream:02-router`, `stream:03-color`, `stream:04-docs-qa`, `stream:05-limiter`
- `gate:tempo-contract`, `gate:palette-facade`, `gate:limiter`

## Gates (integration contracts)
- Gate 3A — Tempo Contract v1 accepted
  - API (`TempoProvider`): `init(sample_rate)`, `update(audio_chunk)`, `get_bpm()`, `get_phase_01()`, `is_downbeat()`, `confidence_01()`
  - Stub behind `ENABLE_TEMPO_STUB` enables Router FSM progress before impl lands
- Gate 3B — Palette Facade API stable
  - `palettes_bridge.h` is the façade; color changes remain behind flags
- Gate 5A — Limiter feature landed
  - Metrics extension can merge once limiter API is stable

## Usage
- Track work by Stream; tie PRs to milestones Phase 3–5 via labels.
- Encode dependencies in issue JSON using `depends_on`.


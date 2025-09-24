# Tempo Technical Plan (Stream 01)

Moved from `docs/tempo/tempo_technical_plan.md` and aligned to Streams & Gates.

Contract (Gate 3A)
- Interface: `TempoProvider` (`src/tempo/tempo_provider.h`)
- API: `init(sample_rate)`, `update(audio_chunk)`, `get_bpm()`, `get_phase_01()`, `is_downbeat()`, `confidence_01()`
- Determinism: phase in [0,1), confidence in [0,1]
- Stub: `tempo_provider_stub.cpp` behind `ENABLE_TEMPO_STUB` for Router FSM development

Acceptance
- Unit tests: phase continuity, BPM step changes, downbeat detect over synthetic inputs
- Integration: Router FSM compiles and runs using stub; logs stable dwell/cooldown behavior

Risks
- BPM jitter under noisy inputs → smooth in provider, not in Router
- CPU budget at 120 FPS → limit work per chunk; prefer incremental updates

feat(color/phase3): add guarded post-processing toggles + debug plumbing (no behavior change)

Scope (Agent A scaffolding)
- Add experimental toggles (default OFF) in `src/user_config.h`:
  - `ENABLE_DITHER_POLICY_EXPERIMENT`
  - `ENABLE_BRIGHTNESS_METRICS`
  - `ENABLE_INCANDESCENT_FOR_PALETTE`
- Wire brightness min/max logging (throttled, 1 Hz) behind `ENABLE_BRIGHTNESS_METRICS` in `apply_brightness()`.
- Provide `src/led_color_debug.cpp` TU to resolve header-only debug warnings.

Behavior
- No functional changes by default. All new code paths are gated OFF.

Guardrails
- Router/I²S untouched.
- Phase-3 file scope respected.

Artifacts
- This PR body (phase evidence). Build will run in CI.

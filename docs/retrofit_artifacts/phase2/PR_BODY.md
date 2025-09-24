refactor(color): add palette facade (phase2 scaffolding)

Scope (Agent A)
- Introduce `src/palettes/palette_facade.h` with `pal_primary`, `pal_contrast`, `pal_accent` helpers.
- No behavior changes; no mode migrations (Agent 2 will migrate).

Guardrails
- Router/I²S untouched. No dynamic allocation. Phase‑2 file scope respected.

Artifacts
- This PR body (phase evidence). Build will run in CI.

Notes
- Facade wraps existing `hsv_or_palette()` and uses `CONFIG.SATURATION`.
- Adoption by modes will be done in separate PRs (`refactor/color-modes-migrate-a/b`).

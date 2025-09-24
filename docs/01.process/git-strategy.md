# Git Strategy

## Branching
- Trunk: `main` (or integration branch for ongoing phases).
- Feature/stream branches use prefixes:
  - `stream/01-tempo/*`, `stream/02-router/*`, `stream/03-color/*`, `stream/04-docs-qa/*`, `stream/05-limiter/*`
  - Infra/docs branches: `infra/*`, `docs/*`, `feat/*`, `fix/*`.
- Phase docs branches (examples): `docs/pipelines-ssot`, `docs/scheduling-ssot`, `docs/magic-numbers`.

## Worktrees (recommended)
Use multiple worktrees for parallel streams to avoid switching interrupts:
```
git worktree add ../K1-09-docs-pipelines   -b docs/pipelines-ssot
git worktree add ../K1-09-docs-scheduling  -b docs/scheduling-ssot
git worktree add ../K1-09-docs-magic       -b docs/magic-numbers
git worktree add ../K1-09-docs-tooling     -b infra/docs-tooling
```

## Labels and Gates
- Labels: `stream:01-tempo` … `stream:05-limiter`, `gate:tempo-contract`, `gate:palette-facade`, `gate:limiter`.
- Use labels to encode stream ownership and integration gates on PRs.

## PR Hygiene
- Keep PRs focused by stream. Include:
  - Build log: `platformio run`
  - Size check: `python3 scripts/verify_size_budget.py --baseline ci/size-baseline.json`
  - Docs impact: link updated SSOT sections; check “SSOT docs reviewed/updated”.

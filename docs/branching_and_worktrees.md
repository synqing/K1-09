# Branching, Worktrees, Protection & Tags

Protected Integration Branch
- `phase3/vp-overhaul` — PRs only. Require green CI + CODEOWNERS approvals.

Feature Branches (one topic each)
- `refactor/visual-boundaries`
- `refactor/visual-registry`
- `feat/async-led-double-buffer`
- `feat/audio-ring-buffer`
- `feat/dual-coordinator`
- `feat/dual-operators`
- `feat/dual-router`
- `refactor/uniform-degrade`
- `feat/dual-current-limiter`

Worktree Commands
```
git worktree add ../K1-09-vp-overhaul       -b phase3/vp-overhaul
git worktree add ../K1-09-visual-boundaries -b refactor/visual-boundaries
git worktree add ../K1-09-visual-registry   -b refactor/visual-registry
git worktree add ../K1-09-async-led         -b feat/async-led-double-buffer
git worktree add ../K1-09-audio-ring        -b feat/audio-ring-buffer
git worktree add ../K1-09-dual-coordinator  -b feat/dual-coordinator
git worktree add ../K1-09-dual-operators    -b feat/dual-operators
git worktree add ../K1-09-dual-router       -b feat/dual-router
git worktree add ../K1-09-degrade           -b refactor/uniform-degrade
git worktree add ../K1-09-limiter           -b feat/dual-current-limiter
```

Tags (annotated)
- `phase3-a1` — boundaries + static alloc scaffolding + registry.
- `phase3-a2` — async LED double buffer + flip assertions.
- `phase3-a3` — audio ring buffer + counters.
- `phase3-a4` — Coordinator/Operators/Router minimal pairs.
- `phase3-a5` — uniform degrade; optional limiter (OFF by default).


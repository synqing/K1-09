# Phase 4 Issue Backlog

| Title | Labels | Paths | Acceptance Checks |
|-------|--------|-------|-------------------|
| Phase 4: Author pipelines SSOT doc (docs/03.ssot/firmware-pipelines-ssot.md) | documentation, phase4, agent-a | `docs/03.ssot/firmware-pipelines-ssot.md`, `docs/99.reference/visual-pipeline-overview.md` | `./agent_runner.sh pio run`; `rg -n "TODO|TBD" docs/03.ssot/firmware-pipelines-ssot.md docs/99.reference/visual-pipeline-overview.md` |
| Phase 4: Scheduling SSOT (docs/03.ssot/firmware-scheduling-ssot.md) | documentation, phase4, agent-a | `docs/03.ssot/firmware-scheduling-ssot.md` | `rg -n "TODO|TBD" docs/03.ssot/firmware-scheduling-ssot.md`; `rg -n "g_rb_reads" docs/03.ssot/firmware-scheduling-ssot.md` |
| Phase 4: Magic numbers catalogue (docs/03.ssot/firmware-magic-numbers-ssot.md) | documentation, phase4, agent-b | `docs/03.ssot/firmware-magic-numbers-ssot.md` | `rg -n "CONFIG.SAMPLES_PER_CHUNK" docs/03.ssot/firmware-magic-numbers-ssot.md`; `rg -n "Watchdog" docs/03.ssot/firmware-magic-numbers-ssot.md` |
| Phase 4: Documentation tooling & PR checklist update | infrastructure, phase4, agent-b | `.github/**`, `docs/03.ssot/**` | `test ! -f analysis/PLACEHOLDER`; `rg -n "SSOT docs reviewed" .github` |

> Use `gh issue create --title "…" --body-file docs/05.tracking/phase-4-issues.json` (or paste details above) to populate the tracker.

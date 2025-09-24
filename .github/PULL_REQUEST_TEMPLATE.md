## Summary

Describe what changed and why. Link to related docs in `docs/`.

## Checklist

- [ ] Code builds for ESP32‑S3 (`pio run`)
- [ ] Size budget check (`python3 scripts/verify_size_budget.py --baseline ci/size-baseline.json`)
- [ ] SSOT docs reviewed/updated (link to changed sections)
- [ ] Docs updated (if flags/commands/paths changed)
- [ ] Filenames follow naming conventions (ASCII, lowercase, underscores)
- [ ] Added/updated `docs/01.process/docs-changelog.md` if restructuring docs
- [ ] For tempo/router work: Features behind flags (`ENABLE_TEMPO_TRACKER`, `ENABLE_ROUTER_FSM`)

## Test Plan

List steps to validate. Include metrics/log outputs if relevant. Attach build/size logs.

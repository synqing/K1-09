# Docs Naming & Structure Standard (v1)

Purpose
- Make docs navigable for new agents in the order they gain context.
- Ensure the top-level `docs/` has only categories, no loose files (except `README.md`).

Top-level categories (ordered)
- `01.process/` — Engineering workflow, branching, naming, lint, changelog
- `02.streams/` — Streams & Gates workflow overlay (stream-01..N)
- `03.ssot/` — Single‑source‑of‑truth specs (pipelines, scheduling, magic numbers)
- `04.plans/` — Project/technical plans (grouped by initiative)
- `05.tracking/` — Issue scaffolds, matrices (markdown + json)
- `06.runbooks/` — Checklists, operational guides
- `07.integration/` — External integration docs
- `08.artifacts/` — Captured outputs (phase folders, dumps, logs)
- `99.reference/` — Indices, shim records, misc references

Conventions
- Use lowercase kebab‑case for filenames, with a `docType` suffix when it clarifies intent:
  - `*-ssot.md`, `*-plan.md`, `*-guide.md`, `*-checklist.md`, `*-runbook.md`, `*-issues.md|.json`, `overview.md`, `reference.md`
- Streams use numeric IDs: `01-tempo`, `02-router`, etc. Use in labels/branches.
- No spaces or unicode punctuation in filenames.
- Each category has a `README.md` index.

Migration policy
- Move loose files into the proper category.
- Update internal references when moved; avoid long‑lived duplicates.
- If needed, add a short deprecation stub for one release cycle that links to the new path.


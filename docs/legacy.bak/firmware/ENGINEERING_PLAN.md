# ENGINEERING PLAN — Tear‑Down → Split → Modularize → Productionize

**Objective:** Replace the legacy monolithic/header‑only design with a clean, modular, verifiable codebase while preserving 1:1 behavior during the transition.

**Scope rules:**

* **Mechanical phases (0–3)**: no new firmware functions/vars; no signature changes. Guardrails/tools/docs may add new code in `tools/**`, `scripts/**`, `/.github/**`, `docs/**`.
* **Feature phases (4+)**: additions allowed via explicit design PRs.

---

## Phase 0 — Guardrails & Infra (Done / Ongoing)

**Goals**

* Prevent “silent success” and initialize analyzer pipeline.

**Deliverables**

* `agent_runner.sh` (fail‑fast), `tools/aggregate_init_scanner.py` (STRICT + `.scannerignore`), `tools/deps_stub_report.py` (Mermaid/JSON/DOT), `.github/workflows/firmware-ci.yml`, `.clang-format`, `.clang-tidy`, `.gitignore` for analysis outputs.
* Branch protection & `CODEOWNERS` for high‑risk paths.

**Tasks (1–9)**

1. Fail‑fast runner (`agent_runner.sh`) with `set -Eeuo pipefail`, log CMD/EXIT.
2. Analyzer emits Mermaid/JSON/DOT; fail fast on missing roots.
3. CI routes all steps via runner; hard‑checks dependency artifacts; records `compile_commands.json`.
4. Scanner: exit 2 on bad roots; **strict** default; legacy requires `--allow-legacy`.
5. `.gitignore` ignores `analysis/*` and `compile_commands.json`.
6. `.clang-format` / `.clang-tidy` baselines.
7. Add `CODEOWNERS` (review required on `/include`, `/src`, `/config`, `/tools`, `/.github`).
8. Enable branch protection (CI green + 1 review for `main`).
9. Add Agent Runbook policy (artifact checks, no meta logs, all shell via runner).

**Success criteria**

* CI fails on any missing artifacts or scanner violations.
* Local instructions in AGENT\_RUNBOOK followed exactly.

---

## Phase 1 — Mechanical Aggregate‑Init Remediation

**Goals**

* Eliminate STRICT scanner violations without behavior change.

**Tasks (10–13)**
10\. Inventory flat‑brace triples/quads across `src/` and `include/`.
11\. Apply guarded `comby` rewrites to nest braces for CRGB16/CRGBA16 (and aliases) only.
12\. Re‑run scanner/build until green.
13\. Open a single PR: `refactor(init): nest aggregate braces for 16‑bit color types (mechanical)`.

**Success criteria**

* Scanner STRICT passes across `src/` and `include/`.
* No firmware logic diffs.

---

## Phase 2 — Header Split at Scale (Many Small PRs)

**Goals**

* Convert header‑only code into proper `.h/.cpp` pairs; reduce include cycles.

**Tasks (14–18)**
14\. Confirm issue template/labels exist (`header-split`, `mechanical`, `safe-change`).
15\. Seed issues from `analysis/deps_report.json` using `scripts/open_header_split_issues.py` (top risk by cycles).
16\. Execute 1–2 splits at a time locally via GPT‑Pro; scale with async agent if needed.
17\. Per‑PR validation: build, scanner strict, regenerate dependency report (hard‑check artifacts).
18\. Optional IWYU pass with `compiledb` to tighten includes.

**Deliverables**

* PRs: `refactor(header): split <file> into .h/.cpp (mechanical)` with scanner/build/dep‑report proof.

**Success criteria**

* CI green on each PR; dependency cycles count trending down.

---

## Phase 3 — Modularization (Interfaces Stabilized)

**Goals**

* Introduce explicit module boundaries and public headers for cross‑module contracts.

**Tasks (19–21)**
19\. Enforce layout: `/include/<mod>/*.h` (APIs), `/src/<mod>/*.cpp` (impl), `/config/*.hpp` (tunables with ranges).
20\. Create boundary headers (e.g., `include/pipeline/audio_to_visual.hpp`) containing only types/constants and documentation comments.
21\. Clang‑tidy focus: `misc-header-guard`, `misc-include-cleaner`, `bugprone-*`, `performance-*`.

**Success criteria**

* Modules compile with minimal includes; boundary headers provide a complete contract.

---

## Phase 4 — Single Source of Truth (Docs)

**Goals**

* Map Audio + Visual pipelines, producers/consumers, data types, and magic numbers with ranges and failure modes.

**Tasks (22–24)**
22\. Scaffold docs site (`docs/`, MkDocs) and (optionally) publish on every merge.
23\. Generate pipeline inventories (scripted scans of structs/enums/constants at boundaries); write Magic Numbers table with ranges & failure modes.
24\. Cross‑link code → docs via `// DOC:` anchors.

**Deliverables**

* `docs/ENGINEERING_PLAN.md`, `docs/AGENT_RUNBOOK.md`, `docs/pipelines/*` with diagrams (Mermaid acceptable initially).

**Success criteria**

* You can answer “what is the type/range for X and who consumes it?” from the docs alone.

---

## Phase 5 — Productionization Plumbing

**Goals**

* Make the build reproducible and releasable.

**Tasks (25–26)**
25\. CI stages: build → scanner → dep report → tidy check → docs build (optional) → artifacts upload.
26\. Versioning: conventional commits, auto‑changelog, signed tags, embed version via linker flags (no new globals).

**Success criteria**

* Reproducible builds; artifacts downloadable from CI; version traceable to commit.

---

## Phase 6 — Full Dependency Analyzer (Upgrade from Stub)

**Goals**

* Build the libclang + NetworkX analyzer for cycles, include impact, and ODR checks with machine‑readable output.

**Tasks (27)**
27\. Implement analyzer: parse files/includes/symbols; detect cycles & ODR heuristics; compute impact radius; emit JSON + Mermaid/DOT; add `validate-split <file>` scoring.

**Success criteria**

* Analyzer flags cycle reductions; PRs include before/after impact metrics.

---

## Timeline (suggested)

* **Week 1**: Phase 0 complete; Phase 1 aggregate‑init PR opened. (Docs scaffold from Phase 4 may start in parallel but is non‑blocking.)
* **Week 2**: Phase 1 merged; open first 5 header‑split issues; begin Phase 2 PRs.
* **Week 3–4**: Phase 2 steady cadence (5–15 mechanical PRs total); start Phase 3 skeletons.
* **Month 2**: Phase 3 stabilization; Phase 5 CI refinements; begin Phase 6 analyzer; expand Phase 4 docs.

---

## Definition of Done (per PR)

* [ ] Build OK (`pio run`)
* [ ] Scanner strict OK
* [ ] Dependency artifacts regenerated and attached
* [ ] No new firmware functions/vars; no signature changes (until Phase 4+)
* [ ] Linked issue with scope and checklist

---

## Governance

* **CODEOWNERS** require review for `/include/**`, `/src/**`, `/config/**`, `/tools/**`, `/.github/**`.
* Branch protection: CI must pass; require review from owner.
* Agent access: async agents allowed only for mechanical tasks with this runbook.

---

## Appendix — Commands (quick reference)

```bash
# Build
./agent_runner.sh pio run

# Deps report
./agent_runner.sh python3 tools/deps_stub_report.py src include lib

# Scanner (strict)
./agent_runner.sh python3 tools/aggregate_init_scanner.py --mode=strict --roots src include lib

# Compiledb for tooling
./agent_runner.sh pio run -t compiledb

# Generate 5 header-split issues
./agent_runner.sh python3 scripts/open_header_split_issues.py
```

# Diagnostic‑First Rubric

Principles
- Measure before you modify; turn hypotheses into single observable lines.
- Prefer runtime‑toggleable instrumentation.
- Compare A/B with minimal confounders.

Checklist
- What’s the smallest thing to measure to validate the hypothesis?
- Can we toggle it at runtime to compare A/B?
- What one line of telemetry will tell a stranger what’s happening?

Examples
- `[acf] mix:{...} hm:{...}` top peaks reveal whether faster pulses exist.
- `[cand] ... pick=...` prints lane scores and the resolver decision.
- `[perf] tick_us=...` confirms budget adherence without flooding logs.


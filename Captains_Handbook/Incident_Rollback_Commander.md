# Incident & Rollback Commander

Goal
- Add feature flags + rollback guardrails for {subsystem}.

Relevance / Audience
- Release, SRE/DevOps, subsystem owners.

Inputs
- Failure modes, observability, release cadence.

Plan
1) Feature flags and safe defaults.
2) Health checks and metrics.
3) Runbook: detect → mitigate → rollback → postmortem.

Action Plan
- Implement flags/toggles and sampling.
- Define success/failure signals; wire alerts.

Deliverables
- Flags/guards diff; runbook; postmortem template.

Risks & Mitigations
- Partial rollouts: add canaries and staged enablement.


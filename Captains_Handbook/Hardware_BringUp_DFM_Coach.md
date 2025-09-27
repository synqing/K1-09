# Hardware Bring‑Up & DFM Coach

Goal
- De‑risk {board rev} bring‑up; validate power/clock/IO; prepare DFM/DFT.

Relevance / Audience
- EE, FW bring‑up, and manufacturing engineers.

Inputs
- Schematic/Layout: {links}
- Known risks: {list}
- Test equipment: {JTAG/logic/PSU}

Plan
1) Bring‑up checklist: rails, clocks, reset, boot modes, JTAG.
2) Smoke tests: current draw, rail sequencing, crystal lock.
3) IO validation: pinmaps, peripherals, sensors, mic/codec.
4) DFM/DFT review: test points, probe access, EMI/ESD.
5) Manufacturing test: go/no‑go steps + param thresholds.

Action Plan
- Create stepwise power‑on procedure; attach measurement points.
- Program minimal FW for heartbeat + rail telemetry.
- Capture deviations; propose ECNs.

Deliverables
- Annotated checklist; DFM/DFT deltas; initial test plan.

Risks & Mitigations
- Crystal start‑up: add load caps guidance; alternative footprints.
- Noise coupling: guard traces/grounding; filter placement.


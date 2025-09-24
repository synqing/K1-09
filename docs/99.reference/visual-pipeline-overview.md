# Visual Pipeline Overview

High-level map of the visual path. For authoritative details see the SSOT:
- Pipelines: `docs/03.ssot/firmware-pipelines-ssot.md`
- Scheduling: `docs/03.ssot/firmware-scheduling-ssot.md`

## Stages (summary)
- Coordination: Coupling plan computed each frame
- Render: Primary/secondary modes produce CRGB16 buffers
- Quantize/Show: Brightness, quantization, async flip with safety guards

# K1-09 Firmware

This repository contains the K1-09 firmware. CI runs PlatformIO builds and generates analysis artifacts for dependency graphs and an authoritative pipeline map.

Developer notes
- Firmware CI can be manually triggered from Actions (workflow_dispatch enabled).
- Analysis artifacts are uploaded by CI and are not committed to the repo.

CI heartbeat: this change is a no-op to retrigger required status checks.

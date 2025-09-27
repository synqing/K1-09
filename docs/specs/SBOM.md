# Software Bill of Materials (SBOM) & Dependency Manifest

Scope: Firmware branch `feature/lc-transplant-plan` for ESP32-S3 Sensory Bridge. Generated from `platformio.ini`, `lib/`, and `libraries/` directories.

---

## 1. Platform & Toolchain Components
| Component | Version / Pin | Source | Notes |
|-----------|---------------|--------|-------|
| PlatformIO Platform `espressif32` | `6.1.0` | `platformio.ini` | Provides ESP-IDF/Arduino tooling for ESP32 family. |
| Framework `framework-arduinoespressif32` | `3.20009.0` (Arduino 2.0.9) | `platformio.ini` (`platform_packages`) | Arduino core for ESP32-S3. |
| Toolchain `toolchain-xtensa-esp32s3` | `8.4.0+2021r2-patch5` | PlatformIO (auto) | GCC for Xtensa LX7. |
| Toolchain `toolchain-riscv32-esp` | `8.4.0+2021r2-patch5` | PlatformIO (auto) | Needed by Arduino core for support tasks. |
| Utility `tool-esptoolpy` | `4.5.0` | PlatformIO | Flashing utility. |

## 2. External Libraries (PlatformIO `lib_deps`)
| Library | Version | License | Usage |
|---------|---------|---------|-------|
| FastLED | `3.9.2` (`fastled/FastLED@3.9.2`) | MIT | LED driving, color utilities (`src/VP/vp_renderer.cpp`). |
| FixedPoints | `1.0.3` (`pharap/FixedPoints@^1.0.3`) | MIT | Fixed-point math support (legacy modules, available for extensions). |
| M5ROTATE8 | `0.4.1` (`robtillaart/M5ROTATE8@^0.4.1`) | MIT | Helper for display rotation (currently unused but kept for compatibility). |

## 3. Local / Vendored Libraries (`lib/`, `libraries/`)
| Path | Description | License (if known) |
|------|-------------|---------------------|
| `lib/M5ROTATE8` | Custom or patched version of Rob Tillaart's M5 rotate library. | MIT (per upstream). |
| `lib/M5UnitScroll` | Support for M5 Unit scroll hardware. | MIT (per upstream). |
| `libraries/FastLED` | Vendored FastLED source. | MIT. |
| `libraries/FixedPoints` | Vendored FixedPoints library. | MIT. |
| `libraries/M5ROTATE8` | Additional variant of M5ROTATE8. | MIT. |
| `libraries/ArduinoJson` | ArduinoJson library. | MIT (v6). |
| `libraries/Async_TCP` | Async TCP library for ESP32. | LGPL-3.0. |
| `libraries/PsychicHttp` | Psychic HTTP server stack. | Apache 2.0 (per repo). |
| `libraries/UrlEncode` | URL encoding lib. | MIT. |
| `libraries/Lightshow_Implementation_Plan.md` | Documentation artefact. | n/a. |

> Note: Local libraries remain ignored in online minimal branch but tracked here for completeness. When packaging releases, verify which libraries are actually referenced in build (current firmware chiefly uses FastLED; Async_TCP/PsychicHttp are legacy artefacts).

## 4. First-Party Modules
| Module | Directory | Responsibility |
|--------|-----------|----------------|
| Layer 1 Mic | `src/AP/sph0645.*` | I2S mic bring-up and chunk acquisition. |
| Audio Producer | `src/audio/` | AudioFrame computation and publication. |
| Visual Pipeline | `src/VP/` | Frame acquisition, LED rendering, HMI proxies. |
| Debug UI | `src/debug/` | Serial debug toggles, telemetry. |
| Storage | `src/storage/NVS.*` | Debounced NVS access. |
| Main App | `src/main.cpp` | Loop orchestration. |

## 5. Build Flags & Config Files
- `platformio.ini`: build definitions, compiler flags, upload parameters.
- `partitions_4mb.csv`: flash layout (app, OTA, NVS).
- `.gitignore`: excludes non-build directories from published repo (docs/specs explicitly allowed).

## 6. Licensing Summary
| License | Components |
|---------|------------|
| MIT | FastLED, FixedPoints, M5ROTATE8, ArduinoJson, UrlEncode, project code (default). |
| LGPL-3.0 | Async_TCP (unused in current firmware). |
| Apache-2.0 | PsychicHttp (legacy). |

> Ensure compliance by including licenses in firmware distribution or referencing upstream repositories when bundling libraries.

## 7. Vulnerability & Update Tracking
- Monitor FastLED releases for ESP32 compatibility updates (currently 3.9.2).
- Arduino-ESP32 framework pinned to 2.0.9; check Espressif advisories for security patches.
- Async_TCP/PsychicHttp only needed if legacy networking re-enabled; otherwise they can remain ignored to reduce SBOM surface.

## 8. Hashes / Provenance (optional)
For reproducible builds, capture commit hashes of local libraries or reference Git submodules if reintroduced. Current repo stores full sources; provenance should be tracked in release notes.

---


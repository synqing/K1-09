# Visual Studio Testing & Diagnostics Setup

## 1. Install Tooling
- Visual Studio 2022+ with "Desktop development with C++" and "Linux and embedded development" workloads.
- (Optional) VisualGDB extension for ESP-IDF aware flashing, RTOS views, and Unity ↔ Test Explorer integration.
- Espressif ESP-IDF 5.x toolchain. Verify `idf.py build && idf.py flash` for a sample project before moving on.
- CMake & Ninja (VS installer can add both). These power the host-side GoogleTest project.

## 2. Host-Side Unit Tests (PC)
- Open VS → `File → Open → CMake...` and select `tests/host/CMakeLists.txt`.
- Choose the `vs-debug` preset when prompted (backed by `CMakePresets.json`).
- VS will configure, build `host_tests`, and populate **Test Explorer** automatically.
- Run/Debug tests:
  - `Test → Test Explorer → Run All` or right-click individual cases.
  - Breakpoints work like any desktop C++ project.
- Extend coverage by adding more files under `tests/host/` and including firmware headers that are hardware-free or have stubs.

### What is Exercised Right Now?
- `AudioProcessedState` lifecycle, peak tracking, and safety checks. Add more suites (e.g., DSP filters) as you refactor shared logic out of Arduino-only modules.

## 3. Firmware/Unity Tests (ESP32-S3)
Two options are scaffolded:

- PlatformIO on-target tests (Unity for Arduino):
  - Tests live under `test/pio/...`. Example: `test/pio/test_utilities/test_main.cpp` exercises `utilities.h` functions.
  - Run on device: `pio test -e esp32-s3-devkitc-1` (flashes a test sketch, executes, reports Unity results).
  - Serial speed is `230400` per `platformio.ini`.

- Hardware-in-the-loop via serial (pytest):
  - Location: `tests/hil/` with `requirements.txt`.
  - Setup: `python -m pip install -r tests/hil/requirements.txt`.
  - Run: `SB_SERIAL_PORT=/dev/cu.usbmodemXXXX pytest -q tests/hil`.
  - What it does: Sends `PERF`/`PERF STRESS` commands and asserts frame time < 8000 µs.

## 4. Hardware-in-the-Loop & Performance
- Create Python/pytest harnesses in `tests/hil/` leveraging `pyserial` or Espressif's `pytest-embedded` plugin.
- Automate latency assertions by parsing UART logs or `esp_apptrace` data; flag any frame >8 ms.
- Recommended tools: Saleae logic analyzer, SEGGER SystemView (with J-Link), or ESP-IDF Trace Viewer for FreeRTOS scheduling insight.

## 5. Debug & Logging Essentials
- Debug probe: ESP-PROG or the ESP32-S3 DevKit's USB-JTAG path.
- OpenOCD + `idf.py openocd` (or VisualGDB session) for RTOS thread view and breakpoints.
- Serial logging: `idf.py monitor` or Visual Studio's serial window; control verbosity via `ESP_LOGx` macros and `esp_log_level_set`.
- Profiling: `esp_apptrace`, `esp_timer_get_time` checkpoints, plus optional InfluxDB/Grafana dashboards for long-run latency monitoring.

## 6. Next Steps Checklist
- [ ] Run host tests (`tests/host`) and add more cases covering DSP/LED mapping.
- [ ] Add more PlatformIO Unity tests for LED driver, audio math, and any custom allocators.
- [ ] Script hardware-in-loop regression (`pytest-embedded`) to guard the 8 ms frame budget.
- [ ] Document how to switch between PlatformIO builds and Visual Studio host tests so teammates stay in sync.

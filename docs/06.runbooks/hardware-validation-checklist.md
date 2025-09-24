# Sensory Bridge Hardware Validation Checklist

This checklist captures the minimum smoke tests required after firmware changes. Update the **Results** column with links to logs or notes for each run.

| Test ID | Category | Description | Procedure | Result |
|---------|----------|-------------|-----------|--------|
| HV-LED-01 | Visual Pipeline | Confirm LED main loop renders baseline modes without artifacts. | Cycle presets `lightshow_modes.h` via serial menu (`mode` command) while observing for color corruption, frame drops, or stuck pixels. | Pending |
| HV-AUD-01 | Audio Pipeline | Validate microphone capture path and FFT output sanity. | Run serial diagnostics (`audio diag` command) while playing swept-tone audio; verify noise floor and peak bins against expected ranges in serial log. | Pending |
| HV-SYNC-01 | P2P Communication | Ensure ESP-NOW peer discovery and sync events propagate. | Pair two units, trigger sync broadcast (`sync` command), monitor for mirrored state updates. | Pending |
| HV-HMI-01 | HMI | Exercise encoder inputs for responsiveness and debouncing. | Rotate both channels of the dual encoder controller and press buttons; verify state transitions and serial telemetry. | Pending |
| HV-TRACE-01 | Debug Instrumentation | Confirm Mabutrace/trace pipeline captures events without stalls. | Enable `TRACE_CAT_LED | TRACE_CAT_CRITICAL` in `performance_optimized_trace.h`, run for 5 minutes, ensure no buffer overflows. | Pending |
| HV-POWER-01 | Power Budget | Validate current limiter behaviour vs package spec. | Set `g_current_limiter_enabled=true`; drive full-white and bright modes; measure supply current with DMM; verify limiter scales to <= `CONFIG.MAX_CURRENT_MA`. Adjust `CURRENT_LIMITER_MA_PER_CHANNEL` for your LED package (e.g., 5050≈20 mA/channel, 2020/3535 lower). | Pending |

## Test Notes
- Update individual Task-Master tickets with log references or anomalies encountered.
- If any test fails, create a remediation sub-task before merging.

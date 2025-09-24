// phase3_guards.h — Compile-time and light runtime guards for Phase 3
//
// PURPOSE
// - Encode key Phase 3 operating constraints as compile-time checks so that
//   inadvertent changes are caught at build time (fast feedback, no regressions).
// - Keep these guards in one place for easy review; include from a central header.
//
// SCOPE
// - LED geometry and strip parity (primary/secondary counts must match 160).
// - DSP bin layout stability (64 bins).
// - These asserts are cheap (compile-time) and impose no runtime cost.

#pragma once

#include "constants.h"

// Phase 3 requires 160-pixel native working resolution
static_assert(NATIVE_RESOLUTION == 160,
              "Phase3 guard: NATIVE_RESOLUTION must be 160");

// Secondary strip must match primary geometry (templates depend on compile-time constants)
static_assert(SECONDARY_LED_COUNT_CONST == NATIVE_RESOLUTION,
              "Phase3 guard: Secondary LED count must match primary");

// GDFT plan is tuned for 64 bins (performance + memory budget)
static_assert(NUM_FREQS == 64,
              "Phase3 guard: NUM_FREQS must be 64 for budget/perf");


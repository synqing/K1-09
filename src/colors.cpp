// colors.cpp
// Purpose: define CRGB16 lookups using safe aggregate initialization.
// NOTE: Do not include Arduino/FastLED here; keep it lean.
#include "constants.h"

// Incandescent filter curve (normalized 0..1) — SAFE PATTERN
// CRITICAL: Uses aggregate initialization without explicit SQ15x16() constructors
// This prevents the C++17 aggregate initialization trap that broke LED Channel 1
// MOVED TO globals.cpp to fix ODR violation - definition removed here
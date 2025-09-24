/*----------------------------------------
  Sensory Bridge HARDWARE CONFIGURATION
----------------------------------------*/

// Every notable setting about hardware moved to the settings.sensorybridge.rocks site for now!
// No more hardcoding values for strip length and such

/*----------------------------------------
  Sensory Bridge SOFTWARE CONFIGURATION
----------------------------------------*/

// TODO: CRGBPalette16/lookups in place of all CHSV usage, which defaults
//       to a rainbow but can be swapped with several pre-made themes by
//       commenting/uncommenting one here or selecting via the UART menu

// Color pipeline guard: enables gamma-aware HSV that outputs linear CRGB16
// Default OFF per workflow; testers can flip to 1 locally when validating.
#ifndef ENABLE_NEW_COLOR_PIPELINE
#define ENABLE_NEW_COLOR_PIPELINE 0
#endif

// Future façade adoption guard (placeholder): toggle for migrating modes to palette_facade helpers.
#ifndef ENABLE_PALETTE_FACADE
#define ENABLE_PALETTE_FACADE 0
#endif

// Phase 3 experimental toggles (default OFF; no behavior change until explicitly enabled)
// If set to 1, Agent A can try alternate dithering policy (e.g., bypass custom 8-step)
#ifndef ENABLE_DITHER_POLICY_EXPERIMENT
#define ENABLE_DITHER_POLICY_EXPERIMENT 0
#endif

// If set to 1, log once/second min/max after master brightness for field parity checks
#ifndef ENABLE_BRIGHTNESS_METRICS
#define ENABLE_BRIGHTNESS_METRICS 0
#endif

// If set to 1, allow palette-mode incandescent tint experiments (policy TBD); keep 0 by default
#ifndef ENABLE_INCANDESCENT_FOR_PALETTE
#define ENABLE_INCANDESCENT_FOR_PALETTE 0
#endif

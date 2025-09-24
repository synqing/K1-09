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

// Color Refit: collaborative rollout guards
// ENABLE_NEW_COLOR_PIPELINE — when 1, HSV path outputs linear CRGB16 (gamma-aware) to match palette LUTs.
// Default OFF until Phase 4 acceptance; all PR slices must be releasable with this set to 0.
#ifndef ENABLE_NEW_COLOR_PIPELINE
#define ENABLE_NEW_COLOR_PIPELINE 0
#endif

// Future façade adoption guard (placeholder): toggle for migrating modes to palette_facade helpers.
#ifndef ENABLE_PALETTE_FACADE
#define ENABLE_PALETTE_FACADE 0
#endif

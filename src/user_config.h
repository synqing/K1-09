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

// Colour pipeline retrofit guard (Phase 1):
// When enabled (1), use gamma-aware HSV → linear conversion to match palette LUT behaviour.
// Set to 0 to revert to the legacy HSV path instantly.
#ifndef ENABLE_NEW_COLOR_PIPELINE
// NOTE: This guard enables a gamma‑aware HSV → linear path that aligns HSV with
// palette LUT behaviour. It is intentionally DISABLED (0) during documentation
// and planning to avoid changing runtime visuals. Do NOT remove the guarded
// code — it is part of the planned Colour Pipeline Phase 1 retrofit and will be enabled when
// development begins.
#define ENABLE_NEW_COLOR_PIPELINE 0
#endif

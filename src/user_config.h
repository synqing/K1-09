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

#ifndef SB_ENABLE_LC_RENDER
#define SB_ENABLE_LC_RENDER 1
#endif

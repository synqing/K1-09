#include "../led_utilities.h"

// Provide concrete storage for lerp params with full type
static LerpParams g_led_lerp_params[160];
LerpParams* led_lerp_params = g_led_lerp_params;
bool lerp_params_initialized = false;


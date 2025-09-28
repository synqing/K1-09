#pragma once
#include <stdint.h>
#include "driver/gpio.h"

/* ============================================================================
   beat_gpio.h — scope-friendly beat pulse on a chosen GPIO:
     - call beat_gpio_init(pin)
     - call beat_gpio_pulse() whenever you emit a beat (non-blocking)
   Default pulse is a very short toggle (high→low) suitable for logic analyzers.
   Change to a timed pulse if you need a visible LED (see comment in .cpp).
   ==========================================================================*/

namespace beat_gpio {

bool beat_gpio_init(gpio_num_t pin);
void beat_gpio_pulse();

} // namespace beat_gpio

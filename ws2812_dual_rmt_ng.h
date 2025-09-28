#pragma once
#include <stdint.h>
#include "driver/rmt_tx.h"     // IDF 5.x new RMT TX driver
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { uint8_t r, g, b; } CRGB;

/** Create two RMT TX channels (new driver) for WS2812 on given pins.
 *  leds_per_line: number of LEDs per strip (same on both lines).
 */
bool ws2812_ng_init(gpio_num_t pin0, gpio_num_t pin1, int leds_per_line);

/** Blocking render of two lines in parallel (no pacing). */
bool ws2812_ng_show_dual(const CRGB* line0, const CRGB* line1);

/** Paced render: enforces ~fixed frame spacing (target_us). */
bool ws2812_ng_show_dual_paced(const CRGB* line0, const CRGB* line1, uint32_t target_us);

/** Return the worst-case transmit time (microseconds) for the configured length. */
uint32_t ws2812_ng_frame_time_us(void);

#ifdef __cplusplus
}
#endif

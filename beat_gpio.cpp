#include "beat_gpio.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"

namespace beat_gpio {

static gpio_num_t s_pin = GPIO_NUM_NC;
static bool s_inited = false;

bool beat_gpio_init(gpio_num_t pin) {
  s_pin = pin;
  if (s_pin == GPIO_NUM_NC) return false;
  gpio_config_t cfg = {};
  cfg.mode = GPIO_MODE_OUTPUT;
  cfg.intr_type = GPIO_INTR_DISABLE;
  cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
  cfg.pull_up_en = GPIO_PULLUP_DISABLE;
  cfg.pin_bit_mask = (1ULL << (int)s_pin);
  esp_err_t err = gpio_config(&cfg);
  if (err != ESP_OK) return false;
  gpio_set_level(s_pin, 0);
  s_inited = true;
  return true;
}

void beat_gpio_pulse() {
  if (!s_inited) return;
  // Instant high→low toggle. On a logic analyzer this is a clear edge.
  // If you want a visible LED, replace with: set high and schedule a deferred low.
  gpio_set_level(s_pin, 1);
  // one or two NOPs is enough for analyzers; keep it non-blocking
  __asm__ __volatile__("nop;nop;nop;");
  gpio_set_level(s_pin, 0);
}

} // namespace beat_gpio

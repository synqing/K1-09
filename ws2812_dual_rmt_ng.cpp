#include "ws2812_dual_rmt_ng.h"
#include <string.h>
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ===== WS2812 timings @ 800 kHz, RMT resolution = 80 MHz (12.5 ns/tick)
static inline uint16_t ticks_from_ns(uint32_t ns) { return (uint16_t)((ns * 80 + 999) / 1000); }
static const uint16_t T0H = ticks_from_ns(400);  // 0.40 us
static const uint16_t T0L = ticks_from_ns(850);  // 0.85 us
static const uint16_t T1H = ticks_from_ns(800);  // 0.80 us
static const uint16_t T1L = ticks_from_ns(450);  // 0.45 us
static const uint32_t TRESET_NS = 60000;         // >= 50 us low
static const uint16_t TRESET_TICKS = ticks_from_ns(TRESET_NS);

typedef rmt_symbol_word_t sym_t;

static rmt_channel_handle_t s_tx0 = nullptr;
static rmt_channel_handle_t s_tx1 = nullptr;
static rmt_encoder_handle_t s_copy_enc = nullptr;
static rmt_transmit_config_t s_tx_cfg = { .loop_count = 0 };

static sym_t* s_items0 = nullptr;
static sym_t* s_items1 = nullptr;
static size_t s_items_per_line = 0;
static int    s_leds = 0;

static uint64_t s_next_deadline = 0;

// Build one byte → 8 symbols (MSB first) at out[0..7]
static inline void encode_byte(uint8_t v, sym_t* out) {
  for (int i = 7; i >= 0; --i) {
    const bool one = (v >> i) & 1;
    sym_t& o = out[7 - i];
    if (one) { o.level0 = 1; o.duration0 = T1H; o.level1 = 0; o.duration1 = T1L; }
    else     { o.level0 = 1; o.duration0 = T0H; o.level1 = 0; o.duration1 = T0L; }
  }
}
static inline void encode_led(const CRGB& c, sym_t* out, size_t& idx) {
  encode_byte(c.g, &out[idx]); idx += 8;
  encode_byte(c.r, &out[idx]); idx += 8;
  encode_byte(c.b, &out[idx]); idx += 8;
}
static inline void append_reset(sym_t* out, size_t& idx) {
  out[idx].level0 = 0; out[idx].duration0 = TRESET_TICKS;
  out[idx].level1 = 0; out[idx].duration1 = 0; idx += 1;
}
static inline void encode_line(const CRGB* src, sym_t* out) {
  size_t idx = 0;
  for (int i = 0; i < s_leds; ++i) encode_led(src[i], out, idx);
  append_reset(out, idx);
}

bool ws2812_ng_init(gpio_num_t pin0, gpio_num_t pin1, int leds_per_line) {
  if (leds_per_line <= 0) return false;
  s_leds = leds_per_line;

  rmt_tx_channel_config_t ch_cfg = {
    .gpio_num = pin0,
    .clk_src = RMT_CLK_SRC_DEFAULT,
    .mem_block_symbols = 64,
    .resolution_hz = 80 * 1000 * 1000,
    .trans_queue_depth = 4,
    .flags = { .invert_out = 0, .with_dma = 0 }
  };
  ESP_RETURN_ON_ERROR(rmt_new_tx_channel(&ch_cfg, &s_tx0), "WS2812", "tx0 new failed");

  ch_cfg.gpio_num = pin1;
  ESP_RETURN_ON_ERROR(rmt_new_tx_channel(&ch_cfg, &s_tx1), "WS2812", "tx1 new failed");

  rmt_copy_encoder_config_t enc_cfg = {};
  ESP_RETURN_ON_ERROR(rmt_new_copy_encoder(&enc_cfg, &s_copy_enc), "WS2812", "copy enc failed");

  ESP_RETURN_ON_ERROR(rmt_enable(s_tx0), "WS2812", "enable tx0 failed");
  ESP_RETURN_ON_ERROR(rmt_enable(s_tx1), "WS2812", "enable tx1 failed");

  s_items_per_line = (size_t)leds_per_line * 24u + 1u;
  s_items0 = (sym_t*)heap_caps_malloc(s_items_per_line * sizeof(sym_t), MALLOC_CAP_INTERNAL);
  s_items1 = (sym_t*)heap_caps_malloc(s_items_per_line * sizeof(sym_t), MALLOC_CAP_INTERNAL);
  if (!s_items0 || !s_items1) return false;

  s_next_deadline = esp_timer_get_time();
  return true;
}

uint32_t ws2812_ng_frame_time_us(void) { return (uint32_t)(s_leds * 30 + 80); }

bool ws2812_ng_show_dual(const CRGB* line0, const CRGB* line1) {
  if (!s_tx0 || !s_tx1) return false;
  encode_line(line0, s_items0);
  encode_line(line1, s_items1);
  ESP_RETURN_ON_ERROR(rmt_transmit(s_tx0, s_copy_enc, s_items0, s_items_per_line * sizeof(sym_t), &s_tx_cfg), "WS2812", "tx0 tx fail");
  ESP_RETURN_ON_ERROR(rmt_transmit(s_tx1, s_copy_enc, s_items1, s_items_per_line * sizeof(sym_t), &s_tx_cfg), "WS2812", "tx1 tx fail");
  ESP_RETURN_ON_ERROR(rmt_tx_wait_all_done(s_tx0, -1), "WS2812", "tx0 wait fail");
  ESP_RETURN_ON_ERROR(rmt_tx_wait_all_done(s_tx1, -1), "WS2812", "tx1 wait fail");
  return true;
}

bool ws2812_ng_show_dual_paced(const CRGB* line0, const CRGB* line1, uint32_t target_us) {
  if (!s_tx0 || !s_tx1) return false;
  const uint64_t now = esp_timer_get_time();
  if ((int64_t)(s_next_deadline - now) > 500) {
    while ((int64_t)(s_next_deadline - esp_timer_get_time()) > 0) taskYIELD();
  } else if ((int64_t)(now - s_next_deadline) > (int64_t)target_us) {
    s_next_deadline = now;
  }
  encode_line(line0, s_items0);
  encode_line(line1, s_items1);
  ESP_RETURN_ON_ERROR(rmt_transmit(s_tx0, s_copy_enc, s_items0, s_items_per_line * sizeof(sym_t), &s_tx_cfg), "WS2812", "tx0 tx fail");
  ESP_RETURN_ON_ERROR(rmt_transmit(s_tx1, s_copy_enc, s_items1, s_items_per_line * sizeof(sym_t), &s_tx_cfg), "WS2812", "tx1 tx fail");
  ESP_RETURN_ON_ERROR(rmt_tx_wait_all_done(s_tx0, -1), "WS2812", "tx0 wait fail");
  ESP_RETURN_ON_ERROR(rmt_tx_wait_all_done(s_tx1, -1), "WS2812", "tx1 wait fail");
  s_next_deadline += target_us;
  return true;
}

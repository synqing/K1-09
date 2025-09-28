#include "ws2812_dual_rmt.h"
#include <string.h>
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "driver/rmt_encoder.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ====== WS2812 timings at 800 kHz, RMT resolution = 80 MHz (12.5 ns/tick)
static inline uint16_t ticks_from_ns(uint32_t ns) { return (uint16_t)((ns * 80 + 999) / 1000); }
static const uint16_t T0H = ticks_from_ns(400);  // 0.40 us
static const uint16_t T0L = ticks_from_ns(850);  // 0.85 us
static const uint16_t T1H = ticks_from_ns(800);  // 0.80 us
static const uint16_t T1L = ticks_from_ns(450);  // 0.45 us
static const uint32_t TRESET_NS = 60000;         // >= 50 us low
static const uint16_t TRESET_TICKS = ticks_from_ns(TRESET_NS);

// New-driver symbol type (alias)
typedef rmt_symbol_word_t sym_t;

// Encoder & channel state
static rmt_channel_handle_t s_tx0 = nullptr;
static rmt_channel_handle_t s_tx1 = nullptr;
static rmt_encoder_handle_t s_copy_enc = nullptr;   // copy encoder reused
static rmt_transmit_config_t s_tx_cfg = {};

static sym_t* s_items0 = nullptr;
static sym_t* s_items1 = nullptr;
static size_t s_items_per_line = 0;
static int    s_leds = 0;

static uint64_t s_next_deadline = 0;

// Build one byte → 8 symbols (MSB first) at out[0..7]
static inline void encode_byte(uint8_t v, sym_t* out) {
  for (int i = 7; i >= 0; --i) {
    const bool one = (v >> i) & 1;
    if (one) {
      out[7 - i].level0 = 1; out[7 - i].duration0 = T1H;
      out[7 - i].level1 = 0; out[7 - i].duration1 = T1L;
    } else {
      out[7 - i].level0 = 1; out[7 - i].duration0 = T0H;
      out[7 - i].level1 = 0; out[7 - i].duration1 = T0L;
    }
  }
}

// Encode one LED (GRB) => 24 symbols at out[idx..idx+23]
static inline void encode_led(const CRGB& c, sym_t* out, size_t& idx) {
  encode_byte(c.g, &out[idx]); idx += 8;
  encode_byte(c.r, &out[idx]); idx += 8;
  encode_byte(c.b, &out[idx]); idx += 8;
}

static inline void append_reset(sym_t* out, size_t& idx) {
  // One long low symbol; many strips need >= 50us; give 60us.
  out[idx].level0 = 0; out[idx].duration0 = TRESET_TICKS;
  out[idx].level1 = 0; out[idx].duration1 = 0;      // not used
  idx += 1;
}

bool ws2812_ng_init(gpio_num_t pin0, gpio_num_t pin1, int leds_per_line) {
  if (leds_per_line <= 0) return false;
  s_leds = leds_per_line;

  // --- Create TX channels (new driver) ---
  rmt_tx_channel_config_t ch_cfg{};
  ch_cfg.gpio_num = pin0;
  ch_cfg.clk_src = RMT_CLK_SRC_DEFAULT;
  ch_cfg.resolution_hz = 80 * 1000 * 1000;   // 80 MHz
  ch_cfg.mem_block_symbols = 64;
  ch_cfg.trans_queue_depth = 4;
  ch_cfg.intr_priority = 0;
  ch_cfg.flags.invert_out = 0;
  ch_cfg.flags.with_dma = 0;
  ch_cfg.flags.io_loop_back = 0;
  ch_cfg.flags.io_od_mode = 0;
  ch_cfg.flags.allow_pd = 0;
  ESP_RETURN_ON_ERROR(rmt_new_tx_channel(&ch_cfg, &s_tx0), "WS2812", "tx0 new failed");

  ch_cfg.gpio_num = pin1;
  ESP_RETURN_ON_ERROR(rmt_new_tx_channel(&ch_cfg, &s_tx1), "WS2812", "tx1 new failed");

  // --- Copy encoder (just transmit prebuilt symbols) ---
  rmt_copy_encoder_config_t enc_cfg = {};
  ESP_RETURN_ON_ERROR(rmt_new_copy_encoder(&enc_cfg, &s_copy_enc), "WS2812", "copy enc failed");

  ESP_RETURN_ON_ERROR(rmt_enable(s_tx0), "WS2812", "enable tx0 failed");
  ESP_RETURN_ON_ERROR(rmt_enable(s_tx1), "WS2812", "enable tx1 failed");

  // --- Allocate symbol buffers: 24 symbols/LED + 1 reset ---
  s_items_per_line = (size_t)leds_per_line * 24u + 1u;
  s_items0 = (sym_t*)heap_caps_malloc(s_items_per_line * sizeof(sym_t), MALLOC_CAP_INTERNAL);
  s_items1 = (sym_t*)heap_caps_malloc(s_items_per_line * sizeof(sym_t), MALLOC_CAP_INTERNAL);
  if (!s_items0 || !s_items1) return false;

  s_tx_cfg.loop_count = 0;
  s_tx_cfg.flags.eot_level = 0;
  s_tx_cfg.flags.queue_nonblocking = 0;

  s_next_deadline = esp_timer_get_time();
  return true;
}

static inline void encode_line(const CRGB* src, sym_t* out) {
  size_t idx = 0;
  for (int i = 0; i < s_leds; ++i) encode_led(src[i], out, idx);
  append_reset(out, idx);
}

uint32_t ws2812_ng_frame_time_us(void) {
  // 30 us/LED @ 800kHz plus ~80 us reset (we use 60 us)
  return (uint32_t)(s_leds * 30 + 80);
}

bool ws2812_ng_show_dual(const CRGB* line0, const CRGB* line1) {
  if (!s_tx0 || !s_tx1) return false;
  encode_line(line0, s_items0);
  encode_line(line1, s_items1);

  ESP_RETURN_ON_ERROR(rmt_transmit(s_tx0, s_copy_enc, s_items0,
                                   s_items_per_line * sizeof(sym_t), &s_tx_cfg),
                      "WS2812", "tx0 transmit failed");
  ESP_RETURN_ON_ERROR(rmt_transmit(s_tx1, s_copy_enc, s_items1,
                                   s_items_per_line * sizeof(sym_t), &s_tx_cfg),
                      "WS2812", "tx1 transmit failed");
  ESP_RETURN_ON_ERROR(rmt_tx_wait_all_done(s_tx0, -1), "WS2812", "tx0 wait failed");
  ESP_RETURN_ON_ERROR(rmt_tx_wait_all_done(s_tx1, -1), "WS2812", "tx1 wait failed");
  return true;
}

bool ws2812_ng_show_dual_paced(const CRGB* line0, const CRGB* line1, uint32_t target_us) {
  if (!s_tx0 || !s_tx1) return false;

  const uint64_t now = esp_timer_get_time();
  // If early, yield until deadline; if very late, re-anchor.
  if ((int64_t)(s_next_deadline - now) > 500) {
    while ((int64_t)(s_next_deadline - esp_timer_get_time()) > 0) taskYIELD();
  } else if ((int64_t)(now - s_next_deadline) > (int64_t)target_us) {
    s_next_deadline = now;
  }

  encode_line(line0, s_items0);
  encode_line(line1, s_items1);

  ESP_RETURN_ON_ERROR(rmt_transmit(s_tx0, s_copy_enc, s_items0,
                                   s_items_per_line * sizeof(sym_t), &s_tx_cfg),
                      "WS2812", "tx0 transmit failed");
  ESP_RETURN_ON_ERROR(rmt_transmit(s_tx1, s_copy_enc, s_items1,
                                   s_items_per_line * sizeof(sym_t), &s_tx_cfg),
                      "WS2812", "tx1 transmit failed");
  ESP_RETURN_ON_ERROR(rmt_tx_wait_all_done(s_tx0, -1), "WS2812", "tx0 wait failed");
  ESP_RETURN_ON_ERROR(rmt_tx_wait_all_done(s_tx1, -1), "WS2812", "tx1 wait failed");

  s_next_deadline += target_us;
  return true;
}

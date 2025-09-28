#include "led_driver.h"

#include <algorithm>

#include "ws2812_dual_rmt.h"

namespace vp {

void LedFrame::clear() {
  if (strip1) {
    fill_solid(strip1, length, CRGB::Black);
  }
  if (strip2) {
    fill_solid(strip2, length, CRGB::Black);
  }
}

CRGB& LedFrame::pixel(uint8_t strip, uint16_t index) {
  index = std::min<uint16_t>(index, length ? (length - 1) : 0);
  if (strip == 0 || strip2 == nullptr) {
    return strip1[index];
  }
  return strip2[index];
}

void LedDriver::init(uint8_t brightness) {
  if (!ready_) {
    FastLED.addLeds<WS2812B, kPrimaryDataPin, GRB>(leds_, 0, kStrip1Leds);
    FastLED.addLeds<WS2812B, kSecondaryDataPin, GRB>(leds_ + kStrip1Leds, kStrip2Leds);
    FastLED.setDither(false);
    FastLED.setMaxRefreshRate(0, true);
    ws2812_ng_init(static_cast<gpio_num_t>(kPrimaryDataPin),
                   static_cast<gpio_num_t>(kSecondaryDataPin),
                   kStrip1Leds);
    ready_ = true;
  }
  brightness_ = brightness;
  fill_solid(leds_, kLedCount, CRGB::Black);
}

LedFrame LedDriver::begin_frame() {
  if (!ready_) {
    return {};
  }
  fill_solid(leds_, kLedCount, CRGB::Black);
  LedFrame frame;
  frame.strip1 = leds_;
  frame.strip2 = leds_ + kStrip1Leds;
  frame.length = kStrip1Leds;
  return frame;
}

void LedDriver::show() {
  if (!ready_) {
    return;
  }
  const CRGB* out0 = leds_;
  const CRGB* out1 = leds_ + kStrip1Leds;

  if (brightness_ < 255) {
    for (uint16_t i = 0; i < kLedCount; ++i) {
      scaled_[i] = leds_[i];
      scaled_[i].nscale8_video(brightness_);
    }
    out0 = scaled_;
    out1 = scaled_ + kStrip1Leds;
  }

  uint32_t target_us = ws2812_ng_frame_time_us();
  ws2812_ng_show_dual_paced(out0, out1, target_us);
}

void LedDriver::set_brightness(uint8_t value) {
  brightness_ = value;
  if (ready_) {
    FastLED.setBrightness(brightness_);
  }
}

}  // namespace vp

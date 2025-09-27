#include "led_driver.h"

#include <algorithm>

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
    ready_ = true;
  }
  brightness_ = brightness;
  FastLED.setBrightness(brightness_);
  fill_solid(leds_, kLedCount, CRGB::Black);
  FastLED.show();
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
  FastLED.setBrightness(brightness_);
  FastLED.show();
}

void LedDriver::set_brightness(uint8_t value) {
  brightness_ = value;
  if (ready_) {
    FastLED.setBrightness(brightness_);
  }
}

}  // namespace vp


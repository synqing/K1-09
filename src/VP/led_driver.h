#pragma once

#include <stdint.h>

#include "FastLED.h"

namespace vp {

struct LedFrame {
  CRGB* strip1 = nullptr;
  CRGB* strip2 = nullptr;
  uint16_t length = 0;

  void clear();
  CRGB& pixel(uint8_t strip, uint16_t index);
};

class LedDriver {
 public:
  void init(uint8_t brightness);

  // Begin a new frame (clears buffers) and returns strip views.
  LedFrame begin_frame();

  // Apply global brightness curve and present.
  void show();

  // Accessors
  uint16_t strip_length() const { return kStrip1Leds; }
  uint8_t brightness() const { return brightness_; }
  void set_brightness(uint8_t value);

  CRGB* raw_data() { return leds_; }
  const CRGB* raw_data() const { return leds_; }

 private:
  static constexpr uint8_t kPrimaryDataPin = 9;
  static constexpr uint8_t kSecondaryDataPin = 10;
  static constexpr uint16_t kStrip1Leds = 160;
  static constexpr uint16_t kStrip2Leds = 160;
  static constexpr uint16_t kLedCount = kStrip1Leds + kStrip2Leds;

  CRGB leds_[kLedCount];
  uint8_t brightness_ = 140;
  bool ready_ = false;
};

}  // namespace vp

#pragma once

#include "vp_consumer.h"

namespace vp_renderer {

void init();
void render(const vp_consumer::VPFrame& f);

// HMI controls
void adjust_brightness(int delta);
void adjust_speed(float factor);
void next_mode();
void prev_mode();

struct Status {
  uint8_t brightness;
  float   speed;
  uint8_t mode;
};
Status status();

} // namespace vp_renderer

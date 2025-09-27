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
void next_palette();
void prev_palette();
void adjust_sensitivity(float factor);

struct Status {
  uint8_t brightness;
  float   speed;
  uint8_t mode;
  uint8_t palette;
  const char* palette_name;
  float sensitivity;
};
Status status();

} // namespace vp_renderer

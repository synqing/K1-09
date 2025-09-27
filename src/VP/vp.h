#pragma once

#include <stdint.h>

// Public VP API. VP consumes AudioFrame and renders visuals.
// Keep hot-path lean; no dynamic allocation in tick.

namespace vp {

void init();
void tick();

// HMI proxies
void brightness_up();
void brightness_down();
void speed_up();
void speed_down();
void next_mode();
void prev_mode();
void next_palette();
void prev_palette();
void sensitivity_up();
void sensitivity_down();

// For status prints
struct HMIStatus {
  unsigned brightness;
  float speed;
  unsigned mode;
  unsigned palette;
  const char* palette_name;
  float sensitivity;
};
HMIStatus hmi_status();

} // namespace vp

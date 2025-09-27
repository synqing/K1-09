#pragma once

namespace vp {

struct Tunables {
  float brightness = 0.55f;   // 0..1
  float speed = 1.0f;         // multiplier for motion
  float saturation = 1.0f;    // palette saturation scalar
  float sensitivity = 1.0f;   // waveform responsiveness (1.0 == legacy default)
  float flux_boost = 0.0f;    // dynamic lift (0..1)
  float beat_boost = 0.0f;    // envelope from beat strength
};

}  // namespace vp

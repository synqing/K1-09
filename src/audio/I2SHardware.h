#pragma once

#include <driver/i2s.h>

namespace K1Lightwave {
namespace Audio {

struct I2SHardwareConfig {
  i2s_port_t port = I2S_NUM_0;
  int bclkPin = 7;
  int lrclkPin = 9;
  int dataInPin = 8;
};

}  // namespace Audio
}  // namespace K1Lightwave

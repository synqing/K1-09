#pragma once
#include <stdint.h>

namespace K1Lightwave {
namespace Audio {
namespace Sph0645 {

// Constants (your hard requirements)
static constexpr int      kSampleRate   = 16000;
static constexpr uint32_t kChunkSize    = 128;   // 128 I2S words per read
static constexpr int      kPinBCLK      = 7;     // SCK / BCLK
static constexpr int      kPinLRCK      = 13;    // WS / LRCK
static constexpr int      kPinDIN       = 8;     // SD / DOUT
static constexpr int      kI2SPort      = 0;     // I2S_NUM_0

// Lifecycle
bool setup();            // init I2S with 16kHz, 128-chunk
void loop();             // read chunk, track & print min/max once per second

// Optional controls
void setUseAPLL(bool on);     // re-init with/without APLL
void resetMinMax();           // clear min/max accumulator
void getMinMax(int32_t& mn, int32_t& mx);

// Optional access (if you want the raw 32-bit I2S chunk)
const int32_t* lastChunk();   // pointer valid until next loop()
uint32_t       lastCount();   // number of samples filled (should be 128)

} // namespace Sph0645
} // namespace Audio
} // namespace K1Lightwave

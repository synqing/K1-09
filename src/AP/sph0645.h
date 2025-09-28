#pragma once
#include <stdint.h>
#include <stddef.h>

namespace K1Lightwave {
namespace Audio {
namespace Sph0645 {

// Layer-1 bring-up (I2S init + internal telemetry printer @ 1 Hz).
void setup();

// Legacy loop (kept for dev; you can stop calling this in production).
void loop();

// DC offset accessors (raw signed 24-bit, carried in int32 after >>8 unpack).
int32_t read_dc_offset_q24();     // fast moving-average (windowed) estimate
int32_t read_dc_offset_1s_q24();  // exact 1-second mean

/**
 * @brief Blocking read of exactly N 32-bit I2S words, unpacked in-place to Q24 via arithmetic >>8.
 *        Returns true only if the full chunk was read successfully.
 *        Side-effects: updates the same internal telemetry as loop() (min/max, DC(1s), AC stats, Qstep, rail).
 *
 * @param out_q24  Destination buffer of length N (int32). On return contains Q24 signed samples.
 * @param n        Number of samples to read (unsigned int to match toolchain symbol).
 */
bool read_q24_chunk(int32_t* out_q24, unsigned int n);

float read_sample_rate_hz();

} // namespace Sph0645
} // namespace Audio
} // namespace K1Lightwave

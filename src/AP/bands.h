#pragma once

#include <array>
#include <cstdint>
#include <cmath>

namespace K1Lightwave {
namespace Audio {

// Equal-tempered musical bank: bin 0 corresponds to 110 Hz (A2).
inline float bin_hz_64(std::uint8_t idx) {
  return 110.0f * std::pow(2.0f, static_cast<float>(idx) / 12.0f);
}

struct Bands4_64 {
  static constexpr std::uint8_t kNumBands = 4;
  static constexpr std::array<std::uint8_t, kNumBands> kBandStart{0u, 11u, 33u, 54u};
  static constexpr std::array<std::uint8_t, kNumBands> kBandEnd  {11u, 33u, 54u, 64u}; // exclusive
  static constexpr std::array<const char*, kNumBands>   kBandNames{"Low", "Low-Mid", "Presence", "High"};

  static constexpr std::uint8_t band_of_bin(std::uint8_t bin) {
    for (std::uint8_t band = 0; band < kNumBands; ++band) {
      if (bin < kBandEnd[band]) {
        return band;
      }
    }
    return kNumBands - 1;
  }
};

} // namespace Audio
} // namespace K1Lightwave


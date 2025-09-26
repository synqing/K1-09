#pragma once

// Minimal Arduino compatibility layer for host-side unit tests.
// Provides stand-ins for types and functions that fixed-point and
// firmware headers expect without pulling in the full Arduino SDK.

#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <algorithm>

using byte = uint8_t;

#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

#ifndef PROGMEM
#define PROGMEM
#endif

#ifndef PI
#define PI 3.14159265358979323846
#endif

inline void delay(unsigned long) {}
inline void yield() {}

inline long random(long max) {
    return static_cast<long>(std::rand() % (max > 0 ? max : 1));
}

inline long random(long min, long max) {
    const long range = (max > min) ? (max - min) : 1;
    return min + static_cast<long>(std::rand() % range);
}

inline void randomSeed(unsigned long seed) {
    std::srand(static_cast<unsigned int>(seed));
}

#ifndef min
template <typename T>
constexpr const T &min(const T &a, const T &b) {
    return (b < a) ? b : a;
}
#endif

#ifndef max
template <typename T>
constexpr const T &max(const T &a, const T &b) {
    return (a < b) ? b : a;
}
#endif

#ifndef constrain
template <typename T>
constexpr const T &constrain(const T &x, const T &a, const T &b) {
    return (x < a) ? a : ((x > b) ? b : x);
}
#endif

inline unsigned long micros() { return 0; }
inline unsigned long millis() { return 0; }

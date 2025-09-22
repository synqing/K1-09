// Palette metadata - separated to avoid multiple definition issues
#pragma once

#include <stdint.h>

// Crameri palette count
constexpr uint8_t CRAMERI_PALETTE_COUNT = 24;

// Crameri palette names
inline const char* const* get_crameri_palette_names() {
    static const char* const names[] = {
        "Vik", "Tokyo", "Roma", "Oleron", "Lisbon", "LaJolla",
        "Hawaii", "Devon", "Cork", "Broc", "Berlin", "Bamako",
        "Acton", "Batlow", "Bilbao", "Buda", "Davos", "GrayC",
        "Imola", "LaPaz", "Nuuk", "Oslo", "Tofino", "Turku"
    };
    return names;
}
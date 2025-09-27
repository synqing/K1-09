#pragma once
#include <stdint.h>
#include <Arduino.h>
#include <Preferences.h>

namespace storage {
namespace nvs {

bool init(const char* ns = "k1_audio");

bool read_u32(const char* key, uint32_t& out_value);

bool write_u32(const char* key, uint32_t value);

void write_u32_debounced(const char* key,
                         uint32_t value,
                         uint32_t minimum_interval_ms = 5000,
                         bool force = false);

void poll();

} // namespace nvs
} // namespace storage


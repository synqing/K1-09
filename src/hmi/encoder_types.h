#pragma once

#include <Arduino.h>

namespace HMI {

enum class ClickKind : uint8_t {
    None = 0,
    Single,
    Double,
    ChordSingle,
    ChordDouble
};

struct EncoderEvent {
    uint8_t encoder_id = 0xFF;   // 0-1 for individual encoders, 0xFE for manager/system, 0xFF invalid
    int32_t rotation = 0;        // Relative rotation since last event
    ClickKind click = ClickKind::None;
    uint32_t timestamp_ms = 0;   // Event generation time
};

}  // namespace HMI


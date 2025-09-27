#pragma once

#include <array>

#include "hmi/encoder_channel.h"

namespace HMI {

class EncoderManager {
public:
    EncoderManager();

    void setHardwareConfig(uint8_t encoder_id, const EncoderHardwareConfig& config);
    bool begin(bool verbose = false);
    void update(uint32_t now_ms);
    bool popEvent(EncoderEvent& event);

    bool available(uint8_t encoder_id) const;
    void setIdleColor(uint8_t encoder_id, uint32_t rgb);
    void setActiveColor(uint8_t encoder_id, uint32_t rgb);
    void applyIdleColors();

private:
    static constexpr uint8_t kEncoderCount = 2;
    static constexpr uint32_t kChordWindowMs = 120;
    static constexpr size_t kQueueSize = 12;

    std::array<EncoderChannel, kEncoderCount> channels_;
    std::array<EncoderHardwareConfig, kEncoderCount> configs_;
    std::array<bool, kEncoderCount> config_set_{};

    std::array<ClickKind, kEncoderCount> recent_click_kind_{};
    std::array<uint32_t, kEncoderCount> recent_click_time_{};

    std::array<EncoderEvent, kQueueSize> event_queue_{};
    size_t queue_head_ = 0;
    size_t queue_tail_ = 0;

    void pushEvent(const EncoderEvent& event);
    void handleChannelEvent(const EncoderEvent& event);
};

}  // namespace HMI


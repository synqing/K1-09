#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <array>

#include "M5UnitScroll.h"

#include "hmi/encoder_types.h"

namespace HMI {

struct EncoderHardwareConfig {
    TwoWire* wire = &Wire;
    uint8_t address = 0x40;
    uint8_t sda_pin = 21;
    uint8_t scl_pin = 22;
    uint32_t bus_speed_hz = 400000U;
};

class EncoderChannel {
public:
    explicit EncoderChannel(uint8_t id = 0);

    void configure(const EncoderHardwareConfig& config);
    bool begin(bool verbose = false);
    void update(uint32_t now_ms);
    bool popEvent(EncoderEvent& event);

    bool available() const { return available_; }
    uint8_t id() const { return encoder_id_; }

    void setIdleColor(uint32_t rgb);
    void setActiveColor(uint32_t rgb);
    void applyIdleColor();

    void requestRecovery(uint32_t when_ms) { next_recovery_ms_ = when_ms; }
    bool shouldAttemptRecovery(uint32_t now_ms) const {
        return !available_ && now_ms >= next_recovery_ms_;
    }

private:
    static constexpr int32_t kMaxStepPerSample = 40;
    static constexpr uint32_t kDoubleClickWindowMs = 350;

    bool ensureBusInitialized();
    void flushPendingClick(uint32_t now_ms);
    void pushEvent(const EncoderEvent& event);

    EncoderHardwareConfig config_{};
    M5UnitScroll device_{};
    uint8_t encoder_id_ = 0;
    bool configured_ = false;
    bool bus_initialized_ = false;
    bool available_ = false;

    int16_t last_encoder_value_ = 0;
    bool last_button_state_ = false;
    uint32_t button_press_ms_ = 0;

    bool pending_single_click_ = false;
    uint32_t pending_single_release_ms_ = 0;

    uint32_t next_recovery_ms_ = 0;
    uint32_t last_comm_success_ms_ = 0;

    uint32_t idle_color_rgb_ = 0x000000;
    uint32_t active_color_rgb_ = 0x000000;

    static constexpr size_t kQueueSize = 6;
    std::array<EncoderEvent, kQueueSize> queue_{};
    size_t queue_head_ = 0;
    size_t queue_tail_ = 0;

    void onRotationDelta(int32_t delta, uint32_t now_ms);
    void onButtonEdge(bool pressed, uint32_t now_ms);
};

}  // namespace HMI


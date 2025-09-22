#include "hmi/encoder_channel.h"

#include <algorithm>

namespace HMI {

namespace {
inline uint32_t clampColor(uint32_t rgb) {
    return rgb & 0x00FFFFFF;  // Ensure 24-bit
}
}

EncoderChannel::EncoderChannel(uint8_t id) : encoder_id_(id) {}

void EncoderChannel::configure(const EncoderHardwareConfig& config) {
    config_ = config;
    configured_ = true;
}

bool EncoderChannel::ensureBusInitialized() {
    if (bus_initialized_) {
        return true;
    }
    if (!configured_ || config_.wire == nullptr) {
        return false;
    }
    config_.wire->begin(config_.sda_pin, config_.scl_pin, config_.bus_speed_hz);
    bus_initialized_ = true;
    return true;
}

bool EncoderChannel::begin(bool verbose) {
    if (!configured_) {
        return false;
    }
    if (!ensureBusInitialized()) {
        return false;
    }

    bool ok = device_.begin(config_.wire, config_.address, config_.sda_pin, config_.scl_pin, config_.bus_speed_hz);
    if (!ok) {
        available_ = false;
        next_recovery_ms_ = millis() + 2000;
        return false;
    }

    available_ = true;
    last_encoder_value_ = device_.getEncoderValue();
    last_button_state_ = device_.getButtonStatus();
    pending_single_click_ = false;
    queue_head_ = queue_tail_ = 0;
    device_.setEncoderValue(0);
    last_encoder_value_ = 0;
    if (idle_color_rgb_ != 0) {
        device_.setLEDColor(idle_color_rgb_);
    }
    last_comm_success_ms_ = millis();
    return true;
}

void EncoderChannel::update(uint32_t now_ms) {
    if (!configured_) {
        return;
    }

    if (!available_) {
        flushPendingClick(now_ms);
        return;
    }

    int16_t current_value = device_.getEncoderValue();
    int32_t delta = static_cast<int32_t>(current_value) - static_cast<int32_t>(last_encoder_value_);
    if (delta > kMaxStepPerSample || delta < -kMaxStepPerSample) {
        delta = 0;
    }
    last_encoder_value_ = current_value;

    if (delta != 0) {
        onRotationDelta(delta, now_ms);
    }

    bool button_state = device_.getButtonStatus();
    if (button_state != last_button_state_) {
        onButtonEdge(button_state, now_ms);
        last_button_state_ = button_state;
    }

    flushPendingClick(now_ms);
    last_comm_success_ms_ = now_ms;
}

void EncoderChannel::flushPendingClick(uint32_t now_ms) {
    if (!pending_single_click_) {
        return;
    }
    if ((now_ms - pending_single_release_ms_) > kDoubleClickWindowMs) {
        EncoderEvent event;
        event.encoder_id = encoder_id_;
        event.rotation = 0;
        event.click = ClickKind::Single;
        event.timestamp_ms = pending_single_release_ms_;
        pushEvent(event);
        pending_single_click_ = false;
    }
}

void EncoderChannel::onRotationDelta(int32_t delta, uint32_t now_ms) {
    EncoderEvent event;
    event.encoder_id = encoder_id_;
    event.rotation = delta;
    event.click = ClickKind::None;
    event.timestamp_ms = now_ms;
    pushEvent(event);
}

void EncoderChannel::onButtonEdge(bool pressed, uint32_t now_ms) {
    if (pressed) {
        button_press_ms_ = now_ms;
        return;
    }

    if (pending_single_click_) {
        if ((now_ms - pending_single_release_ms_) <= kDoubleClickWindowMs) {
            EncoderEvent event;
            event.encoder_id = encoder_id_;
            event.rotation = 0;
            event.click = ClickKind::Double;
            event.timestamp_ms = now_ms;
            pushEvent(event);
            pending_single_click_ = false;
            return;
        } else {
            EncoderEvent event;
            event.encoder_id = encoder_id_;
            event.rotation = 0;
            event.click = ClickKind::Single;
            event.timestamp_ms = pending_single_release_ms_;
            pushEvent(event);
            pending_single_click_ = false;
        }
    }

    pending_single_click_ = true;
    pending_single_release_ms_ = now_ms;
}

bool EncoderChannel::popEvent(EncoderEvent& event) {
    if (queue_head_ == queue_tail_) {
        return false;
    }
    event = queue_[queue_head_];
    queue_head_ = (queue_head_ + 1) % kQueueSize;
    return true;
}

void EncoderChannel::pushEvent(const EncoderEvent& event) {
    if (((queue_tail_ + 1) % kQueueSize) == queue_head_) {
        queue_head_ = (queue_head_ + 1) % kQueueSize;
    }
    queue_[queue_tail_] = event;
    queue_tail_ = (queue_tail_ + 1) % kQueueSize;
}

void EncoderChannel::setIdleColor(uint32_t rgb) {
    idle_color_rgb_ = clampColor(rgb);
    if (available_) {
        device_.setLEDColor(idle_color_rgb_);
    }
}

void EncoderChannel::setActiveColor(uint32_t rgb) {
    active_color_rgb_ = clampColor(rgb);
}

void EncoderChannel::applyIdleColor() {
    if (available_ && idle_color_rgb_ != 0) {
        device_.setLEDColor(idle_color_rgb_);
    }
}

}  // namespace HMI

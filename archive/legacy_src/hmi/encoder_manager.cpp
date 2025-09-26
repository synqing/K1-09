#include "hmi/encoder_manager.h"

#include <algorithm>

namespace {
inline bool queueIsFull(size_t head, size_t tail, size_t capacity) {
    return ((tail + 1) % capacity) == head;
}
}

namespace HMI {

EncoderManager::EncoderManager() {
    for (uint8_t i = 0; i < kEncoderCount; ++i) {
        channels_[i] = EncoderChannel(i);
        recent_click_kind_[i] = ClickKind::None;
        recent_click_time_[i] = 0;
        config_set_[i] = false;
    }
}

void EncoderManager::setHardwareConfig(uint8_t encoder_id, const EncoderHardwareConfig& config) {
    if (encoder_id >= kEncoderCount) {
        return;
    }
    configs_[encoder_id] = config;
    config_set_[encoder_id] = true;
    channels_[encoder_id].configure(config);
}

bool EncoderManager::begin(bool verbose) {
    bool any_ok = false;
    uint32_t now = millis();
    for (uint8_t i = 0; i < kEncoderCount; ++i) {
        if (!config_set_[i]) {
            continue;
        }
        if (channels_[i].begin(verbose)) {
            any_ok = true;
        } else {
            channels_[i].requestRecovery(now + 2000);
        }
    }
    return any_ok;
}

void EncoderManager::update(uint32_t now_ms) {
    for (uint8_t i = 0; i < kEncoderCount; ++i) {
        if (!config_set_[i]) {
            continue;
        }

        if (!channels_[i].available() && channels_[i].shouldAttemptRecovery(now_ms)) {
            channels_[i].begin(false);
        }

        if (!channels_[i].available()) {
            continue;
        }

        channels_[i].update(now_ms);
        EncoderEvent event;
        while (channels_[i].popEvent(event)) {
            handleChannelEvent(event);
        }
    }

    // Expire stale click markers to avoid ghost chord detection
    for (uint8_t i = 0; i < kEncoderCount; ++i) {
        if (recent_click_kind_[i] != ClickKind::None &&
            (now_ms - recent_click_time_[i]) > kChordWindowMs) {
            recent_click_kind_[i] = ClickKind::None;
        }
    }
}

bool EncoderManager::popEvent(EncoderEvent& event) {
    if (queue_head_ == queue_tail_) {
        return false;
    }
    event = event_queue_[queue_head_];
    queue_head_ = (queue_head_ + 1) % kQueueSize;
    return true;
}

bool EncoderManager::available(uint8_t encoder_id) const {
    if (encoder_id >= kEncoderCount) {
        return false;
    }
    return channels_[encoder_id].available();
}

void EncoderManager::setIdleColor(uint8_t encoder_id, uint32_t rgb) {
    if (encoder_id >= kEncoderCount) {
        return;
    }
    channels_[encoder_id].setIdleColor(rgb);
}

void EncoderManager::setActiveColor(uint8_t encoder_id, uint32_t rgb) {
    if (encoder_id >= kEncoderCount) {
        return;
    }
    channels_[encoder_id].setActiveColor(rgb);
}

void EncoderManager::applyIdleColors() {
    for (uint8_t i = 0; i < kEncoderCount; ++i) {
        channels_[i].applyIdleColor();
    }
}

void EncoderManager::pushEvent(const EncoderEvent& event) {
    if (queueIsFull(queue_head_, queue_tail_, kQueueSize)) {
        queue_head_ = (queue_head_ + 1) % kQueueSize;
    }
    event_queue_[queue_tail_] = event;
    queue_tail_ = (queue_tail_ + 1) % kQueueSize;
}

void EncoderManager::handleChannelEvent(const EncoderEvent& event) {
    pushEvent(event);

    if (event.click == ClickKind::None || event.encoder_id >= kEncoderCount) {
        return;
    }

    const uint8_t current = event.encoder_id;
    const uint8_t other = (current + 1) % kEncoderCount;

    if (recent_click_kind_[other] != ClickKind::None) {
        if ((event.timestamp_ms - recent_click_time_[other]) <= kChordWindowMs) {
            EncoderEvent chord;
            chord.encoder_id = 0xFE;
            chord.rotation = 0;
            chord.timestamp_ms = std::max(event.timestamp_ms, recent_click_time_[other]);
            chord.click = (event.click == ClickKind::Double && recent_click_kind_[other] == ClickKind::Double)
                              ? ClickKind::ChordDouble
                              : ClickKind::ChordSingle;
            pushEvent(chord);
            recent_click_kind_[other] = ClickKind::None;
            return;
        }
    }

    recent_click_kind_[current] = event.click;
    recent_click_time_[current] = event.timestamp_ms;
}

}  // namespace HMI

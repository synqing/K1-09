#include "hmi/dual_encoder_controller.h"

#include <algorithm>

#include "constants.h"
#include "globals.h"
#include "usb_cdc_config.h"
#include "palettes/palette_luts_api.h"

#ifdef LED_DATA_PIN
#undef LED_DATA_PIN
#endif
#ifdef LED_CLOCK_PIN
#undef LED_CLOCK_PIN
#endif
#ifdef SECONDARY_LED_DATA_PIN
#undef SECONDARY_LED_DATA_PIN
#endif

#include "lc/core/FxEngine.h"
#include "lc/core/RuntimeTunables.h"
#include "debug/debug_manager.h"

#ifndef USBSerial
#define USBSerial Serial
#endif

extern void save_config_delayed();



extern FxEngine fxEngine;
extern uint8_t rt_transition_type;

namespace {

constexpr float kBrightnessStep = 1.0f / 40.0f;

inline float clampFloat(float value, float min_val, float max_val) {
    if (value < min_val) return min_val;
    if (value > max_val) return max_val;
    return value;
}

inline uint8_t wrapEffectIndex(int value) {
    const int total = static_cast<int>(fxEngine.getNumEffects());
    if (total <= 0) {
        return 0;
    }
    while (value < 0) value += total;
    while (value >= total) value -= total;
    return static_cast<uint8_t>(value);
}

}  // namespace

namespace HMI {

DualEncoderController::DualEncoderController() {
    EncoderHardwareConfig primary_config;
    primary_config.wire = &Wire;
    primary_config.address = 0x40;
    primary_config.sda_pin = 3;
    primary_config.scl_pin = 4;
    primary_config.bus_speed_hz = 400000U;
    manager_.setHardwareConfig(kPrimaryEncoder, primary_config);

    channel_state_[0] = ChannelState{};
}

bool DualEncoderController::begin(bool verbose) {
    bool ok = manager_.begin(verbose);

    channel_state_[0].palette_mode = (CONFIG.PALETTE_INDEX != 0);

    DebugManager::preset_minimal();

    if (verbose && USBSerial) {
        USBSerial.printf("[HMI] Scroll (SDA=3,SCL=4) -> Channel1 %s\n",
                         ok && manager_.available(kPrimaryEncoder) ? "ONLINE" : "OFFLINE");
    }

    return ok;
}

void DualEncoderController::update(uint32_t now_ms) {
    manager_.update(now_ms);

    EncoderEvent event;
    while (manager_.popEvent(event)) {
        if (event.encoder_id == kPrimaryEncoder) {
            handleChannelEvent(event.encoder_id, event);
        }
    }
}

void DualEncoderController::handleChannelEvent(uint8_t channel, const EncoderEvent& event) {
    if (channel >= (sizeof(channel_state_) / sizeof(channel_state_[0]))) {
        return;
    }
    int32_t ticks = -event.rotation;  // Align physical orientation

    if (ticks != 0) {
        if (channel_state_[channel].mode == ChannelControlMode::Brightness) {
            adjustBrightness(channel, ticks, event.timestamp_ms);
        } else {
            adjustLightshow(channel, ticks, event.timestamp_ms);
        }
        markInteraction(event.timestamp_ms, channel);
    }

    if (event.click == ClickKind::Single) {
        toggleMode(channel);
        markInteraction(event.timestamp_ms, channel);
    } else if (event.click == ClickKind::Double) {
        togglePalette(channel);
        markInteraction(event.timestamp_ms, channel);
    }
}

void DualEncoderController::adjustBrightness(uint8_t channel, int32_t ticks, uint32_t timestamp_ms) {
    if (channel >= (sizeof(channel_state_) / sizeof(channel_state_[0]))) {
        return;
    }
    float delta = kBrightnessStep * static_cast<float>(ticks);

    float new_value = clampFloat(CONFIG.PHOTONS + delta, 0.0f, 1.0f);
    if (new_value != CONFIG.PHOTONS) {
        CONFIG.PHOTONS = new_value;
        ::knob_photons.last_change = timestamp_ms;
        ::settings_updated = true;
        save_config_delayed();
        if (debug_mode && USBSerial) {
            USBSerial.printf("[HMI] Channel 1 brightness -> %.3f\n", CONFIG.PHOTONS);
        }
    }
}

void DualEncoderController::adjustLightshow(uint8_t channel, int32_t ticks, uint32_t /*timestamp_ms*/) {
    if (channel >= (sizeof(channel_state_) / sizeof(channel_state_[0]))) {
        return;
    }
    if (ticks == 0) {
        return;
    }

    const uint8_t total = fxEngine.getNumEffects();
    if (total == 0) {
        return;
    }

    const uint8_t new_mode = wrapEffectIndex(static_cast<int>(CONFIG.LIGHTSHOW_MODE) + ticks);
    if (new_mode != CONFIG.LIGHTSHOW_MODE) {
        CONFIG.LIGHTSHOW_MODE = new_mode;
        fxEngine.setEffect(new_mode, rt_transition_type, 800);
        ::settings_updated = true;
        save_config_delayed();
        if (debug_mode) {
            USBSerial.printf("[HMI] Channel 1 lightshow -> %u (%s)\n",
                             CONFIG.LIGHTSHOW_MODE,
                             fxEngine.getEffectName(new_mode));
        }
    }
}

void DualEncoderController::toggleMode(uint8_t channel) {
    if (channel >= (sizeof(channel_state_) / sizeof(channel_state_[0]))) {
        return;
    }
    auto& state = channel_state_[channel];
    state.mode = (state.mode == ChannelControlMode::Brightness)
                     ? ChannelControlMode::Lightshow
                     : ChannelControlMode::Brightness;

    if (debug_mode) {
        USBSerial.printf("[HMI] Channel %u mode -> %s\n",
                         static_cast<unsigned>(channel + 1),
                         state.mode == ChannelControlMode::Brightness ? "Brightness" : "Lightshow");
    }
}

void DualEncoderController::togglePalette(uint8_t channel) {
    if (channel >= (sizeof(channel_state_) / sizeof(channel_state_[0]))) {
        return;
    }
    auto& state = channel_state_[channel];
    state.palette_mode = !state.palette_mode;

    uint8_t palette_total = palette_lut_count();
    if (state.palette_mode) {
        if (palette_total > 1) {
            CONFIG.PALETTE_INDEX = std::min<uint8_t>(palette_total - 1, static_cast<uint8_t>(1));
        } else {
            CONFIG.PALETTE_INDEX = 0;
            state.palette_mode = false;
        }
    } else {
        CONFIG.PALETTE_INDEX = 0;
    }

    // Keep both channel states aligned with the actual palette mode
    bool palette_active = (CONFIG.PALETTE_INDEX != 0);
    channel_state_[0].palette_mode = palette_active;

    ::settings_updated = true;
    save_config_delayed();

    if (debug_mode) {
        USBSerial.printf("[HMI] Palette mode -> %s\n",
                         palette_active ? "Palette" : "HSV");
    }
}

void DualEncoderController::markInteraction(uint32_t timestamp_ms, uint8_t channel) {
    ::g_last_encoder_activity_time = timestamp_ms;
    ::g_last_active_encoder = channel;
}

int DualEncoderController::wrapMode(int value) const {
    const int total = static_cast<int>(fxEngine.getNumEffects());
    if (total <= 0) {
        return 0;
    }
    while (value < 0) value += total;
    while (value >= total) value -= total;
    return value;
}

}  // namespace HMI

#include "hmi/dual_encoder_controller.h"

#include <algorithm>

#include "constants.h"
#include "globals.h"
#include "usb_cdc_config.h"
#include "palettes/palette_luts_api.h"
#include "debug/debug_manager.h"

#ifndef USBSerial
#define USBSerial Serial
#endif

extern void save_config_delayed();

#if 0

namespace {
constexpr uint32_t kDeckColors[] = {
    0x404040,  // Brightness
    0x0080FF,  // Color
    0xFF40B0,  // Dynamics
    0xFFA000,  // Shows
    0x00A070   // System
};

constexpr float kEncoderStep = 1.0f / 60.0f;  // Coarser control
constexpr float kPrismPrimaryStep = 0.5f;

inline uint32_t applySecondaryTint(uint32_t color) {
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    b = std::min<uint16_t>(255, b + 80);
    return (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
}

inline float clampFloat(float value, float min_val, float max_val) {
    if (value < min_val) return min_val;
    if (value > max_val) return max_val;
    return value;
}

inline uint8_t wrapMode(int value) {
    const int total = static_cast<int>(NUM_MODES);
    if (total <= 0) {
        return 0;
    }
    while (value < 0) value += total;
    while (value >= total) value -= total;
    return static_cast<uint8_t>(value);
}
}

namespace HMI {

DualEncoderController::DualEncoderController() {
    EncoderHardwareConfig nav_config;
    nav_config.wire = &Wire;
    nav_config.address = 0x40;
    nav_config.sda_pin = 1;
    nav_config.scl_pin = 2;
    nav_config.bus_speed_hz = 400000U;
    manager_.setHardwareConfig(kNavigationEncoder, nav_config);

    EncoderHardwareConfig adjust_config;
    adjust_config.wire = &Wire1;
    adjust_config.address = 0x40;
    adjust_config.sda_pin = 3;
    adjust_config.scl_pin = 4;
    adjust_config.bus_speed_hz = 400000U;
    manager_.setHardwareConfig(kAdjustmentEncoder, adjust_config);
}

bool DualEncoderController::begin(bool verbose) {
    bool ok = manager_.begin(verbose);
    updateLedCues();
    last_interaction_ms_ = millis();

    bool nav_ok = manager_.available(kNavigationEncoder);
    bool adj_ok = manager_.available(kAdjustmentEncoder);

    if (verbose && USBSerial) {
        USBSerial.printf("[HMI] Scroll0 (SDA=%u,SCL=%u): %s\n",
                         1, 2, nav_ok ? "ONLINE" : "OFFLINE");
        USBSerial.printf("[HMI] Scroll1 (SDA=%u,SCL=%u): %s\n",
                         3, 4, adj_ok ? "ONLINE" : "OFFLINE");
        USBSerial.println("[HMI] Audio/perf debug metrics suppressed (use debug_full to restore)");
    }

    DebugManager::preset_minimal();
    if (debug_mode) {
        logDeckState("init");
        logParameterFocus();
    }

    return ok;
}

void DualEncoderController::update(uint32_t now_ms) {
    manager_.update(now_ms);
    EncoderEvent event;
    while (manager_.popEvent(event)) {
        handleEvent(event);
    }

    if ((now_ms - last_interaction_ms_) > kUiTimeoutMs) {
        current_deck_ = Deck::Brightness;
        parameter_index_ = 0;
        secondary_focus_ = false;
        updateLedCues();
        last_interaction_ms_ = now_ms;
    }
}

void DualEncoderController::handleEvent(const EncoderEvent& event) {
    if (event.encoder_id == kNavigationEncoder) {
        handleNavigationEvent(event);
    } else if (event.encoder_id == kAdjustmentEncoder) {
        handleAdjustmentEvent(event);
    } else if (event.encoder_id == kChordEncoder) {
        handleChordEvent(event);
    }
}

void DualEncoderController::handleNavigationEvent(const EncoderEvent& event) {
    if (event.rotation != 0) {
        changeDeck(-event.rotation);
        markInteraction(event.timestamp_ms, event.encoder_id);
    }

    if (event.click == ClickKind::Single) {
        advanceParameter(+1);
        markInteraction(event.timestamp_ms, event.encoder_id);
    } else if (event.click == ClickKind::Double) {
        toggleSecondaryFocus();
        markInteraction(event.timestamp_ms, event.encoder_id);
    }
}

void DualEncoderController::handleAdjustmentEvent(const EncoderEvent& event) {
    if (event.rotation != 0 && current_deck_ != Deck::System) {
        adjustParameter(currentParameterId(), -event.rotation, event.timestamp_ms);
        markInteraction(event.timestamp_ms, event.encoder_id);
    }

    if (event.click == ClickKind::Single) {
        triggerPrimaryActionForDeck();
        markInteraction(event.timestamp_ms, event.encoder_id);
    } else if (event.click == ClickKind::Double && current_deck_ != Deck::System) {
        resetParameter(currentParameterId());
        markInteraction(event.timestamp_ms, event.encoder_id);
    }
}

void DualEncoderController::handleChordEvent(const EncoderEvent& event) {
    if (event.click == ClickKind::ChordDouble) {
        if (ENABLE_SECONDARY_LEDS) {
            SECONDARY_LIGHTSHOW_MODE = wrapMode(static_cast<int>(SECONDARY_LIGHTSHOW_MODE) + 1);
            settings_updated = true;
            save_config_delayed();
            markInteraction(event.timestamp_ms, kChordEncoder);
        }
    } else if (event.click == ClickKind::ChordSingle) {
        toggleSecondaryFocus();
        markInteraction(event.timestamp_ms, kChordEncoder);
    }
}

void DualEncoderController::changeDeck(int32_t direction) {
    int current = static_cast<int>(current_deck_);
    int count = static_cast<int>(Deck::Count);
    current += (direction > 0) ? 1 : -1;
    if (current < 0) current = count - 1;
    if (current >= count) current = 0;
    current_deck_ = static_cast<Deck>(current);
    parameter_index_ = 0;
    updateLedCues();
    if (debug_mode) {
        logDeckState("rotation");
        logParameterFocus();
    }
}

void DualEncoderController::advanceParameter(int8_t direction) {
    const uint8_t count = [this]() {
        switch (current_deck_) {
            case Deck::Brightness: return static_cast<uint8_t>(1);
            case Deck::Color: return static_cast<uint8_t>(3);
            case Deck::Dynamics: return static_cast<uint8_t>(3);
            case Deck::Shows: return static_cast<uint8_t>(1);
            case Deck::System: return static_cast<uint8_t>(1);
            case Deck::Count: return static_cast<uint8_t>(1);
            default: return static_cast<uint8_t>(1);
        }
    }();

    if (count == 0) {
        parameter_index_ = 0;
        return;
    }

    parameter_index_ = (parameter_index_ + direction + count) % count;
    if (debug_mode) {
        logParameterFocus();
    }
}

DualEncoderController::ParameterId DualEncoderController::currentParameterId() const {
    switch (current_deck_) {
        case Deck::Brightness:
            return ParameterId::Photons;
        case Deck::Color:
            switch (parameter_index_ % 3) {
                case 0: return ParameterId::Chroma;
                case 1: return ParameterId::Saturation;
                default: return ParameterId::IncandescentFilter;
            }
        case Deck::Dynamics:
            switch (parameter_index_ % 3) {
                case 0: return ParameterId::Mood;
                case 1: return ParameterId::PrismCount;
                default: return ParameterId::SquareIter;
            }
        case Deck::Shows:
            return ParameterId::LightshowMode;
        case Deck::System:
            return ParameterId::Photons; // Placeholder - rotation disabled
        case Deck::Count:
            return ParameterId::Photons;
        default:
            return ParameterId::Photons;
    }
}

void DualEncoderController::adjustParameter(ParameterId parameter, int32_t ticks, uint32_t timestamp_ms) {
    const float step = kEncoderStep;
    bool secondary = secondary_focus_ && ENABLE_SECONDARY_LEDS;
    switch (parameter) {
        case ParameterId::Photons: {
            float& target = secondary ? SECONDARY_PHOTONS : CONFIG.PHOTONS;
            float new_value = clampFloat(target + (step * ticks), 0.0f, 1.0f);
            if (new_value != target) {
                target = new_value;
                settings_updated = true;
                save_config_delayed();
                if (!secondary) {
                    knob_photons.last_change = timestamp_ms;
                }
                if (debug_mode && USBSerial) {
                    USBSerial.printf("[HMI] Photons (%s) -> %.3f\n",
                                     secondary ? "secondary" : "primary",
                                     new_value);
                }
            }
            break;
        }
        case ParameterId::Chroma: {
            float& target = secondary ? SECONDARY_CHROMA : CONFIG.CHROMA;
            float new_value = clampFloat(target + (step * ticks), 0.0f, 1.0f);
            if (new_value != target) {
                target = new_value;
                settings_updated = true;
                save_config_delayed();
                if (!secondary) {
                    knob_chroma.last_change = timestamp_ms;
                }
                if (debug_mode && USBSerial) {
                    USBSerial.printf("[HMI] Chroma (%s) -> %.3f\n",
                                     secondary ? "secondary" : "primary",
                                     new_value);
                }
            }
            break;
        }
        case ParameterId::Saturation: {
            float& target = secondary ? SECONDARY_SATURATION : CONFIG.SATURATION;
            float new_value = clampFloat(target + (step * ticks), 0.0f, 1.0f);
            if (new_value != target) {
                target = new_value;
                settings_updated = true;
                save_config_delayed();
                if (debug_mode) {
                    USBSerial.printf("[HMI] Saturation (%s) -> %.3f\n",
                                     secondary ? "secondary" : "primary",
                                     new_value);
                }
            }
            break;
        }
        case ParameterId::IncandescentFilter: {
            float& target = secondary ? SECONDARY_INCANDESCENT_FILTER : CONFIG.INCANDESCENT_FILTER;
            float new_value = clampFloat(target + (step * ticks), 0.0f, 1.0f);
            if (new_value != target) {
                target = new_value;
                settings_updated = true;
                save_config_delayed();
                if (debug_mode) {
                    USBSerial.printf("[HMI] Incandescent (%s) -> %.3f\n",
                                     secondary ? "secondary" : "primary",
                                     new_value);
                }
            }
            break;
        }
        case ParameterId::Mood: {
            float& target = secondary ? SECONDARY_MOOD : CONFIG.MOOD;
            float new_value = clampFloat(target + (step * ticks), 0.0f, 1.0f);
            if (new_value != target) {
                target = new_value;
                settings_updated = true;
                save_config_delayed();
                if (!secondary) {
                    knob_mood.last_change = timestamp_ms;
                }
                if (debug_mode) {
                    USBSerial.printf("[HMI] Mood (%s) -> %.3f\n",
                                     secondary ? "secondary" : "primary",
                                     new_value);
                }
            }
            break;
        }
        case ParameterId::PrismCount: {
            if (secondary) {
                int value = static_cast<int>(SECONDARY_PRISM_COUNT) + ticks;
                value = std::max(0, std::min(8, value));
                if (value != SECONDARY_PRISM_COUNT) {
                    SECONDARY_PRISM_COUNT = static_cast<uint8_t>(value);
                    settings_updated = true;
                    save_config_delayed();
                    if (debug_mode) {
                        USBSerial.printf("[HMI] PrismCount (secondary) -> %d\n", value);
                    }
                }
            } else {
                const float prism_step = kPrismPrimaryStep;
                float new_value = clampFloat(CONFIG.PRISM_COUNT + prism_step * ticks, 0.0f, 8.0f);
                if (new_value != CONFIG.PRISM_COUNT) {
                    CONFIG.PRISM_COUNT = new_value;
                    settings_updated = true;
                    save_config_delayed();
                    if (debug_mode) {
                        USBSerial.printf("[HMI] PrismCount (primary) -> %.2f\n", new_value);
                    }
                }
            }
            break;
        }
        case ParameterId::SquareIter: {
            uint8_t& target = CONFIG.SQUARE_ITER;
            int value = static_cast<int>(target) + ticks;
            value = std::max(0, std::min(10, value));
            if (value != target) {
                target = static_cast<uint8_t>(value);
                settings_updated = true;
                save_config_delayed();
                if (debug_mode) {
                    USBSerial.printf("[HMI] SquareIter -> %d\n", value);
                }
            }
            break;
        }
        case ParameterId::LightshowMode: {
            uint8_t& target = secondary ? SECONDARY_LIGHTSHOW_MODE : CONFIG.LIGHTSHOW_MODE;
            int value = static_cast<int>(target) + ticks;
            target = wrapMode(value);
            settings_updated = true;
            save_config_delayed();
            if (debug_mode) {
                USBSerial.printf("[HMI] Lightshow (%s) -> %u\n",
                                 secondary ? "secondary" : "primary",
                                 target);
            }
            break;
        }
    }
}

void DualEncoderController::resetParameter(ParameterId parameter) {
    bool secondary = secondary_focus_ && ENABLE_SECONDARY_LEDS;
    switch (parameter) {
        case ParameterId::Photons:
            if (secondary) {
                SECONDARY_PHOTONS = CONFIG_DEFAULTS.PHOTONS;
            } else {
                CONFIG.PHOTONS = CONFIG_DEFAULTS.PHOTONS;
            }
            break;
        case ParameterId::Chroma:
            if (secondary) {
                SECONDARY_CHROMA = CONFIG_DEFAULTS.CHROMA;
            } else {
                CONFIG.CHROMA = CONFIG_DEFAULTS.CHROMA;
            }
            break;
        case ParameterId::Saturation:
            if (secondary) {
                SECONDARY_SATURATION = CONFIG_DEFAULTS.SATURATION;
            } else {
                CONFIG.SATURATION = CONFIG_DEFAULTS.SATURATION;
            }
            break;
        case ParameterId::IncandescentFilter:
            if (secondary) {
                SECONDARY_INCANDESCENT_FILTER = CONFIG_DEFAULTS.INCANDESCENT_FILTER;
            } else {
                CONFIG.INCANDESCENT_FILTER = CONFIG_DEFAULTS.INCANDESCENT_FILTER;
            }
            break;
        case ParameterId::Mood:
            if (secondary) {
                SECONDARY_MOOD = CONFIG_DEFAULTS.MOOD;
            } else {
                CONFIG.MOOD = CONFIG_DEFAULTS.MOOD;
            }
            break;
        case ParameterId::PrismCount:
            if (secondary) {
                SECONDARY_PRISM_COUNT = static_cast<uint8_t>(CONFIG_DEFAULTS.PRISM_COUNT);
            } else {
                CONFIG.PRISM_COUNT = CONFIG_DEFAULTS.PRISM_COUNT;
            }
            break;
        case ParameterId::SquareIter:
            CONFIG.SQUARE_ITER = CONFIG_DEFAULTS.SQUARE_ITER;
            break;
        case ParameterId::LightshowMode:
            if (secondary) {
                SECONDARY_LIGHTSHOW_MODE = CONFIG_DEFAULTS.LIGHTSHOW_MODE;
            } else {
                CONFIG.LIGHTSHOW_MODE = CONFIG_DEFAULTS.LIGHTSHOW_MODE;
            }
            break;
    }
    settings_updated = true;
    save_config_delayed();
}

void DualEncoderController::toggleSecondaryFocus() {
    if (!ENABLE_SECONDARY_LEDS) {
        secondary_focus_ = false;
    } else {
        secondary_focus_ = !secondary_focus_;
    }
    updateLedCues();
    if (debug_mode) {
        logDeckState("focus toggle");
        logParameterFocus();
    }
}

void DualEncoderController::triggerPrimaryActionForDeck() {
    switch (current_deck_) {
        case Deck::Brightness:
            toggleSecondaryFocus();
            break;
        case Deck::Color: {
            const uint8_t palette_total = palette_lut_count();
            if (CONFIG.PALETTE_INDEX == 0) {
                if (palette_total > 0) {
                    CONFIG.PALETTE_INDEX = 1;
                }
            } else {
                CONFIG.PALETTE_INDEX = 0;
            }
            settings_updated = true;
            save_config_delayed();
            break;
        }
        case Deck::Dynamics:
            CONFIG.SQUARE_ITER = CONFIG_DEFAULTS.SQUARE_ITER;
            settings_updated = true;
            save_config_delayed();
            break;
        case Deck::Shows:
            adjustParameter(ParameterId::LightshowMode, +1, millis());
            break;
        case Deck::System:
            triggerSystemAction();
            break;
        case Deck::Count:
            break;
        default:
            break;
    }
}

void DualEncoderController::triggerSystemAction() {
    CONFIG.MIRROR_ENABLED = !CONFIG.MIRROR_ENABLED;
    settings_updated = true;
    save_config_delayed();
}

void DualEncoderController::updateLedCues() {
    uint32_t base_color = kDeckColors[static_cast<int>(current_deck_) % (sizeof(kDeckColors) / sizeof(kDeckColors[0]))];
    uint32_t nav_color = base_color;
    uint32_t adj_color = base_color;

    if (secondary_focus_ && ENABLE_SECONDARY_LEDS) {
        nav_color = applySecondaryTint(nav_color);
        adj_color = applySecondaryTint(adj_color);
    }

    manager_.setIdleColor(kNavigationEncoder, nav_color);
    manager_.setIdleColor(kAdjustmentEncoder, adj_color);
    manager_.applyIdleColors();
}

void DualEncoderController::markInteraction(uint32_t now_ms, uint8_t active_encoder) {
    last_interaction_ms_ = now_ms;
    g_last_encoder_activity_time = now_ms;
    if (active_encoder != 0xFF && active_encoder < 8) {
        g_last_active_encoder = active_encoder;
    }
}

const char* DualEncoderController::deckName(Deck deck) const {
    switch (deck) {
        case Deck::Brightness: return "Brightness";
        case Deck::Color: return "Color";
        case Deck::Dynamics: return "Dynamics";
        case Deck::Shows: return "Shows";
        case Deck::System: return "System";
        case Deck::Count: return "Count";
        default: return "Unknown";
    }
}

const char* DualEncoderController::parameterName(ParameterId parameter) const {
    switch (parameter) {
        case ParameterId::Photons: return "Photons";
        case ParameterId::Chroma: return "Chroma";
        case ParameterId::Saturation: return "Saturation";
        case ParameterId::IncandescentFilter: return "Incandescent";
        case ParameterId::Mood: return "Mood";
        case ParameterId::PrismCount: return "Prism";
        case ParameterId::SquareIter: return "Contrast";
        case ParameterId::LightshowMode: return "Lightshow";
        default: return "Param";
    }
}

void DualEncoderController::logDeckState(const char* reason) const {
    if (!debug_mode) return;
    if (reason) {
        USBSerial.printf("[HMI] Deck -> %s (%s) (target:%s)\n",
                         deckName(current_deck_), reason,
                         secondary_focus_ ? "secondary" : "primary");
    } else {
        USBSerial.printf("[HMI] Deck -> %s (target:%s)\n",
                         deckName(current_deck_),
                         secondary_focus_ ? "secondary" : "primary");
    }
}

void DualEncoderController::logParameterFocus() const {
    if (!debug_mode) return;
    USBSerial.printf("[HMI] Focus -> %s (%s)\n",
                     parameterName(currentParameterId()),
                     secondary_focus_ ? "secondary" : "primary");
}

}  // namespace HMI

#endif  // 0

namespace {

constexpr float kBrightnessStep = 1.0f / 40.0f;

inline float clampFloat(float value, float min_val, float max_val) {
    if (value < min_val) return min_val;
    if (value > max_val) return max_val;
    return value;
}

}  // namespace

namespace HMI {

DualEncoderController::DualEncoderController() {
    EncoderHardwareConfig primary_config;
    primary_config.wire = &Wire;
    primary_config.address = 0x40;
    primary_config.sda_pin = 1;
    primary_config.scl_pin = 2;
    primary_config.bus_speed_hz = 400000U;
    manager_.setHardwareConfig(kPrimaryEncoder, primary_config);

    EncoderHardwareConfig secondary_config;
    secondary_config.wire = &Wire1;
    secondary_config.address = 0x40;
    secondary_config.sda_pin = 3;
    secondary_config.scl_pin = 4;
    secondary_config.bus_speed_hz = 400000U;
    manager_.setHardwareConfig(kSecondaryEncoder, secondary_config);

    channel_state_[0] = ChannelState{};
    channel_state_[1] = ChannelState{};
}

bool DualEncoderController::begin(bool verbose) {
    bool ok = manager_.begin(verbose);

    channel_state_[0].palette_mode = (CONFIG.PALETTE_INDEX != 0);
    channel_state_[1].palette_mode = channel_state_[0].palette_mode;

    DebugManager::preset_minimal();

    if (verbose && USBSerial) {
        USBSerial.printf("[HMI] Scroll2 (SDA=1,SCL=2) -> Channel1 %s\n",
                         ok && manager_.available(kPrimaryEncoder) ? "ONLINE" : "OFFLINE");
        USBSerial.printf("[HMI] Scroll1 (SDA=3,SCL=4) -> Channel2 %s\n",
                         ok && manager_.available(kSecondaryEncoder) ? "ONLINE" : "OFFLINE");
    }

    return ok;
}

void DualEncoderController::update(uint32_t now_ms) {
    manager_.update(now_ms);

    EncoderEvent event;
    while (manager_.popEvent(event)) {
        if (event.encoder_id == kPrimaryEncoder || event.encoder_id == kSecondaryEncoder) {
            handleChannelEvent(event.encoder_id, event);
        }
    }
}

void DualEncoderController::handleChannelEvent(uint8_t channel, const EncoderEvent& event) {
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
    float delta = kBrightnessStep * static_cast<float>(ticks);

    if (channel == kPrimaryEncoder) {
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
    } else {
        float new_value = clampFloat(SECONDARY_PHOTONS + delta, 0.0f, 1.0f);
        if (new_value != SECONDARY_PHOTONS) {
            SECONDARY_PHOTONS = new_value;
            ::settings_updated = true;
            save_config_delayed();
            if (debug_mode && USBSerial) {
                USBSerial.printf("[HMI] Channel 2 brightness -> %.3f\n", SECONDARY_PHOTONS);
            }
        }
    }
}

void DualEncoderController::adjustLightshow(uint8_t channel, int32_t ticks, uint32_t /*timestamp_ms*/) {
    if (ticks == 0) {
        return;
    }

    if (channel == kPrimaryEncoder) {
        int new_mode = wrapMode(static_cast<int>(CONFIG.LIGHTSHOW_MODE) + ticks);
        if (new_mode != CONFIG.LIGHTSHOW_MODE) {
            CONFIG.LIGHTSHOW_MODE = static_cast<uint8_t>(new_mode);
            ::settings_updated = true;
            save_config_delayed();
            if (debug_mode) {
                USBSerial.printf("[HMI] Channel 1 lightshow -> %u\n", CONFIG.LIGHTSHOW_MODE);
            }
        }
    } else {
        int new_mode = wrapMode(static_cast<int>(SECONDARY_LIGHTSHOW_MODE) + ticks);
        if (new_mode != SECONDARY_LIGHTSHOW_MODE) {
            SECONDARY_LIGHTSHOW_MODE = static_cast<uint8_t>(new_mode);
            ::settings_updated = true;
            save_config_delayed();
            if (debug_mode) {
                USBSerial.printf("[HMI] Channel 2 lightshow -> %u\n", SECONDARY_LIGHTSHOW_MODE);
            }
        }
    }
}

void DualEncoderController::toggleMode(uint8_t channel) {
    auto& state = channel_state_[channel];
    state.mode = (state.mode == ChannelControlMode::Brightness)
                     ? ChannelControlMode::Lightshow
                     : ChannelControlMode::Brightness;

    if (debug_mode) {
        USBSerial.printf("[HMI] Channel %u mode -> %s\n",
                         channel == kPrimaryEncoder ? 1 : 2,
                         state.mode == ChannelControlMode::Brightness ? "Brightness" : "Lightshow");
    }
}

void DualEncoderController::togglePalette(uint8_t channel) {
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
    channel_state_[1].palette_mode = palette_active;

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
    const int total = static_cast<int>(NUM_MODES);
    if (total <= 0) {
        return 0;
    }
    while (value < 0) value += total;
    while (value >= total) value -= total;
    return value;
}

}  // namespace HMI

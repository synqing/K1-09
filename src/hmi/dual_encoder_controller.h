#pragma once

#include <array>

#include "hmi/encoder_manager.h"

#if 0

namespace HMI {

class DualEncoderController {
public:
    enum class Deck : uint8_t {
        Brightness = 0,
        Color,
        Dynamics,
        Shows,
        System,
        Count
    };

    DualEncoderController();

    bool begin(bool verbose = false);
    void update(uint32_t now_ms);

    Deck currentDeck() const { return current_deck_; }
    uint8_t currentParameterIndex() const { return parameter_index_; }
    bool secondaryFocus() const { return secondary_focus_; }

private:
    enum class ParameterId : uint8_t {
        Photons = 0,
        Chroma,
        Saturation,
        IncandescentFilter,
        Mood,
        PrismCount,
        SquareIter,
        LightshowMode
    };

    static constexpr uint8_t kNavigationEncoder = 0;
    static constexpr uint8_t kAdjustmentEncoder = 1;
    static constexpr uint8_t kChordEncoder = 0xFE;
    static constexpr uint32_t kUiTimeoutMs = 5500;

    EncoderManager manager_;
    Deck current_deck_ = Deck::Brightness;
    uint8_t parameter_index_ = 0;
    bool secondary_focus_ = false;
    uint32_t last_interaction_ms_ = 0;

    void handleEvent(const EncoderEvent& event);
    void handleNavigationEvent(const EncoderEvent& event);
    void handleAdjustmentEvent(const EncoderEvent& event);
    void handleChordEvent(const EncoderEvent& event);

    void changeDeck(int32_t direction);
    void advanceParameter(int8_t direction);
    ParameterId currentParameterId() const;

    void adjustParameter(ParameterId parameter, int32_t ticks, uint32_t timestamp_ms);
    void resetParameter(ParameterId parameter);

    void toggleSecondaryFocus();
    void triggerPrimaryActionForDeck();
    void triggerSystemAction();

    void updateLedCues();
    void markInteraction(uint32_t now_ms, uint8_t active_encoder = 0xFF);
    const char* deckName(Deck deck) const;
    const char* parameterName(ParameterId parameter) const;
    void logDeckState(const char* reason = nullptr) const;
    void logParameterFocus() const;
};

}  // namespace HMI

#endif  // 0

namespace HMI {

class DualEncoderController {
public:
    DualEncoderController();

    bool begin(bool verbose = false);
    void update(uint32_t now_ms);

private:
    enum class ChannelControlMode : uint8_t {
        Brightness = 0,
        Lightshow
    };

    struct ChannelState {
        ChannelControlMode mode = ChannelControlMode::Brightness;
        bool palette_mode = false;
    };

    static constexpr uint8_t kPrimaryEncoder = 0;   // Scroll2 (SDA=1,SCL=2)
    static constexpr uint8_t kSecondaryEncoder = 1; // Scroll1 (SDA=3,SCL=4)

    EncoderManager manager_;
    ChannelState channel_state_[2];

    void handleChannelEvent(uint8_t channel, const EncoderEvent& event);
    void adjustBrightness(uint8_t channel, int32_t ticks, uint32_t timestamp_ms);
    void adjustLightshow(uint8_t channel, int32_t ticks, uint32_t timestamp_ms);
    void toggleMode(uint8_t channel);
    void togglePalette(uint8_t channel);
    void markInteraction(uint32_t timestamp_ms, uint8_t channel);
    int wrapMode(int value) const;
};

}  // namespace HMI

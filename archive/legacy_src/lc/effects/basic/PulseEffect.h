#ifndef PULSE_EFFECT_H
#define PULSE_EFFECT_H

#include <algorithm>
#include <cmath>

#include "../EffectBase.h"

class PulseEffect : public EffectBase {
private:
    uint8_t pulsePhase = 0;
    
public:
    PulseEffect() : EffectBase("Pulse", 160, 20, 10) {}
    
    void render() override {
        const auto& metrics = audioMetrics();
        const auto& spectrum = metrics.spectrogram;

        const float punch = std::clamp(metrics.currentPunch, 0.0f, 1.0f);
        const float baseBrightness = std::clamp(metrics.brightness, 0.0f, 1.0f);

        uint8_t fadeValue = static_cast<uint8_t>(std::max(8.0f, fadeAmount * (0.5f + (1.0f - punch) * 0.5f)));
        fadeToBlackBy(leds, HardwareConfig::NUM_LEDS, fadeValue);

        uint16_t pulseRate = static_cast<uint16_t>(std::clamp(18.0f + punch * 90.0f, 18.0f, 140.0f));
        pulsePhase += pulseRate;

        const size_t spectrumSize = spectrum.size();

        for (uint16_t i = 0; i < HardwareConfig::NUM_LEDS; i++) {
            size_t bin = spectrumSize ? (static_cast<size_t>(i) * spectrumSize) / HardwareConfig::NUM_LEDS : 0;
            float energy = spectrumSize ? spectrum[bin] : punch;
            energy = std::clamp(energy, 0.0f, 1.0f);

            float radial = static_cast<float>(radii[i]) / 255.0f;
            float envelope = std::sqrt(energy) * (0.35f + radial * 0.65f);
            float brightnessNorm = std::clamp(baseBrightness * envelope, 0.0f, 1.0f);
            uint8_t brightness = static_cast<uint8_t>(brightnessNorm * 255.0f);

            uint8_t hueOffset = static_cast<uint8_t>(radial * 32.0f);
            uint8_t colorIndex = angles[i] + pulsePhase + hueOffset;

            CRGB color = ColorFromPalette(currentPalette, colorIndex, brightness, LINEARBLEND);
            leds[i] += color;
        }

        #if FEATURE_DEBUG_OUTPUT
        static uint32_t lastDebug = 0;
        if (millis() - lastDebug > 1500) {
            Serial.print(F("[EFFECT] Pulse (audio) punch="));
            Serial.printf("%.2f", punch);
            Serial.print(F(" rate="));
            Serial.print(pulseRate);
            Serial.println();
            lastDebug = millis();
        }
        #endif
    }
};

#endif // PULSE_EFFECT_H

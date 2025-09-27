#ifndef GRADIENT_EFFECT_H
#define GRADIENT_EFFECT_H

#include <algorithm>
#include <array>
#include <cmath>

#include <Arduino.h>

#include "../EffectBase.h"

class GradientEffect : public EffectBase {
private:
    uint8_t effectiveFade;
    uint8_t effectiveSpeed;
    
public:
    GradientEffect() : EffectBase("Gradient", 128, 10, 20) {}
    
    void render() override {
        const auto& metrics = audioMetrics();
        const auto& spectrum = metrics.spectrogram;
        const auto& chroma = metrics.chroma;

        const float punch = std::clamp(metrics.currentPunch, 0.0f, 1.0f);
        const float baseBrightness = std::clamp(metrics.brightness, 0.0f, 1.0f);

        static float smoothedPunch = 0.0f;
        smoothedPunch = 0.85f * smoothedPunch + 0.15f * punch;

        constexpr size_t kBands = 8;
        std::array<float, kBands> bandEnergy{};
        std::array<uint16_t, kBands> bandCounts{};

        const size_t spectrumSize = spectrum.size();
        if (spectrumSize > 0) {
            for (size_t i = 0; i < spectrumSize; ++i) {
                size_t band = (i * kBands) / spectrumSize;
                if (band >= kBands) band = kBands - 1;
                bandEnergy[band] += spectrum[i];
                bandCounts[band]++;
            }
            for (size_t b = 0; b < kBands; ++b) {
                if (bandCounts[b] > 0) {
                    bandEnergy[b] /= static_cast<float>(bandCounts[b]);
                }
            }
        }

        float globalAvg = 0.0f;
        float globalMax = 0.0f;
        for (size_t b = 0; b < kBands; ++b) {
            globalAvg += bandEnergy[b];
            if (bandEnergy[b] > globalMax) {
                globalMax = bandEnergy[b];
            }
        }
        globalAvg /= static_cast<float>(kBands);
        const float span = std::max(0.05f, globalMax - globalAvg);

        static std::array<float, kBands> bandHistory{};
        for (size_t b = 0; b < kBands; ++b) {
            float energyNorm = (bandEnergy[b] - globalAvg) / span;
            energyNorm = std::clamp(energyNorm, 0.0f, 1.0f);
            bandHistory[b] = 0.7f * bandHistory[b] + 0.3f * energyNorm;
        }

        size_t dominantChroma = 0;
        float dominantValue = 0.0f;
        for (size_t i = 0; i < chroma.size(); ++i) {
            if (chroma[i] > dominantValue) {
                dominantValue = chroma[i];
                dominantChroma = i;
            }
        }
        const uint8_t chromaOffset = static_cast<uint8_t>((dominantChroma * 255) / std::max<size_t>(1, chroma.size()));

        uint8_t fadeFloor = static_cast<uint8_t>(std::max(6.0f, 24.0f - smoothedPunch * 18.0f));
        effectiveFade = std::max<uint8_t>(fadeFloor, fadeAmount);
        fadeToBlackBy(leds, HardwareConfig::NUM_LEDS, effectiveFade);

        effectiveSpeed = constrain(static_cast<int>(paletteSpeed * (1.0f + smoothedPunch * 3.0f)), 1, 120);
        const uint32_t now = millis();
        const uint8_t paletteOffset = (now / effectiveSpeed) & 0xFF;

        for (uint16_t ledIndex = 0; ledIndex < HardwareConfig::NUM_LEDS; ++ledIndex) {
            size_t band = (ledIndex * kBands) / HardwareConfig::NUM_LEDS;
            if (band >= kBands) band = kBands - 1;

            float energy = std::clamp(bandHistory[band], 0.0f, 1.0f);
            float boosted = std::pow(energy, 1.5f);
            float brightnessNorm = baseBrightness * 0.25f + boosted * (0.9f + smoothedPunch * 0.6f);
            brightnessNorm = std::clamp(brightnessNorm, 0.0f, 1.0f);

            uint8_t brightness = static_cast<uint8_t>(brightnessNorm * 255.0f);
            uint8_t radialScale = map(radii[ledIndex], 0, 255, 80, 255);
            brightness = scale8(brightness, radialScale);

            uint8_t energyHue = static_cast<uint8_t>(boosted * 96.0f + smoothedPunch * 48.0f);
            uint8_t paletteIndex = angles[ledIndex] + paletteOffset + chromaOffset + energyHue;

            CRGB color = ColorFromPalette(currentPalette, paletteIndex, brightness, LINEARBLEND);
            leds[ledIndex] = color;
        }

        #if FEATURE_DEBUG_OUTPUT
        static uint32_t lastDebugTime = 0;
        if (millis() - lastDebugTime > 2000) {
            Serial.print(F("[EFFECT] Gradient(audio) fade="));
            Serial.print(effectiveFade);
            Serial.print(F(" speed="));
            Serial.print(effectiveSpeed);
            Serial.print(F(" punch="));
            Serial.printf("%.2f", punch);
            Serial.print(F(" avgEnergy="));
            Serial.print(globalAvg, 3);
            Serial.print(F(" maxEnergy="));
            Serial.print(globalMax, 3);
            Serial.println();
            lastDebugTime = millis();
        }
        #endif
    }
};

#endif // GRADIENT_EFFECT_H

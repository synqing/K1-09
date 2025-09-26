#pragma once

#include "../config/features.h"

#if FEATURE_PIXELSPORK

#include <Pixel_Spork.h>

// Lightweight helper that wraps PixelSpork's EffectSetPS and EffectSetFaderPS
// so we can gradually migrate existing scene orchestration toward PixelSpork's
// show-control utilities. This controller allocates the effect pointer array
// and exposes helpers for wiring effects + optional fades.
class PixelSporkSetController {
public:
    PixelSporkSetController() = default;
    ~PixelSporkSetController();

    // Allocate storage for up to `capacity` effects and build an EffectSetPS
    // around it. Existing storage is released if present.
    void init(uint8_t capacity, uint16_t runTimeMs);

    // Assign an effect pointer at a given slot (wraps EffectSetPS::setEffect).
    void setEffect(uint8_t index, EffectBasePS* effectPtr);

    // Optional fade helper (wraps EffectSetFaderPS). If not needed, skip.
    void attachFader(uint16_t fadeMs, bool fadeIn = true, bool fadeOut = true);

    // Update the underlying set/fader (call from loop when active).
    void update();

    // Reset both set + fader timers/brightness.
    void reset();

    EffectSetPS* effectSet() { return effectSet_; }
    EffectSetFaderPS* fader() { return fader_; }

private:
    void release();

    EffectBasePS** effectArray_ = nullptr;
    uint8_t capacity_ = 0;

    EffectSetPS* effectSet_ = nullptr;
    EffectSetFaderPS* fader_ = nullptr;
};

#endif // FEATURE_PIXELSPORK

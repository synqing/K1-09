#include "PixelSporkSetController.h"

#if FEATURE_PIXELSPORK

PixelSporkSetController::~PixelSporkSetController() {
    release();
}

void PixelSporkSetController::release() {
    delete fader_;
    fader_ = nullptr;

    delete effectSet_;
    effectSet_ = nullptr;

    delete[] effectArray_;
    effectArray_ = nullptr;
    capacity_ = 0;
}

void PixelSporkSetController::init(uint8_t capacity, uint16_t runTimeMs) {
    release();
    if (capacity == 0) {
        return;
    }

    capacity_ = capacity;
    effectArray_ = new EffectBasePS*[capacity_];
    for (uint8_t i = 0; i < capacity_; ++i) {
        effectArray_[i] = nullptr;
    }

    effectSet_ = new EffectSetPS(effectArray_, capacity_, runTimeMs);
    effectSet_->effectDestLimit = 0;
}

void PixelSporkSetController::setEffect(uint8_t index, EffectBasePS* effectPtr) {
    if (!effectSet_ || !effectArray_) {
        return;
    }
    if (index >= capacity_) {
        index %= capacity_;
    }
    effectSet_->setEffect(effectPtr, index);
}

void PixelSporkSetController::attachFader(uint16_t fadeMs, bool fadeIn, bool fadeOut) {
    if (!effectSet_) {
        return;
    }
    delete fader_;
    fader_ = new EffectSetFaderPS(*effectSet_, fadeMs, fadeIn, fadeOut);
    // Place the fader at slot 0 by convention (matches library docs)
    setEffect(0, fader_);
    // Prevent accidental destruction of the fader when using destruct limits
    effectSet_->effectDestLimit = 1;
}

void PixelSporkSetController::update() {
    if (fader_) {
        fader_->update();
    }
    if (effectSet_) {
        effectSet_->update();
    }
}

void PixelSporkSetController::reset() {
    if (effectSet_) {
        effectSet_->reset();
    }
    if (fader_) {
        fader_->reset();
    }
}

#endif // FEATURE_PIXELSPORK

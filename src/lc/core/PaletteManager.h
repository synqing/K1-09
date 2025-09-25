#ifndef PALETTE_MANAGER_H
#define PALETTE_MANAGER_H

#include <Arduino.h>
#include <FastLED.h>
#include "../config/hardware_config.h"
#include "../config/features.h"

// Forward declare the gradient palettes arrays
extern const TProgmemRGBGradientPaletteRef gGradientPalettes[];
extern const uint8_t gGradientPaletteCount;

// Forward declare Crameri palettes
extern const TProgmemRGBGradientPaletteRef gCrameriPalettes[];
extern const uint8_t gCrameriPaletteCount;

class PaletteManager {
public:
    enum PaletteType {
        STANDARD,
        CRAMERI,
        PALETTE_TYPE_COUNT
    };

private:
    CRGBPalette16 currentPalette;
    CRGBPalette16 targetPalette;
    uint8_t currentPaletteIndex;
    uint8_t blendSpeed;
    PaletteType currentPaletteType;
    
    // Palette names for UI display
    static const char* standardPaletteNames[];
    static const char* const* crameriPaletteNames;
    
    // Get the current palette array based on type
    const TProgmemRGBGradientPaletteRef* getCurrentPaletteArray() const {
        return (currentPaletteType == STANDARD) ? gGradientPalettes : gCrameriPalettes;
    }
    
    // Get the current palette count based on type
    uint8_t getCurrentPaletteCount() const {
        return (currentPaletteType == STANDARD) ? gGradientPaletteCount : gCrameriPaletteCount;
    }
    
public:
    PaletteManager() : 
        currentPaletteIndex(0),
        blendSpeed(24),
        currentPaletteType(STANDARD) {}
    
    void begin() {
        // Initialize with first palette
        setPalette(0, true);
    }

    void setPaletteType(PaletteType type, bool resetToFirst = true);

    void setPalette(uint8_t index, bool force = false) {
        uint8_t count = getCurrentPaletteCount();
        if (index >= count) return;

        currentPaletteIndex = index;
        const TProgmemRGBGradientPaletteRef* palettes = getCurrentPaletteArray();
        targetPalette = CRGBPalette16(palettes[index]);
        if (force) {
            currentPalette = targetPalette;
        }
        
        #if FEATURE_DEBUG_OUTPUT
        Serial.print(F("[PALETTE] Changed to: "));
        Serial.print(getCurrentTypeName());
        Serial.print(F(" - "));
        Serial.print(index);
        Serial.print(F(" - "));
        Serial.println(getCurrentName());
        #endif
    }
    
    void nextPalette() {
        uint8_t count = getCurrentPaletteCount();
        setPalette((currentPaletteIndex + 1) % count);
    }
    
    void prevPalette() {
        uint8_t count = getCurrentPaletteCount();
        setPalette((currentPaletteIndex == 0) ? count - 1 : currentPaletteIndex - 1);
    }
    
    void updatePaletteBlending() {
        nblendPaletteTowardPalette(currentPalette, targetPalette, blendSpeed);
    }
    
    void setBlendSpeed(uint8_t speed) {
        blendSpeed = speed;
    }
    
    // Getters
    CRGBPalette16& getCurrentPalette() { return currentPalette; }
    CRGBPalette16& getTargetPalette() { return targetPalette; }
    uint8_t getCurrentIndex() const { return currentPaletteIndex; }
    PaletteType getCurrentType() const { return currentPaletteType; }
    
    const char* getCurrentName() const {
        if (currentPaletteType == STANDARD) {
            return standardPaletteNames[currentPaletteIndex];
        } else {
            return crameriPaletteNames[currentPaletteIndex];
        }
    }
    
    const char* getCurrentTypeName() const {
        static const char* typeNames[] = {"Standard", "Crameri"};
        return typeNames[currentPaletteType];
    }
    
    const char* const* getPaletteNames() const {
        return (currentPaletteType == STANDARD) ? standardPaletteNames : crameriPaletteNames;
    }
    
    uint8_t getPaletteCount() const {
        return getCurrentPaletteCount();
    }

    // Combined palette helpers (Standard + Crameri)
    uint16_t getCombinedCount() const;
    uint16_t getCombinedIndex() const;
    bool setCombinedIndex(uint16_t index, bool force = false);
    
    // Static methods to get type information
    static const char* getTypeName(PaletteType type) {
        static const char* typeNames[] = {"Standard", "Crameri"};
        return (type < PALETTE_TYPE_COUNT) ? typeNames[type] : "Unknown";
    }
    
    static uint8_t getTypeCount() {
        return PALETTE_TYPE_COUNT;
    }
};

// External declaration of Crameri palette names (defined elsewhere)
extern const char* const CrameriPaletteNames[];

#endif // PALETTE_MANAGER_H

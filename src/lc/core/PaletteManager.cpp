#include "PaletteManager.h"

// Define standard palette names
const char* PaletteManager::standardPaletteNames[] = {
    "Sunset_Real", "es_rivendell_15", "es_ocean_breeze_036", "rgi_15", "retro2_16",
    "Analogous_1", "es_pinksplash_08", "Coral_reef", "es_ocean_breeze_068", "es_pinksplash_07",
    "es_vintage_01", "departure", "es_landscape_64", "es_landscape_33", "rainbowsherbet",
    "gr65_hult", "gr64_hult", "GMT_drywet", "ib_jul01", "es_vintage_57",
    "ib15", "Fuschia_7", "es_emerald_dragon_08", "lava", "fire",
    "Colorfull", "Magenta_Evening", "Pink_Purple", "Sunset_Real", "es_autumn_19",
    "BlacK_Blue_Magenta_White", "BlacK_Magenta_Red", "BlacK_Red_Magenta_Yellow"
};

// External declaration of Crameri palette names
extern const char* const CrameriPaletteNames[];
const char* const* PaletteManager::crameriPaletteNames = CrameriPaletteNames;

uint16_t PaletteManager::getCombinedCount() const {
    return static_cast<uint16_t>(gGradientPaletteCount) + static_cast<uint16_t>(gCrameriPaletteCount);
}

void PaletteManager::setPaletteType(PaletteType type, bool resetToFirst) {
    if (type >= PALETTE_TYPE_COUNT) return;

    if (type != currentPaletteType) {
        currentPaletteType = type;
        uint8_t count = getCurrentPaletteCount();
        uint8_t index = resetToFirst ? 0 : min(currentPaletteIndex, static_cast<uint8_t>(count ? count - 1 : 0));
        setPalette(index, true);
    } else if (resetToFirst) {
        setPalette(0, true);
    }
}

uint16_t PaletteManager::getCombinedIndex() const {
    if (currentPaletteType == STANDARD) {
        return currentPaletteIndex;
    }
    return static_cast<uint16_t>(gGradientPaletteCount) + currentPaletteIndex;
}

bool PaletteManager::setCombinedIndex(uint16_t index, bool force) {
    uint16_t total = getCombinedCount();
    if (total == 0) return false;

    index %= total;
    PaletteType desiredType = (index < gGradientPaletteCount) ? STANDARD : CRAMERI;
    uint8_t localIndex = (desiredType == STANDARD)
                             ? static_cast<uint8_t>(index)
                             : static_cast<uint8_t>(index - gGradientPaletteCount);

    if (desiredType != currentPaletteType) {
        setPaletteType(desiredType, false);
    }

    setPalette(localIndex, force);
    return true;
}

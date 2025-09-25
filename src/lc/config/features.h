#ifndef FEATURES_H
#define FEATURES_H

#include "../config.h"

// Feature flags to enable/disable functionality at compile time.
// Sensory Bridge owns the high-level toggles via SB_LC_ENABLE_* macros.

// Core features
#define FEATURE_SERIAL_MENU         SB_LC_ENABLE_SERIAL_MENU
#define FEATURE_PERFORMANCE_MONITOR SB_LC_ENABLE_PERFORMANCE_MONITOR
#define FEATURE_BUTTON_CONTROL      1

// Network features (disabled until SB provides adapters)
#define FEATURE_WEB_SERVER          SB_LC_ENABLE_WEB
#define FEATURE_WEBSOCKET           0
#define FEATURE_OTA_UPDATE          0

// Effect categories
#define FEATURE_BASIC_EFFECTS       1
#define FEATURE_ADVANCED_EFFECTS    1
#define FEATURE_PIPELINE_EFFECTS    1
#define FEATURE_AUDIO_EFFECTS       0

// Hardware optimization
#define FEATURE_HARDWARE_OPTIMIZATION 1
#define FEATURE_FASTLED_OPTIMIZATION  1
#define FEATURE_ENCODERS              SB_LC_ENABLE_ENCODERS

// Optional PixelSpork integration (SegmentSet + Color Modes adapter)
#define FEATURE_PIXELSPORK           SB_LC_ENABLE_PIXELSPORK

// Debug features
#define FEATURE_DEBUG_OUTPUT         1
#define FEATURE_MEMORY_DEBUG         0
#define FEATURE_TIMING_DEBUG         1

// Derived flags
#if FEATURE_WEB_SERVER || FEATURE_WEBSOCKET
    #define FEATURE_NETWORK 1
#else
    #define FEATURE_NETWORK 0
#endif

#if FEATURE_PERFORMANCE_MONITOR || FEATURE_MEMORY_DEBUG || FEATURE_TIMING_DEBUG
    #define FEATURE_PROFILING 1
#else
    #define FEATURE_PROFILING 0
#endif

#endif // FEATURES_H

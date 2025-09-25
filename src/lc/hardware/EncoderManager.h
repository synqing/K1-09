#ifndef ENCODER_MANAGER_H
#define ENCODER_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <FastLED.h>
#include "../config/hardware_config.h"
#include "../config/features.h"
#include "../core/FxEngine.h"
#include "../core/PaletteManager.h"
#include "../core/RuntimeTunables.h"

#if FEATURE_ENCODERS
#include "../core/ScenesManager.h"
#include <m5rotate8.h>

// Externs from globals and main
extern FxEngine fxEngine;
extern PaletteManager paletteManager;
extern uint8_t brightnessVal;
extern uint8_t fadeAmount;
extern uint8_t paletteSpeed;
extern uint16_t fps;

class EncoderManager {
 public:
  bool begin(uint8_t sda = HardwareConfig::I2C_SDA, uint8_t scl = HardwareConfig::I2C_SCL);
  void update(uint32_t now_ms);
  void updateLEDs(uint32_t now_ms);
  enum class Layer : uint8_t { NAV = 0, EDIT = 1, PLAY = 2 };
  Layer getLayer() const { return layer; }

 private:
  M5ROTATE8 enc{M5ROTATE8_DEFAULT_ADDRESS, &Wire};
  bool available = false;
  uint8_t lastActive = 255;
  uint32_t lastActivity = 0;
  Layer layer = Layer::NAV;
  uint32_t layerChangeFlashUntil = 0;

  // Simple per-channel accumulators for coarse actions (palette/effect change)
  int32_t accPal = 0;
  int32_t accFx  = 0;
  uint8_t selectedScene = 0;
  uint32_t nextPollAt = 0;
  uint32_t nextLedUpdateAt = 0;
  bool     dwellAdjust = false;

  // PLAY layer state
  bool     lfoEnabled = false;
  float    lfoRateHz  = 0.5f; // 0.5 Hz default
  uint8_t  lfoDepth   = 64;   // 0..255
  uint8_t  lfoTarget  = 0;    // 0=fade,1=speed,2=brightness,3=vignette,4=blur
  uint32_t lfoPhase   = 0;    // 16.16 fixed phase accumulator
  uint32_t lfoLastMs  = 0;
  // Baselines
  uint8_t  baseFade = 20;
  uint8_t  baseSpeed = 10;
  uint8_t  baseBrightness = 96;
  uint8_t  baseVignette = 64;
  uint8_t  baseBlur = 24;

  void cycleLayer();
  void indicateLayer();
  void handleButtonCH3(uint32_t now_ms, bool pressed);
  void handleSceneControl(uint32_t now_ms, int32_t rel, bool shortPress, bool longPress);
  void tickLFO(uint32_t now_ms);
  // I2C probing and recovery
  bool scanBus(uint8_t& foundAddr);
  bool configureAtAddress(uint8_t addr);
  uint32_t nextScanAt = 0;

  void applyRelative(uint8_t ch, int32_t rel);
};

#else
class EncoderManager {
 public:
  bool begin(uint8_t, uint8_t) { return false; }
  void update(uint32_t) {}
  void updateLEDs(uint32_t) {}
};
#endif

#endif // ENCODER_MANAGER_H

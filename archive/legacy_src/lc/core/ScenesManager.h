#ifndef SCENES_MANAGER_H
#define SCENES_MANAGER_H

#include <Arduino.h>
#include <FastLED.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include "../config/hardware_config.h"
#include "../core/FxEngine.h"
#include "../core/PaletteManager.h"
#include "../core/RuntimeTunables.h"
#include "../config/features.h"
#if FEATURE_PIXELSPORK
#include "../middleware/PixelSporkAdapter.h"
extern PixelSporkAdapter gSpork;
#endif

// Externs from global state
extern FxEngine fxEngine;
extern PaletteManager paletteManager;
extern uint8_t brightnessVal;
extern uint8_t fadeAmount;
extern uint8_t paletteSpeed;
extern uint16_t fps;
extern uint8_t currentPaletteIndex;

class ScenesManager {
public:
  static constexpr uint8_t kMaxScenes = 8;

  struct Scene {
    uint8_t brightness = HardwareConfig::DEFAULT_BRIGHTNESS;
    uint8_t fade = 20;
    uint8_t speed = 10;
    uint16_t targetFps = HardwareConfig::DEFAULT_FPS;
    uint8_t effectIndex = 0;
    uint8_t paletteIndex = 0; // index within type
    uint8_t paletteType = 0;  // 0=Standard, 1=Crameri
    bool dither = false;
    uint16_t sporkOffsetRate = 0;
    uint8_t sporkOffsetStep = 1;
    uint16_t sporkGradLen = 0;
    uint16_t sporkFade = 2000;
  };

  void begin() { loadFromFS(); }

  void setSelected(uint8_t s) { selected = s % kMaxScenes; }
  uint8_t getSelected() const { return selected; }

  // Capture current runtime state into slot
  void capture(uint8_t slot) {
    Scene &sc = scenes[slot % kMaxScenes];
    sc.brightness = brightnessVal;
    sc.fade = fadeAmount;
    sc.speed = paletteSpeed;
    sc.targetFps = fps;
    sc.effectIndex = fxEngine.getCurrentEffectIndex();
    sc.paletteType = static_cast<uint8_t>(paletteManager.getCurrentType());
    sc.paletteIndex = paletteManager.getCurrentIndex();
    sc.sporkOffsetRate = rt_spork_offset_rate_ms;
    sc.sporkOffsetStep = rt_spork_offset_step;
    sc.sporkGradLen = rt_spork_grad_len_override;
    sc.sporkFade = rt_spork_fade_ms;
    // Best-effort: we don't track current FastLED dither state; assume last set via encoder
    // Keep stored value as-is unless caller specifies
  }

  // Apply scene to runtime
  void apply(uint8_t slot, bool withTransition = true) {
    const Scene &sc = scenes[slot % kMaxScenes];
    brightnessVal = sc.brightness;
    fadeAmount = sc.fade;
    paletteSpeed = sc.speed;
    fps = sc.targetFps;
    FastLED.setBrightness(brightnessVal);

    // Palette
    paletteManager.setPaletteType(static_cast<PaletteManager::PaletteType>(sc.paletteType), false);
    paletteManager.setPalette(sc.paletteIndex, true);

    // Effect
    if (withTransition) {
      fxEngine.setEffect(sc.effectIndex, 0, 800);
    } else {
      fxEngine.setEffect(sc.effectIndex, 0, 0);
    }

    // Dither
    FastLED.setDither(sc.dither ? 1 : 0);

    rt_spork_offset_rate_ms = sc.sporkOffsetRate;
    rt_spork_offset_step = sc.sporkOffsetStep;
    rt_spork_grad_len_override = sc.sporkGradLen;
    rt_spork_fade_ms = sc.sporkFade;

    #if FEATURE_PIXELSPORK
    gSpork.syncFromRuntime();
    #endif
  }

  // Save one scene to FS
  bool saveToFS() {
    JsonDocument doc;
    JsonArray arr = doc["scenes"].to<JsonArray>();
    for (uint8_t i = 0; i < kMaxScenes; ++i) {
      JsonObject o = arr.add<JsonObject>();
      const Scene &s = scenes[i];
      o["b"] = s.brightness;
      o["f"] = s.fade;
      o["s"] = s.speed;
      o["fps"] = s.targetFps;
      o["e"] = s.effectIndex;
      o["pt"] = s.paletteType;
      o["pi"] = s.paletteIndex;
      o["d"] = s.dither;
      o["sor"] = s.sporkOffsetRate;
      o["sos"] = s.sporkOffsetStep;
      o["sgl"] = s.sporkGradLen;
      o["sfd"] = s.sporkFade;
    }
    File f = SPIFFS.open("/scenes.json", FILE_WRITE);
    if (!f) return false;
    bool ok = (serializeJson(doc, f) > 0);
    f.close();
    return ok;
  }

  bool loadFromFS() {
    if (!SPIFFS.exists("/scenes.json")) return false;
    File f = SPIFFS.open("/scenes.json", FILE_READ);
    if (!f) return false;
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) return false;
    JsonArray arr = doc["scenes"].as<JsonArray>();
    if (arr.isNull()) return false;
    uint8_t i = 0;
    for (JsonObject o : arr) {
      if (i >= kMaxScenes) break;
      Scene &s = scenes[i++];
      s.brightness = o["b"] | s.brightness;
      s.fade = o["f"] | s.fade;
      s.speed = o["s"] | s.speed;
      s.targetFps = o["fps"] | s.targetFps;
      s.effectIndex = o["e"] | s.effectIndex;
      s.paletteType = o["pt"] | s.paletteType;
      s.paletteIndex = o["pi"] | s.paletteIndex;
      s.dither = o["d"] | s.dither;
      s.sporkOffsetRate = o["sor"] | s.sporkOffsetRate;
      s.sporkOffsetStep = o["sos"] | s.sporkOffsetStep;
      s.sporkGradLen = o["sgl"] | s.sporkGradLen;
      s.sporkFade = o["sfd"] | s.sporkFade;
    }
    return true;
  }

  Scene& scene(uint8_t slot) { return scenes[slot % kMaxScenes]; }

private:
  Scene scenes[kMaxScenes];
  uint8_t selected = 0;
};

#endif // SCENES_MANAGER_H

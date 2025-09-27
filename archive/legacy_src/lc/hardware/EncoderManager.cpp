#include "EncoderManager.h"

#if FEATURE_ENCODERS

static inline uint8_t clamp8(int v) {
  return v < 0 ? 0 : (v > 255 ? 255 : (uint8_t)v);
}

static inline uint8_t clampRange8(int v, int lo, int hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return (uint8_t)v;
}

#if FEATURE_DEBUG_OUTPUT
static const __FlashStringHelper* layerLabel(EncoderManager::Layer layer) {
  switch (layer) {
    case EncoderManager::Layer::NAV:  return F("NAV");
    case EncoderManager::Layer::EDIT: return F("EDIT");
    case EncoderManager::Layer::PLAY: return F("PLAY");
  }
  return F("?");
}
#endif

bool EncoderManager::begin(uint8_t sda, uint8_t scl) {
  // Strategy: try default Wire pins first (variant pins), then fall back to configured pins.
  // This avoids "Invalid pin" errors on some variants.
  uint8_t addr = 0;

  Wire.begin();
  Wire.setClock(400000);
  delay(10);
  if (scanBus(addr) && configureAtAddress(addr)) {
    Serial.print(F("[ENC] M5ROTATE8 initialized (default pins) at 0x")); Serial.println(addr, HEX);
    available = true;
    enc.setAll(0, 0, 4);
    return true;
  }

  Serial.println(F("[ENC] Default Wire pins found nothing; trying configured pins"));
  // Try configured pins @400kHz, then 100kHz
  Wire.begin(sda, scl);
  Wire.setClock(400000);
  delay(10);
  if (!scanBus(addr)) {
    Serial.println(F("[ENC] I2C scan (400kHz) found no devices; retrying at 100kHz"));
    Wire.setClock(100000);
    delay(10);
    if (!scanBus(addr)) {
      Serial.println(F("[ENC] I2C scan failed; encoders unavailable"));
      available = false;
      nextScanAt = millis() + 5000;
      return false;
    }
  }
  if (configureAtAddress(addr)) {
    Serial.print(F("[ENC] M5ROTATE8 initialized (configured pins) at 0x")); Serial.println(addr, HEX);
    available = true;
    enc.setAll(0, 0, 4);
    return true;
  }
  Serial.println(F("[ENC] Detected I2C device did not respond as M5ROTATE8"));
  available = false;
  nextScanAt = millis() + 5000;
  return false;
}

void EncoderManager::applyRelative(uint8_t ch, int32_t rel) {
  if (rel == 0) return;
  lastActive = ch;
  lastActivity = millis();

#if FEATURE_DEBUG_OUTPUT
  auto layerName = [](Layer layer) {
    switch (layer) {
      case Layer::NAV:  return F("NAV");
      case Layer::EDIT: return F("EDIT");
      case Layer::PLAY: return F("PLAY");
    }
    return F("?");
  };
#endif

  switch (ch) {
    case 0: { // Brightness 0..255
      if (layer == Layer::NAV) {
        int v = (int)brightnessVal + (rel * 4);
        brightnessVal = clamp8(v);
        FastLED.setBrightness(brightnessVal);
#if FEATURE_DEBUG_OUTPUT
        Serial.print(F("[ENC][")); Serial.print(layerName(layer)); Serial.print(F("] Brightness -> "));
        Serial.println(brightnessVal);
#endif
      } else if (layer == Layer::EDIT) {
        // Green trim scale
        int v = (int)rt_curation_green_scale + (rel * 2);
        rt_curation_green_scale = clamp8(v);
        rt_enable_palette_curation = true;
#if FEATURE_DEBUG_OUTPUT
        Serial.print(F("[ENC][")); Serial.print(layerName(layer)); Serial.print(F("] Palette Green Scale -> "));
        Serial.println(rt_curation_green_scale);
#endif
      } else { // PLAY
        // LFO rate 0.05..5 Hz
        float delta = rel * 0.05f;
        lfoRateHz += delta;
        if (lfoRateHz < 0.05f) lfoRateHz = 0.05f;
        if (lfoRateHz > 5.0f) lfoRateHz = 5.0f;
#if FEATURE_DEBUG_OUTPUT
        Serial.print(F("[ENC][")); Serial.print(layerName(layer)); Serial.print(F("] LFO Rate Hz -> "));
        Serial.println(lfoRateHz, 2);
#endif
      }
      break;
    }
    case 1: { // Palette step via accumulator
      if (layer == Layer::NAV) {
        accPal += rel;
        const int step = 2;
        if (accPal >= step) {
          uint16_t total = paletteManager.getCombinedCount();
          if (total > 0) {
            uint16_t idx = paletteManager.getCombinedIndex();
            idx = (idx + 1) % total;
            paletteManager.setCombinedIndex(idx);
            currentPaletteIndex = static_cast<uint8_t>(idx);
#if FEATURE_DEBUG_OUTPUT
            Serial.print(F("[ENC][NAV] Next Palette -> "));
            Serial.println(idx);
#endif
          }
          accPal = 0;
        }
        else if (accPal <= -step) {
          uint16_t total = paletteManager.getCombinedCount();
          if (total > 0) {
            uint16_t idx = paletteManager.getCombinedIndex();
            idx = (idx == 0) ? total - 1 : idx - 1;
            paletteManager.setCombinedIndex(idx);
            currentPaletteIndex = static_cast<uint8_t>(idx);
#if FEATURE_DEBUG_OUTPUT
            Serial.print(F("[ENC][NAV] Prev Palette -> "));
            Serial.println(idx);
#endif
          }
          accPal = 0;
        }
      } else if (layer == Layer::EDIT) {
        // Brown-trim scale
        int v = (int)rt_curation_brown_green_scale + (rel * 2);
        rt_curation_brown_green_scale = clamp8(v);
        rt_enable_palette_curation = true;
#if FEATURE_DEBUG_OUTPUT
        Serial.print(F("[ENC][")); Serial.print(layerName(layer)); Serial.print(F("] Palette Brown Scale -> "));
        Serial.println(rt_curation_brown_green_scale);
#endif
      } else {
        // LFO depth 0..255
        int v = (int)lfoDepth + (rel * 8);
        lfoDepth = clamp8(v);
#if FEATURE_DEBUG_OUTPUT
        Serial.print(F("[ENC][PLAY] LFO Depth -> "));
        Serial.println(lfoDepth);
#endif
      }
      break;
    }
    case 2: { // Effect step via accumulator
      if (layer == Layer::NAV) {
        accFx += rel;
        const int step = 2;
        if (accFx >= step) {
#if FEATURE_DEBUG_OUTPUT
          uint8_t total = fxEngine.getNumEffects();
          if (total) {
            uint8_t next = (fxEngine.getCurrentEffectIndex() + 1) % total;
            Serial.print(F("[ENC][NAV] Next Effect -> "));
            Serial.println(fxEngine.getEffectName(next));
          }
#endif
          fxEngine.nextEffect(rt_transition_type, 800);
          CONFIG.LIGHTSHOW_MODE = fxEngine.getCurrentEffectIndex();
          accFx = 0;
        }
        else if (accFx <= -step) {
#if FEATURE_DEBUG_OUTPUT
          uint8_t total = fxEngine.getNumEffects();
          if (total) {
            uint8_t current = fxEngine.getCurrentEffectIndex();
            uint8_t prev = (current == 0) ? total - 1 : current - 1;
            Serial.print(F("[ENC][NAV] Prev Effect -> "));
            Serial.println(fxEngine.getEffectName(prev));
          }
#endif
          fxEngine.prevEffect(rt_transition_type, 800);
          CONFIG.LIGHTSHOW_MODE = fxEngine.getCurrentEffectIndex();
          accFx = 0;
        }
      } else if (layer == Layer::EDIT) {
        // Vignette strength
        int v = (int)rt_vignette_strength + (rel * 4);
        rt_vignette_strength = clamp8(v);
#if FEATURE_DEBUG_OUTPUT
        Serial.print(F("[ENC][EDIT] Vignette Strength -> "));
        Serial.println(rt_vignette_strength);
#endif
      } else {
        // LFO target select
        int v = (int)lfoTarget + rel;
        if (v < 0) v = 0; if (v > 4) v = 4;
        lfoTarget = (uint8_t)v;
        // capture base on target switch
        baseFade = fadeAmount;
        baseSpeed = paletteSpeed;
        baseBrightness = brightnessVal;
        baseVignette = rt_vignette_strength;
        baseBlur = rt_blur2d_amount;
#if FEATURE_DEBUG_OUTPUT
        Serial.print(F("[ENC][PLAY] LFO Target -> "));
        Serial.println(lfoTarget);
#endif
      }
      break;
    }
    case 3: { // Toggle FastLED dithering on click (handled in update via button), rotation: no-op
      if (layer == Layer::EDIT) {
        // Rotate as no-op in EDIT
      } else if (layer == Layer::PLAY) {
        // Rotate no-op; press toggles LFO in update()
      }
      break;
    }
    case 4: { // fadeAmount 0..255
      if (layer == Layer::NAV) {
        int v = (int)fadeAmount + (rel * 2);
        fadeAmount = clamp8(v);
#if FEATURE_DEBUG_OUTPUT
        Serial.print(F("[ENC][NAV] Fade Amount -> "));
        Serial.println(fadeAmount);
#endif
      } else if (layer == Layer::EDIT) {
        // Blur amount
        int v = (int)rt_blur2d_amount + (rel * 2);
        rt_blur2d_amount = clamp8(v);
        rt_enable_blur2d = true;
#if FEATURE_DEBUG_OUTPUT
        Serial.print(F("[ENC][EDIT] Blur Amount -> "));
        Serial.println(rt_blur2d_amount);
#endif
      } else {
        // PixelSpork gradient length override (0 = auto)
        int32_t len = (int32_t)rt_spork_grad_len_override;
        if (len == 0) {
          len = HardwareConfig::NUM_LEDS;
        }
        len += rel;
        if (len < 0) len = 0;
        if (len > HardwareConfig::NUM_LEDS) len = HardwareConfig::NUM_LEDS;
        rt_spork_grad_len_override = (uint16_t)len;
#if FEATURE_DEBUG_OUTPUT
        Serial.print(F("[ENC][PLAY] Spork Grad Len -> "));
        Serial.println(rt_spork_grad_len_override);
#endif
      }
      break;
    }
    case 5: { // paletteSpeed 1..50
      if (dwellAdjust) {
        // Adjust palette dwell time in 500ms steps, clamp 1000..60000ms
        int v = (int)rt_palette_dwell_ms + (rel * 500);
        if (v < 1000) v = 1000; if (v > 60000) v = 60000;
        rt_palette_dwell_ms = (uint32_t)v;
#if FEATURE_DEBUG_OUTPUT
        Serial.print(F("[ENC] Palette dwell -> "));
        Serial.println(rt_palette_dwell_ms);
#endif
      } else if (layer == Layer::NAV) {
        int v = (int)paletteSpeed + rel;
        if (v < 1) v = 1; if (v > 50) v = 50;
        paletteSpeed = (uint8_t)v;
#if FEATURE_DEBUG_OUTPUT
        Serial.print(F("[ENC][NAV] Palette Speed -> "));
        Serial.println(paletteSpeed);
#endif
      } else if (layer == Layer::EDIT) {
        // AE target luma 0..255
        int v = (int)rt_ae_target_luma + (rel * 2);
        rt_ae_target_luma = clamp8(v);
#if FEATURE_DEBUG_OUTPUT
        Serial.print(F("[ENC][EDIT] AE Target -> "));
        Serial.println(rt_ae_target_luma);
#endif
      } else {
        // PixelSpork gradient offset rate (0 = auto)
        int32_t rate = (int32_t)rt_spork_offset_rate_ms;
        if (rate == 0) rate = 200;
        rate += rel * 10;
        if (rate < 0) rate = 0;
        if (rate > 5000) rate = 5000;
        rt_spork_offset_rate_ms = (uint16_t)rate;
#if FEATURE_DEBUG_OUTPUT
        Serial.print(F("[ENC][PLAY] Spork Offset Rate -> "));
        Serial.println(rt_spork_offset_rate_ms);
#endif
      }
      break;
    }
    case 6: { // fps 10..120
      if (layer == Layer::NAV) {
        int v = (int)fps + (rel * 5);
        if (v < 10) v = 10; if (v > 120) v = 120;
        fps = (uint16_t)v;
#if FEATURE_DEBUG_OUTPUT
        Serial.print(F("[ENC][NAV] FPS -> "));
        Serial.println(fps);
#endif
      } else if (layer == Layer::EDIT) {
        // Gamma 1.0..3.0 in steps
        float delta = rel * 0.05f;
        rt_gamma_value += delta;
        if (rt_gamma_value < 1.0f) rt_gamma_value = 1.0f;
        if (rt_gamma_value > 3.0f) rt_gamma_value = 3.0f;
        rt_apply_gamma = true;
#if FEATURE_DEBUG_OUTPUT
        Serial.print(F("[ENC][EDIT] Gamma -> "));
        Serial.println(rt_gamma_value, 2);
#endif
      } else if (layer == Layer::PLAY) {
        int v = (int)rt_spork_offset_step + rel;
        if (v < 1) v = 1;
        if (v > 16) v = 16;
        rt_spork_offset_step = (uint8_t)v;
#if FEATURE_DEBUG_OUTPUT
        Serial.print(F("[ENC][PLAY] Spork Offset Step -> "));
        Serial.println(rt_spork_offset_step);
#endif
      }
      break;
    }
    case 7: { // Scene slot select (0..7)
      int v = (int)selectedScene + rel;
      if (v < 0) v = 0; if (v > 7) v = 7;
      selectedScene = (uint8_t)v;
      break;
    }
  }
}

void EncoderManager::update(uint32_t now_ms) {
  if (!available) return;

  // Throttled relative counts
  if (now_ms >= nextPollAt) {
    nextPollAt = now_ms + 20; // poll at ~50Hz
    for (uint8_t ch = 0; ch < 8; ++ch) {
      int32_t rel = enc.getRelCounter(ch);
      if (rel > 40 || rel < -40) rel = 0;
      if (rel != 0) applyRelative(ch, rel);
    }
  }

  // Button CH3: NAV short=dither; EDIT short=toggle vignette; PLAY short=toggle LFO. Long press cycles layer.
  static bool lastBtn3 = false;
  static uint32_t btn3DownAt = 0;
  bool btn3 = enc.getKeyPressed(3);
  if (btn3 && !lastBtn3) { btn3DownAt = now_ms; }
  if (!btn3 && lastBtn3) {
    uint32_t held = now_ms - btn3DownAt;
    if (held >= 800) {
      // Cycle layer
      layer = static_cast<Layer>((static_cast<uint8_t>(layer) + 1) % 3);
      lastActivity = now_ms;
#if FEATURE_DEBUG_OUTPUT
      Serial.print(F("[ENC] Mode changed -> "));
      Serial.println(layerLabel(layer));
#endif
    } else {
      if (layer == Layer::NAV) {
        static bool dither = false;
        dither = !dither;
        FastLED.setDither(dither ? 1 : 0);
#if FEATURE_DEBUG_OUTPUT
        Serial.print(F("[ENC][NAV] Dither -> "));
        Serial.println(dither ? F("ON") : F("OFF"));
#endif
      } else if (layer == Layer::EDIT) {
        rt_enable_vignette = !rt_enable_vignette;
#if FEATURE_DEBUG_OUTPUT
        Serial.print(F("[ENC][EDIT] Vignette -> "));
        Serial.println(rt_enable_vignette ? F("ON") : F("OFF"));
#endif
      } else {
        lfoEnabled = !lfoEnabled;
        // capture bases when enabling
        if (lfoEnabled) {
          baseFade = fadeAmount;
          baseSpeed = paletteSpeed;
          baseBrightness = brightnessVal;
          baseVignette = rt_vignette_strength;
          baseBlur = rt_blur2d_amount;
          lfoLastMs = now_ms;
        }
#if FEATURE_DEBUG_OUTPUT
        Serial.print(F("[ENC][PLAY] LFO -> "));
        Serial.println(lfoEnabled ? F("ON") : F("OFF"));
#endif
      }
      lastActivity = now_ms;
    }
  }
  lastBtn3 = btn3;

  // Button CH1: toggle auto palette cycle per layer
  static bool lastBtn1 = false;
  bool btn1 = enc.getKeyPressed(1);
  if (btn1 && !lastBtn1) { /* down */ }
  if (!btn1 && lastBtn1) {
    switch (layer) {
      case Layer::NAV:  rt_autocycle_nav  = !rt_autocycle_nav;  break;
      case Layer::EDIT: rt_autocycle_edit = !rt_autocycle_edit; break;
      case Layer::PLAY: rt_autocycle_play = !rt_autocycle_play; break;
    }
    // brief visual cue on CH1 LED: green when on, red when off
    bool on = (layer == Layer::NAV) ? rt_autocycle_nav : (layer == Layer::EDIT ? rt_autocycle_edit : rt_autocycle_play);
    enc.writeRGB(1, on ? 0 : 32, on ? 48 : 0, 0);
    lastActivity = now_ms;
  }
  lastBtn1 = btn1;

  // Button CH5: long-press toggles Dwell Adjust mode (rotate CH5 to change rt_palette_dwell_ms)
  static bool lastBtn5 = false;
  static uint32_t btn5DownAt = 0;
  bool btn5 = enc.getKeyPressed(5);
  if (btn5 && !lastBtn5) { btn5DownAt = now_ms; }
  if (!btn5 && lastBtn5) {
    uint32_t held = now_ms - btn5DownAt;
    if (held >= 800) {
      dwellAdjust = !dwellAdjust;
      // Orange LED indicates dwell adjust mode
      enc.writeRGB(5, dwellAdjust ? 48 : 0, dwellAdjust ? 32 : 0, 0);
      lastActivity = now_ms;
    } else {
      // Short press: toggle Auto-Exposure
      rt_enable_auto_exposure = !rt_enable_auto_exposure;
      // Green if ON, red if OFF
      enc.writeRGB(5, rt_enable_auto_exposure ? 0 : 32, rt_enable_auto_exposure ? 48 : 0, 0);
      lastActivity = now_ms;
    }
  }
  lastBtn5 = btn5;

  // Button CH6: short press toggles Brown Guardrail
  static bool lastBtn6 = false;
  static uint32_t btn6DownAt = 0;
  bool btn6 = enc.getKeyPressed(6);
  if (btn6 && !lastBtn6) { btn6DownAt = now_ms; }
  if (!btn6 && lastBtn6) {
    uint32_t held = now_ms - btn6DownAt;
    if (held < 800) {
      rt_enable_brown_guardrail = !rt_enable_brown_guardrail;
      enc.writeRGB(6, rt_enable_brown_guardrail ? 0 : 32, rt_enable_brown_guardrail ? 48 : 0, 0);
      lastActivity = now_ms;
    }
  }
  lastBtn6 = btn6;

  // Button CH2: short press cycles transition type (0..2)
  static bool lastBtn2 = false;
  bool btn2 = enc.getKeyPressed(2);
  if (!btn2 && lastBtn2) {
    rt_transition_type = (rt_transition_type + 1) % 3;
    // Brief cue on CH2: blueish flash
    enc.writeRGB(2, 0, 0, 64);
    lastActivity = now_ms;
  }
  lastBtn2 = btn2;

  // Button CH7: short press recall scene, long press save
  static bool lastBtn7 = false;
  static uint32_t btn7DownAt = 0;
  bool btn7 = enc.getKeyPressed(7);
  if (btn7 && !lastBtn7) { btn7DownAt = now_ms; }
  if (!btn7 && lastBtn7) {
    uint32_t held = now_ms - btn7DownAt;
    static ScenesManager scenes;
    static bool initialized = false;
    if (!initialized) { scenes.begin(); initialized = true; }
    scenes.setSelected(selectedScene);
    if (held >= 800) {
      scenes.capture(selectedScene);
      scenes.saveToFS();
      enc.writeRGB(7, 0, 64, 0); // green blink on save
    } else {
      scenes.apply(selectedScene, true);
      enc.writeRGB(7, 0, 32, 32); // cyan blink on recall
    }
    lastActivity = now_ms;
  }
  lastBtn7 = btn7;

  // LFO tick (PLAY layer)
  tickLFO(now_ms);

  // Recovery: if device went missing, rescan occasionally
  if (!available && now_ms >= nextScanAt) {
    uint8_t addr;
    if (scanBus(addr) && configureAtAddress(addr)) {
      Serial.print(F("[ENC] Recovered M5ROTATE8 at 0x")); Serial.println(addr, HEX);
      available = true;
      enc.setAll(0, 0, 4);
    } else {
      nextScanAt = now_ms + 5000;
    }
  }

  updateLEDs(now_ms);
}

void EncoderManager::updateLEDs(uint32_t now_ms) {
  if (!available) return;

  const uint32_t activeTimeout = 2000;
  uint8_t active = (now_ms - lastActivity < activeTimeout) ? lastActive : 255;

  // Choose idle color per layer
  static uint32_t lastLedWrite = 0;
  if (now_ms < nextLedUpdateAt) return;
  nextLedUpdateAt = now_ms + 120; // ~8Hz
  uint8_t idleR=0, idleG=0, idleB=6;
  switch (layer) {
    case Layer::NAV:  idleR = 0;  idleG = 32; idleB = 32; break; // cyan
    case Layer::EDIT: idleR = 32; idleG = 0;  idleB = 32; break; // magenta
    case Layer::PLAY: idleR = 32; idleG = 24; idleB = 0;  break; // amber
  }
  for (uint8_t i = 0; i < 8; ++i) {
    uint8_t r = idleR, g = idleG, b = idleB;
    if (i == active) {
      r = 0; g = 64; b = 0;
    } else if (i == 6) {
      if (rt_enable_brown_guardrail) {
        r = 0; g = 60; b = 0;
      } else {
        r = 48; g = 0; b = 0;
      }
    }
    if (!enc.writeRGB(i, r, g, b)) { available = false; return; }
  }
}

void EncoderManager::tickLFO(uint32_t now_ms) {
  if (!lfoEnabled) return;
  if (lfoLastMs == 0) lfoLastMs = now_ms;
  uint32_t dt = now_ms - lfoLastMs;
  lfoLastMs = now_ms;
  // advance phase (16.16 fixed)
  float step = lfoRateHz * 65536.0f * (float)dt / 1000.0f;
  lfoPhase += (uint32_t)step;
  uint8_t s = sin8((uint8_t)(lfoPhase >> 8));
  int16_t centered = (int16_t)s - 128; // -128..127
  int16_t offset = ((int16_t)lfoDepth * centered) / 128;

  switch (lfoTarget) {
    case 0: { // fadeAmount 0..255
      int v = (int)baseFade + offset;
      fadeAmount = clamp8(v);
      break; }
    case 1: { // paletteSpeed 1..50 with reduced sensitivity
      int v = (int)baseSpeed + (offset / 16);
      if (v < 1) v = 1; if (v > 50) v = 50;
      paletteSpeed = (uint8_t)v;
      break; }
    case 2: { // brightness 0..255
      int v = (int)baseBrightness + offset;
      brightnessVal = clamp8(v);
      FastLED.setBrightness(brightnessVal);
      break; }
    case 3: { // vignette strength 0..255
      int v = (int)baseVignette + offset;
      rt_vignette_strength = clamp8(v);
      rt_enable_vignette = true;
      break; }
    case 4: { // blur amount 0..255
      int v = (int)baseBlur + offset;
      rt_blur2d_amount = clamp8(v);
      rt_enable_blur2d = true;
      break; }
  }
}

bool EncoderManager::scanBus(uint8_t& foundAddr) {
  foundAddr = 0;
  Serial.println(F("[ENC] Scanning I2C bus..."));
  uint8_t preferred40 = 0, preferred41 = 0, first = 0;
  for (uint8_t addr = 8; addr < 0x78; ++addr) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      Serial.print(F("  - Found 0x")); Serial.println(addr, HEX);
      if (!first) first = addr;
      if (addr == 0x40) preferred40 = addr;
      if (addr == 0x41) preferred41 = addr;
    }
  }
  if (preferred41) { foundAddr = preferred41; return true; }
  if (preferred40) { foundAddr = preferred40; return true; }
  if (first)       { foundAddr = first;       return true; }
  return false;
}

bool EncoderManager::configureAtAddress(uint8_t addr) {
  if (!addr) return false;
  enc.setAddress(addr);
  // Light sanity check: try a benign read & write
  (void)enc.getRelCounter(0);
  bool ok = enc.writeRGB(0, 0, 0, 0);
  if (!ok) return false;
  // Version register (best effort)
  uint8_t clr = enc.inputSwitch();
  (void)clr;
  return true;
}

#endif // FEATURE_ENCODERS

#ifndef FX_ENGINE_H
#define FX_ENGINE_H

#include <FastLED.h>
#include "../config/hardware_config.h"
#include "../effects/EffectBase.h"

// Forward declarations - these are defined elsewhere
extern CRGB leds[HardwareConfig::NUM_LEDS];
extern uint8_t angles[HardwareConfig::NUM_LEDS];
extern uint8_t radii[HardwareConfig::NUM_LEDS];
extern CRGBPalette16 currentPalette;
extern uint8_t currentPaletteIndex;
extern uint8_t fadeAmount;
extern uint8_t paletteSpeed;
extern uint8_t brightnessVal;

// Effect function pointer type
typedef void (*EffectFunction)();

// Effect descriptor structure
struct EffectDescriptor {
  const char* name = "";
  EffectFunction legacyFunction = nullptr;
  EffectBase* objectInstance = nullptr;
  bool usesObject = false;
  uint8_t defaultBrightness = 128;
  uint8_t defaultSpeed = 10;
  uint8_t defaultFade = 20;
};

// FxEngine class for professional effect management
class FxEngine {
private:
  static constexpr uint8_t MAX_EFFECTS = HardwareConfig::MAX_EFFECTS;
  
  EffectDescriptor effects[MAX_EFFECTS];
  bool effectActive[MAX_EFFECTS] = {false};
  uint8_t currentEffectIndex = 0;
  uint8_t numEffects = 0;
  
  // Transition state
  bool isTransitioning = false;
  uint32_t transitionStartTime = 0;
  uint16_t transitionDuration = 1000; // ms
  uint8_t transitionType = 0; // 0=fade, 1=wipe, 2=blend
  uint8_t nextEffectIndex = 0;
  uint8_t previousEffectIndex = 0;
  
  // Performance tracking
  uint32_t lastFrameTime = 0;
  uint32_t frameCount = 0;
  float averageFrameTime = 0.0f;
  
  // Backup buffer for transitions
  CRGB* transitionBuffer;
  
public:
  FxEngine() {
    // Allocate transition buffer
    transitionBuffer = new CRGB[HardwareConfig::NUM_LEDS];
    memset(transitionBuffer, 0, sizeof(CRGB) * HardwareConfig::NUM_LEDS);
    memset(effectActive, 0, sizeof(effectActive));
  }
  
  ~FxEngine() {
    // Clean up transition buffer
    delete[] transitionBuffer;

    for (uint8_t i = 0; i < numEffects; ++i) {
      if (effects[i].usesObject && effects[i].objectInstance) {
        if (effectActive[i]) {
          effects[i].objectInstance->cleanup();
          effectActive[i] = false;
        }
        delete effects[i].objectInstance;
        effects[i].objectInstance = nullptr;
      }
    }
  }
  
  // Register an effect with the engine
  bool addEffect(const char* name, EffectFunction function, 
                 uint8_t brightness = 128, uint8_t speed = 10, uint8_t fade = 20) {
    if (numEffects >= MAX_EFFECTS) return false;
    
    EffectDescriptor descriptor;
    descriptor.name = name;
    descriptor.legacyFunction = function;
    descriptor.usesObject = false;
    descriptor.defaultBrightness = brightness;
    descriptor.defaultSpeed = speed;
    descriptor.defaultFade = fade;
    effects[numEffects] = descriptor;
    effectActive[numEffects] = false;
    numEffects++;
    return true;
  }

  bool addEffect(EffectBase* effect) {
    if (!effect || numEffects >= MAX_EFFECTS) return false;

    EffectDescriptor descriptor;
    descriptor.name = effect->getName();
    descriptor.objectInstance = effect;
    descriptor.usesObject = true;
    descriptor.defaultBrightness = effect->getDefaultBrightness();
    descriptor.defaultSpeed = effect->getDefaultSpeed();
    descriptor.defaultFade = effect->getDefaultFade();
    effects[numEffects] = descriptor;
    effectActive[numEffects] = false;
    numEffects++;
    return true;
  }
  
  // Get current effect info
  const char* getCurrentEffectName() const {
    if (numEffects == 0) return "None";
    return effects[currentEffectIndex].name;
  }

  uint8_t getCurrentEffectIndex() const { return currentEffectIndex; }
  uint8_t getNumEffects() const { return numEffects; }

  const char* getEffectName(uint8_t index) const {
    if (index >= numEffects) return "";
    return effects[index].name ? effects[index].name : "";
  }
  
  // Effect switching with transitions
  void setEffect(uint8_t index, uint8_t transitionType = 0, uint16_t duration = 1000) {
    if (index >= numEffects) return;
    
    if (index != currentEffectIndex && !isTransitioning) {
      // Start transition
      nextEffectIndex = index;
      previousEffectIndex = currentEffectIndex;
      this->transitionType = transitionType;
      transitionDuration = duration;
      isTransitioning = true;
      transitionStartTime = millis();

      initEffect(nextEffectIndex);

      // Store current state in transition buffer
      memcpy(transitionBuffer, leds, sizeof(CRGB) * HardwareConfig::NUM_LEDS);
    }
  }
  
  void nextEffect(uint8_t transitionType = 0, uint16_t duration = 1000) {
    if (numEffects == 0) return;
    setEffect((currentEffectIndex + 1) % numEffects, transitionType, duration);
  }
  
  void prevEffect(uint8_t transitionType = 0, uint16_t duration = 1000) {
    if (numEffects == 0) return;
    uint8_t prev = (currentEffectIndex == 0) ? numEffects - 1 : currentEffectIndex - 1;
    setEffect(prev, transitionType, duration);
  }
  
  // Main render loop - call this from main loop
  void render() {
    if (numEffects == 0) return;
    
    uint32_t now = millis();
    
    // Performance tracking
    if (lastFrameTime > 0) {
      uint32_t frameTime = now - lastFrameTime;
      averageFrameTime = (averageFrameTime * 0.9f) + (frameTime * 0.1f);
      frameCount++;
    }
    lastFrameTime = now;

    initEffect(currentEffectIndex);

    if (isTransitioning) {
      renderTransition(now);
    } else {
      // Render current effect
      renderEffect(currentEffectIndex);
    }
  }
  
  // Get performance metrics
  float getAverageFrameTime() const { return averageFrameTime; }
  float getApproximateFPS() const { 
    return averageFrameTime > 0 ? 1000.0f / averageFrameTime : 0.0f; 
  }
  uint32_t getFrameCount() const { return frameCount; }
  
  // Transition status
  bool getIsTransitioning() const { return isTransitioning; }
  float getTransitionProgress() const {
    if (!isTransitioning) return 0.0f;
    uint32_t elapsed = millis() - transitionStartTime;
    return constrain(float(elapsed) / float(transitionDuration), 0.0f, 1.0f);
  }
  
private:
  void renderTransition(uint32_t now) {
    uint32_t elapsed = now - transitionStartTime;
    float progress = (transitionDuration == 0)
                         ? 1.0f
                         : float(elapsed) / float(transitionDuration);

    if (progress >= 1.0f) {
      // Transition complete
      isTransitioning = false;
      cleanupEffect(previousEffectIndex);
      currentEffectIndex = nextEffectIndex;
      progress = 1.0f;
    }

    // Render next effect to main buffer
    renderEffect(nextEffectIndex);
    
    // Apply transition
    switch (transitionType) {
      case 0: // Fade transition
        fadeTransition(progress);
        break;
      case 1: // Wipe transition
        wipeTransition(progress);
        break;
      case 2: // Blend transition
        blendTransition(progress);
        break;
    }
  }
  
  void fadeTransition(float progress) {
    uint8_t newBrightness = 255 * progress;
    uint8_t oldBrightness = 255 * (1.0f - progress);
    
    for (uint16_t i = 0; i < HardwareConfig::NUM_LEDS; i++) {
      CRGB oldColor = transitionBuffer[i];
      CRGB newColor = leds[i];
      
      oldColor.fadeToBlackBy(255 - oldBrightness);
      newColor.fadeToBlackBy(255 - newBrightness);
      
      leds[i] = oldColor + newColor;
    }
  }
  
  void wipeTransition(float progress) {
    uint16_t wipePosition = HardwareConfig::NUM_LEDS * progress;
    
    for (uint16_t i = 0; i < HardwareConfig::NUM_LEDS; i++) {
      if (i < wipePosition) {
        // Use new effect
        // leds[i] already contains new effect
      } else {
        // Use old effect
        leds[i] = transitionBuffer[i];
      }
    }
  }
  
  void blendTransition(float progress) {
    uint8_t blendAmount = 255 * progress;
    
    for (uint16_t i = 0; i < HardwareConfig::NUM_LEDS; i++) {
      leds[i] = blend(transitionBuffer[i], leds[i], blendAmount);
    }
  }

  void renderEffect(uint8_t index) {
    if (index >= numEffects) return;
    EffectDescriptor& descriptor = effects[index];
    if (descriptor.usesObject) {
      if (descriptor.objectInstance) {
        descriptor.objectInstance->render();
      }
    } else if (descriptor.legacyFunction) {
      descriptor.legacyFunction();
    }
  }

  void initEffect(uint8_t index) {
    if (index >= numEffects) return;
    EffectDescriptor& descriptor = effects[index];
    if (!descriptor.usesObject || effectActive[index]) return;
    if (descriptor.objectInstance) {
      descriptor.objectInstance->init();
      effectActive[index] = true;
    }
  }

  void cleanupEffect(uint8_t index) {
    if (index >= numEffects) return;
    EffectDescriptor& descriptor = effects[index];
    if (!descriptor.usesObject || !effectActive[index]) return;
    if (descriptor.objectInstance) {
      descriptor.objectInstance->cleanup();
      effectActive[index] = false;
    }
  }
};


#endif // FX_ENGINE_H

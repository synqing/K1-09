#pragma once
#include <Arduino.h>
#include <FastLED.h>

class PerformanceMetrics {
private:
    uint32_t frameStartTime = 0;
    uint32_t frameEndTime = 0;
    uint32_t frameCount = 0;
    uint32_t totalFrameTime = 0;
    uint32_t maxFrameTime = 0;
    uint32_t minFrameTime = UINT32_MAX;
    uint32_t lastReportTime = 0;
    uint32_t effectStartTime = 0;
    uint32_t effectTotalTime = 0;
    
    // Per-effect metrics
    struct EffectMetrics {
        uint32_t totalTime = 0;
        uint32_t callCount = 0;
        uint32_t maxTime = 0;
        uint32_t minTime = UINT32_MAX;
    };
    
    EffectMetrics effectMetrics[20];  // Support up to 20 effects
    uint8_t currentEffectIndex = 0;
    
public:
    void beginFrame() {
        frameStartTime = micros();
    }
    
    void endFrame() {
        frameEndTime = micros();
        uint32_t frameTime = frameEndTime - frameStartTime;
        
        totalFrameTime += frameTime;
        frameCount++;
        
        if (frameTime > maxFrameTime) maxFrameTime = frameTime;
        if (frameTime < minFrameTime) minFrameTime = frameTime;
    }
    
    void beginEffect(uint8_t effectIndex) {
        currentEffectIndex = effectIndex;
        effectStartTime = micros();
    }
    
    void endEffect() {
        uint32_t effectTime = micros() - effectStartTime;
        
        if (currentEffectIndex < 20) {
            EffectMetrics& metrics = effectMetrics[currentEffectIndex];
            metrics.totalTime += effectTime;
            metrics.callCount++;
            if (effectTime > metrics.maxTime) metrics.maxTime = effectTime;
            if (effectTime < metrics.minTime) metrics.minTime = effectTime;
        }
    }
    
    void report(const char* effectNames[], uint8_t numEffects) {
        uint32_t now = millis();
        if (now - lastReportTime < 5000) return;  // Report every 5 seconds
        
        lastReportTime = now;
        
        if (frameCount == 0) return;
        
        // Calculate averages
        float avgFrameTime = (float)totalFrameTime / frameCount;
        float fps = 1000000.0f / avgFrameTime;
        
        Serial.println("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        Serial.println("                    PERFORMANCE METRICS REPORT");
        Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        
        Serial.printf("📊 Frame Statistics:\n");
        Serial.printf("   • Average FPS: %.1f\n", fps);
        Serial.printf("   • Frame Time: %.1fµs avg (%.1fµs min, %.1fµs max)\n", 
                     avgFrameTime, (float)minFrameTime, (float)maxFrameTime);
        Serial.printf("   • Total Frames: %u\n", frameCount);
        
        Serial.println("\n📈 Effect Performance:");
        Serial.println("┌─────────────────────┬──────────┬──────────┬──────────┬──────────┐");
        Serial.println("│ Effect              │ Avg (µs) │ Min (µs) │ Max (µs) │ Calls    │");
        Serial.println("├─────────────────────┼──────────┼──────────┼──────────┼──────────┤");
        
        for (uint8_t i = 0; i < numEffects && i < 20; i++) {
            EffectMetrics& metrics = effectMetrics[i];
            if (metrics.callCount > 0) {
                float avgTime = (float)metrics.totalTime / metrics.callCount;
                Serial.printf("│ %-19s │ %8.1f │ %8u │ %8u │ %8u │\n",
                             effectNames[i], avgTime, metrics.minTime, metrics.maxTime, metrics.callCount);
            }
        }
        
        Serial.println("└─────────────────────┴──────────┴──────────┴──────────┴──────────┘");
        
        // FastLED specific metrics
        Serial.println("\n🚀 FastLED Performance:");
        Serial.printf("   • Brightness: %d\n", FastLED.getBrightness());
        
        Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
        
        // Reset counters for next period
        frameCount = 0;
        totalFrameTime = 0;
        maxFrameTime = 0;
        minFrameTime = UINT32_MAX;
        
        // Reset effect metrics
        for (uint8_t i = 0; i < 20; i++) {
            effectMetrics[i] = EffectMetrics();
        }
    }
    
    float getCurrentFPS() {
        if (frameCount == 0) return 0;
        float avgFrameTime = (float)totalFrameTime / frameCount;
        return 1000000.0f / avgFrameTime;
    }
};
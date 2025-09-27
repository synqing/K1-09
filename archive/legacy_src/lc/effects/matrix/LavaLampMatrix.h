#ifndef LAVA_LAMP_MATRIX_H
#define LAVA_LAMP_MATRIX_H

#include <Arduino.h>
#include <FastLED.h>
#include "../../utils/FastLEDLUTs.h"

// Lava Lamp Matrix - Metaballs with heat-based coloring using lava palette
class LavaLampMatrix {
private:
    static constexpr uint8_t MAX_BLOBS = 5;
    static constexpr uint8_t GRID_SIZE = 9;
    static constexpr uint8_t NUM_LEDS = 81;
    static constexpr uint16_t FIXED_SCALE = 256;  // 8.8 fixed-point
    
    // Blob structure for metaball
    struct Blob {
        uint16_t x, y;         // Position in 8.8 fixed-point
        uint16_t radius;       // Size in 8.8 fixed-point
        int16_t vx, vy;        // Velocity
        uint8_t temperature;   // Heat level (affects color)
        uint8_t pulsePhase;    // For size pulsing
    };
    
    Blob blobs[MAX_BLOBS];
    CRGB* leds;
    CRGBPalette16* currentPalette;
    
    // Heat map for the entire grid (temperature field)
    uint8_t heatMap[NUM_LEDS];
    
    // Convert grid coordinates to LED index
    uint8_t gridToIndex(uint8_t x, uint8_t y) {
        if (x >= GRID_SIZE || y >= GRID_SIZE) return 0;
        return y * GRID_SIZE + x;
    }
    
    // Calculate metaball field strength at a point
    uint16_t calculateField(uint16_t px, uint16_t py) {
        uint32_t totalField = 0;
        
        for (uint8_t b = 0; b < MAX_BLOBS; b++) {
            // Calculate distance to blob center
            int32_t dx = (int32_t)px - blobs[b].x;
            int32_t dy = (int32_t)py - blobs[b].y;
            
            // Squared distance (avoid sqrt for performance)
            uint32_t distSq = (dx * dx + dy * dy) >> 8;  // Scale back from fixed-point
            
            if (distSq > 0) {
                // Metaball equation: field = radius^2 / distance^2
                uint32_t radiusSq = (blobs[b].radius * blobs[b].radius) >> 8;
                uint32_t field = 0;
                
                if (distSq < radiusSq * 4) {  // Only calculate within influence radius
                    field = (radiusSq * 256) / (distSq + 1);
                    
                    // Apply temperature weighting
                    field = (field * blobs[b].temperature) >> 8;
                    
                    // Limit maximum contribution
                    if (field > 1024) field = 1024;
                }
                
                totalField += field;
            }
        }
        
        return min(totalField, 65535);
    }
    
public:
    LavaLampMatrix(CRGB* ledArray, CRGBPalette16* palette) : 
        leds(ledArray), currentPalette(palette) {
        
        // Initialize blobs with random positions and velocities
        for (uint8_t i = 0; i < MAX_BLOBS; i++) {
            blobs[i].x = random16(1 * FIXED_SCALE, 8 * FIXED_SCALE);
            blobs[i].y = random16(1 * FIXED_SCALE, 8 * FIXED_SCALE);
            blobs[i].radius = random16(FIXED_SCALE, 2 * FIXED_SCALE);
            blobs[i].vx = random16(64) - 32;  // -0.125 to 0.125
            blobs[i].vy = random16(64) - 32;
            blobs[i].temperature = random8(128, 255);
            blobs[i].pulsePhase = random8();
        }
        
        // Clear heat map
        memset(heatMap, 0, NUM_LEDS);
    }
    
    void update(uint8_t speed, uint8_t viscosity) {
        // Update blob positions
        for (uint8_t b = 0; b < MAX_BLOBS; b++) {
            // Update position based on velocity
            blobs[b].x += (blobs[b].vx * speed) >> 4;
            blobs[b].y += (blobs[b].vy * speed) >> 4;
            
            // Bounce off walls with damping
            if (blobs[b].x < FIXED_SCALE || blobs[b].x > 8 * FIXED_SCALE) {
                blobs[b].vx = -blobs[b].vx;
                blobs[b].x = constrain(blobs[b].x, FIXED_SCALE, 8 * FIXED_SCALE);
            }
            if (blobs[b].y < FIXED_SCALE || blobs[b].y > 8 * FIXED_SCALE) {
                blobs[b].vy = -blobs[b].vy;
                blobs[b].y = constrain(blobs[b].y, FIXED_SCALE, 8 * FIXED_SCALE);
            }
            
            // Apply viscosity (velocity damping)
            uint8_t damping = 255 - (viscosity >> 2);
            blobs[b].vx = (blobs[b].vx * damping) >> 8;
            blobs[b].vy = (blobs[b].vy * damping) >> 8;
            
            // Random velocity changes (brownian motion)
            if (random8() < speed) {
                blobs[b].vx += random8(16) - 8;
                blobs[b].vy += random8(16) - 8;
            }
            
            // Pulse blob size
            blobs[b].pulsePhase += speed >> 1;
            uint8_t pulseMod = sin8(blobs[b].pulsePhase);
            blobs[b].radius = FIXED_SCALE + (pulseMod >> 2);
            
            // Temperature changes (cooling/heating)
            if (random8() < 20) {
                blobs[b].temperature += random8(40) - 20;
                blobs[b].temperature = constrain(blobs[b].temperature, 100, 255);
            }
            
            // Blob interaction - merge and split
            for (uint8_t other = b + 1; other < MAX_BLOBS; other++) {
                int32_t dx = (int32_t)blobs[b].x - blobs[other].x;
                int32_t dy = (int32_t)blobs[b].y - blobs[other].y;
                uint32_t distSq = (dx * dx + dy * dy) >> 8;
                
                uint16_t minDist = (blobs[b].radius + blobs[other].radius) >> 1;
                
                if (distSq < minDist * minDist / 256) {
                    // Blobs are overlapping - repel
                    if (distSq > 0) {
                        int16_t force = (minDist * minDist / 256 - distSq) >> 4;
                        blobs[b].vx += (dx * force) >> 12;
                        blobs[b].vy += (dy * force) >> 12;
                        blobs[other].vx -= (dx * force) >> 12;
                        blobs[other].vy -= (dy * force) >> 12;
                    }
                    
                    // Exchange some temperature
                    uint8_t avgTemp = (blobs[b].temperature + blobs[other].temperature) >> 1;
                    blobs[b].temperature = (blobs[b].temperature + avgTemp) >> 1;
                    blobs[other].temperature = (blobs[other].temperature + avgTemp) >> 1;
                }
            }
        }
        
        // Cool down heat map slightly
        for (uint16_t i = 0; i < NUM_LEDS; i++) {
            heatMap[i] = scale8(heatMap[i], 240);
        }
        
        // Render metaballs to heat map
        for (uint8_t y = 0; y < GRID_SIZE; y++) {
            for (uint8_t x = 0; x < GRID_SIZE; x++) {
                uint16_t px = x * FIXED_SCALE + FIXED_SCALE/2;
                uint16_t py = y * FIXED_SCALE + FIXED_SCALE/2;
                
                // Calculate field strength at this pixel
                uint16_t field = calculateField(px, py);
                
                // Threshold for blob edge (creates distinct blobs)
                uint8_t heat = 0;
                if (field > 200) {
                    // Inside blob - map field strength to heat
                    heat = field >> 8;
                    if (heat > 255) heat = 255;
                    
                    // Add some noise for texture
                    uint8_t noise = inoise8(x * 30, y * 30, millis() >> 2) >> 3;
                    heat = qadd8(heat, noise);
                }
                
                uint8_t idx = gridToIndex(x, y);
                heatMap[idx] = qadd8(heatMap[idx], heat);
            }
        }
        
        // Apply convection - heat rises
        for (uint8_t y = 0; y < GRID_SIZE - 1; y++) {
            for (uint8_t x = 0; x < GRID_SIZE; x++) {
                uint8_t idx = gridToIndex(x, y);
                uint8_t idxAbove = gridToIndex(x, y + 1);
                
                // Transfer some heat upward
                uint8_t transfer = heatMap[idx] >> 4;
                heatMap[idx] = qsub8(heatMap[idx], transfer);
                heatMap[idxAbove] = qadd8(heatMap[idxAbove], transfer);
            }
        }
        
        // Map heat to lava palette
        for (uint16_t i = 0; i < NUM_LEDS; i++) {
            // Use heat as direct index into lava palette
            // Low heat = black/dark red (molten rock)
            // High heat = yellow/white (liquid lava)
            leds[i] = ColorFromPalette(*currentPalette, heatMap[i]);
            
            // Add subtle glow for very hot areas
            if (heatMap[i] > 200) {
                leds[i] += CRGB(heatMap[i] >> 4, heatMap[i] >> 5, 0);
            }
        }
    }
    
    // Control functions
    void setViscosity(uint8_t v) {
        // Higher viscosity = thicker lava, slower movement
        for (uint8_t b = 0; b < MAX_BLOBS; b++) {
            uint8_t damping = 255 - v;
            blobs[b].vx = (blobs[b].vx * damping) >> 8;
            blobs[b].vy = (blobs[b].vy * damping) >> 8;
        }
    }
    
    void addHeat(uint8_t amount) {
        // Inject heat at random blob
        uint8_t b = random8(MAX_BLOBS);
        blobs[b].temperature = qadd8(blobs[b].temperature, amount);
    }
    
    void stir() {
        // Add random motion to all blobs
        for (uint8_t b = 0; b < MAX_BLOBS; b++) {
            blobs[b].vx += random8(32) - 16;
            blobs[b].vy += random8(32) - 16;
        }
    }
};

#endif // LAVA_LAMP_MATRIX_H
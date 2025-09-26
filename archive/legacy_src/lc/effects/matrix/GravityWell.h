#ifndef GRAVITY_WELL_H
#define GRAVITY_WELL_H

#include <Arduino.h>
#include <FastLED.h>

// Gravity Well Effect - Multiple gravity sources pull particles across the matrix
class GravityWell {
private:
    static constexpr uint8_t MAX_SOURCES = 3;
    static constexpr uint8_t MAX_PARTICLES = 20;
    static constexpr uint8_t GRID_SIZE = 9;
    static constexpr uint8_t NUM_LEDS = 81;
    
    // Gravity source structure
    struct GravitySource {
        uint8_t x, y;          // Position in 9x9 grid (0-8)
        int8_t strength;       // -128 to 127 (negative = repel)
        uint8_t hue;           // Base color
        uint16_t phase;        // For pulsing effect
        bool active;
    };
    
    // Particle structure
    struct Particle {
        uint16_t x, y;         // Position in 8.8 fixed-point (0-2304 = 0-9)
        int16_t vx, vy;        // Velocity in 8.8 fixed-point
        uint8_t life;          // Lifetime (255 = new, 0 = dead)
        uint8_t hue;           // Color inherited from nearest source
    };
    
    GravitySource sources[MAX_SOURCES];
    Particle particles[MAX_PARTICLES];
    CRGB* leds;
    
    // Convert grid coordinates to LED index
    uint8_t gridToIndex(uint8_t x, uint8_t y) {
        if (x >= GRID_SIZE || y >= GRID_SIZE) return 0;
        return y * GRID_SIZE + x;
    }
    
    // Get distance between two points (returns 0-255)
    uint8_t getDistance(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2) {
        int16_t dx = (int16_t)x1 - x2;
        int16_t dy = (int16_t)y1 - y2;
        uint16_t dist_squared = (dx * dx) + (dy * dy);
        // Max possible distance squared is 128 (8*8 + 8*8)
        // Scale to 0-255 range
        return sqrt16(dist_squared * 32);
    }
    
public:
    GravityWell(CRGB* ledArray) : leds(ledArray) {
        // Initialize gravity sources
        sources[0] = {2, 2, 80, 60, 0, true};    // Blue attractor
        sources[1] = {6, 6, -60, 20, 21845, true}; // Red repulsor
        sources[2] = {4, 7, 50, 140, 43690, true}; // Purple attractor
        
        // Initialize particles randomly
        for (uint8_t i = 0; i < MAX_PARTICLES; i++) {
            respawnParticle(i);
        }
    }
    
    void respawnParticle(uint8_t index) {
        particles[index].x = random16(GRID_SIZE * 256);
        particles[index].y = random16(GRID_SIZE * 256);
        particles[index].vx = random16(512) - 256;  // -1 to 1 in fixed-point
        particles[index].vy = random16(512) - 256;
        particles[index].life = 255;
        particles[index].hue = 160; // Default cyan
    }
    
    void update(uint8_t speed, uint8_t fadeRate) {
        // Fade existing pixels
        fadeToBlackBy(leds, NUM_LEDS, fadeRate);
        
        // Update gravity source phases for pulsing
        for (uint8_t s = 0; s < MAX_SOURCES; s++) {
            if (sources[s].active) {
                sources[s].phase += speed * 100;
                
                // Draw gravity source core
                uint8_t coreIndex = gridToIndex(sources[s].x, sources[s].y);
                uint8_t pulse = sin8(sources[s].phase >> 8);
                uint8_t brightness = 128 + (pulse >> 1);
                
                if (sources[s].strength > 0) {
                    // Attractor - cool colors
                    leds[coreIndex] = CHSV(sources[s].hue, 255, brightness);
                } else {
                    // Repulsor - warm colors with flicker
                    uint8_t flicker = random8(20);
                    leds[coreIndex] = CHSV(sources[s].hue, 200, brightness + flicker);
                }
            }
        }
        
        // Update particles
        for (uint8_t i = 0; i < MAX_PARTICLES; i++) {
            if (particles[i].life == 0) {
                if (random8() < speed) {
                    respawnParticle(i);
                }
                continue;
            }
            
            // Calculate forces from all gravity sources
            int16_t ax = 0, ay = 0;  // Acceleration
            uint8_t nearestSource = 0;
            uint8_t minDist = 255;
            
            for (uint8_t s = 0; s < MAX_SOURCES; s++) {
                if (!sources[s].active) continue;
                
                // Get particle position in grid coordinates
                uint8_t px = particles[i].x >> 8;
                uint8_t py = particles[i].y >> 8;
                
                // Calculate distance to source
                uint8_t dist = getDistance(px, py, sources[s].x, sources[s].y);
                if (dist < minDist) {
                    minDist = dist;
                    nearestSource = s;
                }
                
                if (dist > 0) {
                    // Calculate force vector
                    int16_t dx = ((int16_t)sources[s].x << 8) - particles[i].x;
                    int16_t dy = ((int16_t)sources[s].y << 8) - particles[i].y;
                    
                    // Normalize and apply force (inverse square law)
                    int16_t force = sources[s].strength;
                    if (dist > 32) {
                        force = (force * 1024) / (dist * dist);
                    }
                    
                    ax += (dx * force) >> 12;
                    ay += (dy * force) >> 12;
                }
            }
            
            // Update particle color based on nearest source
            particles[i].hue = sources[nearestSource].hue + (minDist >> 2);
            
            // Apply acceleration to velocity
            particles[i].vx += ax;
            particles[i].vy += ay;
            
            // Apply damping
            particles[i].vx = (particles[i].vx * 230) >> 8;
            particles[i].vy = (particles[i].vy * 230) >> 8;
            
            // Limit maximum velocity
            particles[i].vx = constrain(particles[i].vx, -768, 768);
            particles[i].vy = constrain(particles[i].vy, -768, 768);
            
            // Update position
            particles[i].x += particles[i].vx;
            particles[i].y += particles[i].vy;
            
            // Boundary check
            if (particles[i].x >= (GRID_SIZE << 8) || particles[i].y >= (GRID_SIZE << 8)) {
                particles[i].life = 0;
                continue;
            }
            
            // Age particle
            if (particles[i].life > fadeRate) {
                particles[i].life -= fadeRate;
            } else {
                particles[i].life = 0;
            }
            
            // Draw particle with motion blur
            uint8_t px = particles[i].x >> 8;
            uint8_t py = particles[i].y >> 8;
            
            if (px < GRID_SIZE && py < GRID_SIZE) {
                uint8_t index = gridToIndex(px, py);
                uint8_t brightness = scale8(particles[i].life, 200);
                
                // Add to existing color for glow effect
                leds[index] += CHSV(particles[i].hue, 255, brightness);
                
                // Add slight trail
                if (abs(particles[i].vx) > 256 || abs(particles[i].vy) > 256) {
                    uint8_t trailBright = brightness >> 2;
                    int8_t dx = -constrain(particles[i].vx >> 9, -1, 1);
                    int8_t dy = -constrain(particles[i].vy >> 9, -1, 1);
                    
                    uint8_t tx = px + dx;
                    uint8_t ty = py + dy;
                    if (tx < GRID_SIZE && ty < GRID_SIZE) {
                        leds[gridToIndex(tx, ty)] += CHSV(particles[i].hue + 20, 200, trailBright);
                    }
                }
            }
        }
    }
    
    // Parameter control functions
    void setSourceStrength(uint8_t sourceIndex, int8_t strength) {
        if (sourceIndex < MAX_SOURCES) {
            sources[sourceIndex].strength = strength;
        }
    }
    
    void setSourcePosition(uint8_t sourceIndex, uint8_t x, uint8_t y) {
        if (sourceIndex < MAX_SOURCES && x < GRID_SIZE && y < GRID_SIZE) {
            sources[sourceIndex].x = x;
            sources[sourceIndex].y = y;
        }
    }
    
    void toggleSource(uint8_t sourceIndex) {
        if (sourceIndex < MAX_SOURCES) {
            sources[sourceIndex].active = !sources[sourceIndex].active;
        }
    }
};

#endif // GRAVITY_WELL_H
# Matrix Effects Integration Guide

## Adding Gravity Well Effect to main.cpp

### 1. Include the header
Add at the top of main.cpp after other includes:
```cpp
#include "effects/matrix/GravityWell.h"
```

### 2. Create global instance
Add after other global variables:
```cpp
// Matrix effect instances
GravityWell* gravityWell = nullptr;
```

### 3. Initialize in setup()
Add in setup() function after FastLED initialization:
```cpp
// Initialize matrix effects
gravityWell = new GravityWell(leds);
```

### 4. Create wrapper function
Add with other effect functions:
```cpp
void gravityWellEffect() {
    // Use paletteSpeed and fadeAmount as parameters
    gravityWell->update(paletteSpeed, fadeAmount);
}
```

### 5. Add to effects array
Replace the rainbow effects with new ones:
```cpp
Effect effects[] = {
    // Remove these:
    // {"Rainbow", rainbow},
    // {"Rainbow Glitter", rainbowWithGlitter},
    
    // Add new effects:
    {"Gravity Well", gravityWellEffect},
    {"Confetti", confetti},
    {"Sinelon", sinelon},
    {"Juggle", juggle},
    {"BPM", bpm},
    
    // Wave effects
    {"Wave", waveEffect},
    {"Ripple", rippleEffect},
    {"Interference", interferenceEffect},
    
    // Mathematical patterns
    {"Fibonacci", fibonacciSpiral},
    {"Kaleidoscope", kaleidoscope},
    {"Plasma", plasma},
    
    // Nature effects
    {"Fire", fire},
    {"Ocean", ocean}
};
```

### 6. Optional: Add encoder control
For real-time parameter adjustment:
```cpp
// In loop(), add encoder handling:
// Encoder 4: Gravity strength of source 0
// Encoder 5: Number of particles
// Encoder 6: Fade rate
// Encoder 7: Speed multiplier
```

## Performance Considerations

The Gravity Well effect is optimized for performance:
- Uses fixed-point math (8.8 format) for smooth sub-pixel movement
- Pre-calculated distance calculations where possible
- Efficient particle pooling system
- Configurable particle count for performance tuning

## Visual Tuning

Key parameters to adjust:
- `MAX_PARTICLES`: 20 works well for 9x9 grid
- `fadeRate`: 20-40 for nice trails
- `speed`: 10-30 for smooth motion
- Source positions: Experiment with asymmetric layouts
- Source strengths: Mix positive/negative for interesting flows

## Next Effects to Implement

1. **Electromagnetic Pulse** - Similar particle system, different physics
2. **Crystal Growth** - Cellular automaton, very efficient
3. **Quantum Foam** - Random spawning, simple to implement
4. **Lava Lamp Matrix** - Metaball algorithm, moderate complexity
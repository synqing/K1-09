# Sensory Bridge: Lightshow Implementation Plan

## Current Light Show Modes

1. **LIGHT_MODE_GDFT**
   - Default spectral visualization
   - Maps frequency bands directly to LED positions
   - Uses saturation and hue shifting for visual interest

2. **LIGHT_MODE_GDFT_CHROMAGRAM**
   - Visualizes musical notes via chromagram
   - Creates gradient representation of note intensity

3. **LIGHT_MODE_GDFT_CHROMAGRAM_DOTS**
   - Dot-based visualization of chromagram data
   - Visualizes note intensity as dot position and brightness

4. **LIGHT_MODE_BLOOM**
   - Creates flowing, blooming visuals that respond to music
   - Uses sprite-based rendering system
   - Strong organic feel with good musical responsiveness

5. **LIGHT_MODE_VU_DOT**
   - Simple VU meter visualization using dots
   - Directly maps volume to dot position
   - Clean, minimal aesthetic

6. **LIGHT_MODE_KALEIDOSCOPE**
   - Creates symmetrical flowing patterns
   - Uses Perlin noise for organic movement
   - Divides audio into low/mid/high bands for better musical response

7. **LIGHT_MODE_QUANTUM_COLLAPSE**
   - Physics-based simulation with particles and probability fields
   - Complex fluid dynamics and quantum-inspired mechanics
   - Currently limited musical responsiveness

## Motion Mechanics Analysis: Quantum Collapse Mode

### Core Motion Systems

#### 1. Fluid Dynamics
```cpp
// Simplified view of fluid mechanics
fluid_velocity[i] = velocity[i] * (1.0 - 2.0*diffusion) + 
                   (velocity[i-1] + velocity[i+1]) * diffusion;
fluid_velocity[i] *= 0.99; // Natural friction decay
```
- Uses partial differential equation approximation (similar to Navier-Stokes)
- Fluid creates organic flow patterns with natural turbulence
- Audio reactivity is limited to velocity impulses on beats

#### 2. Wave Probability Field
```cpp
// Field diffusion with asymmetric flow
left_mix = diffusion + flow_direction + fluid_velocity[i] * 2.0;
right_mix = diffusion - flow_direction - fluid_velocity[i] * 2.0;
wave[i] = field[i]*center + field[i-1]*left_mix + field[i+1]*right_mix;
```
- Core visual element that creates flowing waves
- Field values represent probability density (0.0-1.0)
- Audio affects field through minor wave additions and collapse events
- Lacks direct frequency-specific response

#### 3. Particle Physics
```cpp
// Particle motion with field gradients
field_gradient = (wave[pos+3] - wave[pos-3]) * 0.3;
acceleration = field_gradient * 0.25 * speed / mass_factor;
particle_velocities[i] += acceleration;
```
- Uses simplified Newtonian mechanics with field forces
- Particles attracted to high-density regions of probability field
- Audio responses primarily through:
  - Energy level modification
  - Velocity boosts on beats
  - Not directly mapped to specific frequencies

#### 4. Quantum Collapse Events
```cpp
// On beat detection:
if (audio_level > average * 1.3 && audio_level > 0.15) {
  // Create collapse with Gaussian probability distribution
  collapse_probability = exp(-distance*distance * 8.0 * audio_intensity);
}
```
- Major audio feature - creates "quantum collapse" patterns
- More visible response to music, but limited by cooldown timer
- Uses weighted random collapses rather than frequency-mapped collapses

### Variables Controlling Motion

| Variable | Purpose | Audio Influence | Range |
|----------|---------|----------------|-------|
| field_energy | Overall animation energy | Major - directly increased by audio | 0.5-2.5 |
| speed_mult_fixed | Global speed multiplier | Indirect - via MOOD | 0.7-4.7 |
| audio_impact | Smoothed audio tracker | Direct from audio_vu_level | 0.0-2.0 |
| audio_pulse | Beat response | Direct from beat detection | 0.0-1.5 |
| beat_strength | Beat detection | Direct from energy_delta | 0.0-1.0 |
| fluid_velocity[] | Flow field | Minor - via impulses | -0.4 to 0.4 |
| particle_energies[] | Particle energy | Moderate - affects size, speed | 0.1-1.5 |

### Colour Mechanics

1. **Triadic Colour Scheme**
   - Uses three colours 120° apart on colour wheel
   - Follows auto colour shift but lacks musical responsiveness
   - Zones blend smoothly between colours

2. **Audio-Reactive Colour Effects**
   - Subtle hue shifts based on audio level (only ±2%)
   - Saturation modulation (±20%)
   - No direct frequency-to-colour mapping

3. **Saturation Effects**
   - Dynamic saturation based on brightness (natural desaturation)
   - Audio level can boost saturation slightly

### Problems with Musical Responsiveness

1. **Limited Frequency Differentiation**
   - Uses only `audio_vu_level` (overall volume)
   - No separation of bass/mid/treble responses
   - Cannot map different instruments/frequencies to different effects

2. **Subtle Audio Reactions**
   - Most audio multipliers are small (0.1-0.3 range)
   - Visual effects get masked by constant animation
   - Beat detection thresholds may be too high

3. **Slow Response Curves**
   - Audio impact has slow decay (0.05 rate)
   - Collapse cooldown prevents rapid beat response
   - Smoothing filters reduce impact of sudden changes

## Proposed Improvements for Quantum Collapse

1. **Frequency Band Separation**
   - Split audio into at least 3 bands (bass, mid, treble)
   - Map each band to different visual elements:
     - Bass → Field energy and flow direction
     - Mids → Particle energy and size
     - Highs → Bloom effects and colour shifts

2. **Enhanced Beat Detection**
   - Implement multi-band onset detection
   - Reduce cooldown time between collapses (150ms instead of 250ms)
   - Vary collapse effects based on frequency content

3. **Direct Spectral Mapping**
   - Create frequency-to-position mapping for collapses
   - Low frequencies collapse at edges, high frequencies at center
   - Colour zones based on dominant frequency ranges

4. **Visual Amplification**
   - Increase audio reaction multipliers (0.3-0.8 range)
   - Create more pronounced visual changes on beats
   - Add temporary "freeze" moments after major beats

## Planned New Light Show Modes

1. **LIGHT_MODE_FREQUENCY_STORM**
   - Visualizes audio as dynamic storm systems
   - Bass frequencies create "thunder" and large, slow-moving structures
   - Mid frequencies create "rain" particles with varying intensity
   - High frequencies create "lightning" flashes and arcs
   - Implementation: Particle system with varying size/speed/brightness

2. **LIGHT_MODE_HARMONIC_FLOW**
   - Maps harmonic structure of audio to flowing riverlike patterns
   - Spectral centroid determines "current" direction and speed
   - Harmonic vs. percussive separation for different visual elements
   - Implementation: Flow field visualization with harmonic mapping

3. **LIGHT_MODE_NEURAL_PULSE**
   - Visualizes audio as firing neural connections
   - Beat detection creates "neural firing" events
   - Networks of connections light up based on frequency content
   - Implementation: Graph-based visualization with nodes and edges

4. **LIGHT_MODE_SPECTRAL_FABRIC**
   - Weaves audio spectrum into fabric-like patterns
   - Vertical threads represent frequency bands
   - Horizontal threads represent time
   - Creates knot points at audio peaks
   - Implementation: Procedural textile simulation

5. **LIGHT_MODE_RHYTHM_LANDSCAPES**
   - Creates evolving terrain based on rhythm patterns
   - Builds mountains and valleys based on rhythm strength
   - Changes lighting based on melodic elements
   - Implementation: Height map generation with spectral mapping

## Implementation Strategy

### Phase 1: Improve Existing Modes
- Enhance Quantum Collapse with frequency band separation
- Update Kaleidoscope for better musical responsiveness
- Optimize Bloom mode for smoother animations

### Phase 2: Core New Modes
- Implement Frequency Storm mode
- Implement Harmonic Flow mode
- Add comprehensive musical feature extraction system

### Phase 3: Advanced Modes
- Implement Neural Pulse mode
- Implement Spectral Fabric mode
- Implement Rhythm Landscapes mode

### Phase 4: Performance Optimization
- SIMD optimizations for fluid dynamics
- Memory usage optimization
- Adaptive quality based on system performance

## Technical Requirements

### Audio Analysis Enhancements
- Implement constant-Q transform for better note resolution
- Add onset detection system for all frequency bands
- Implement harmonic/percussive separation

### Visual System Enhancements
- Implement shader-like pixel processing for efficiency
- Create unified particle system architecture
- Add frame interpolation for smoother transitions

### User Experience
- Allow saving favorite parameter combinations
- Implement smooth transitions between modes
- Add adaptive audio sensitivity calibration

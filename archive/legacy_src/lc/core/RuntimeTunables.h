#ifndef RUNTIME_TUNABLES_H
#define RUNTIME_TUNABLES_H

#include <stdint.h>

// Runtime-adjustable visual pipeline parameters (EDIT/PLAY mappings)
extern bool     rt_enable_palette_curation;
extern uint8_t  rt_curation_green_scale;
extern uint8_t  rt_curation_brown_green_scale;

extern bool     rt_enable_vignette;
extern uint8_t  rt_vignette_strength;

extern bool     rt_enable_blur2d;
extern uint8_t  rt_blur2d_amount;

extern bool     rt_enable_auto_exposure;
extern uint8_t  rt_ae_target_luma;

extern bool     rt_apply_gamma;
extern float    rt_gamma_value;

extern bool     rt_enable_brown_guardrail;
extern uint8_t  rt_max_g_percent_of_r;
extern uint8_t  rt_max_b_percent_of_r;

// Palette auto-cycling per layer (NAV/EDIT/PLAY)
extern bool     rt_autocycle_nav;
extern bool     rt_autocycle_edit;
extern bool     rt_autocycle_play;
extern uint32_t rt_palette_dwell_ms; // dwell per palette when auto-cycling is enabled

// Transition settings
extern uint8_t  rt_transition_type; // 0=Fade, 1=Wipe, 2=Blend

// PixelSpork integration tunables
extern uint16_t rt_spork_offset_rate_ms;   // 0 => auto map from paletteSpeed
extern uint8_t  rt_spork_offset_step;      // gradient offset step per update
extern uint16_t rt_spork_grad_len_override; // 0 => auto (segment length)
extern uint16_t rt_spork_fade_ms;          // EffectSet fade duration

#endif // RUNTIME_TUNABLES_H

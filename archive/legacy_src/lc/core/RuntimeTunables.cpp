#include "RuntimeTunables.h"

// Defaults aligned with prior static settings in main.cpp
bool     rt_enable_palette_curation       = false;
uint8_t  rt_curation_green_scale          = 230;
uint8_t  rt_curation_brown_green_scale    = 190;

bool     rt_enable_vignette               = false;
uint8_t  rt_vignette_strength             = 64;

bool     rt_enable_blur2d                 = false;
uint8_t  rt_blur2d_amount                 = 24;

bool     rt_enable_auto_exposure          = false;
uint8_t  rt_ae_target_luma                = 110;

bool     rt_apply_gamma                   = true;
float    rt_gamma_value                   = 2.2f;

bool     rt_enable_brown_guardrail        = false;
uint8_t  rt_max_g_percent_of_r            = 28;
uint8_t  rt_max_b_percent_of_r            = 8;

bool     rt_autocycle_nav                 = false;
bool     rt_autocycle_edit                = false;
bool     rt_autocycle_play                = false;
uint32_t rt_palette_dwell_ms              = 8000; // default dwell; gated by per-layer toggles
uint8_t  rt_transition_type               = 0;

uint16_t rt_spork_offset_rate_ms          = 0;    // 0 => auto from paletteSpeed
uint8_t  rt_spork_offset_step             = 1;
uint16_t rt_spork_grad_len_override       = 0;    // 0 => auto
uint16_t rt_spork_fade_ms                 = 2000;

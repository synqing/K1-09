#pragma once
#include <stdint.h>

/* ===================== Audio Config (compile-time) ===================== */

#define AUDIO_DIAG_TEMPO 1
#define AUDIO_DIAG_FLUX 1

#ifndef audio_sample_rate
#define audio_sample_rate 16000u        // Hz (Layer-1 WS)
#endif

#ifndef chunk_size
#define chunk_size 128u                 // samples per producer tick
#endif

#ifndef freq_bins
#define freq_bins 64u                   // musical half-steps A2..C8
#endif

// ========== Optional diagnostics (compile-time toggles) ==========
#ifndef AUDIO_DIAG_TEMPO
#define AUDIO_DIAG_TEMPO 0
#endif
#ifndef AUDIO_DIAG_TEMPO_TOPK
#define AUDIO_DIAG_TEMPO_TOPK 4
#endif
#ifndef AUDIO_DIAG_TEMPO_PERIOD
#define AUDIO_DIAG_TEMPO_PERIOD 32u      // tempo dump cadence in AP ticks
#endif

#ifndef AUDIO_DIAG_FLUX
#define AUDIO_DIAG_FLUX 0
#endif
#ifndef AUDIO_DIAG_FLUX_PERIOD_MS
#define AUDIO_DIAG_FLUX_PERIOD_MS 1000u  // flux histogram cadence (ms)
#endif

// ========== smooth_spectral defaults & bounds (Q16.16 alpha) ==========
#ifndef ema_alpha_q16_default
#define ema_alpha_q16_default (uint32_t)(0.10 * 65536.0)  // ~Lixie-like
#endif

#ifndef ema_alpha_q16_min
#define ema_alpha_q16_min     (uint32_t)(0.02 * 65536.0)  // very silky
#endif
#ifndef ema_alpha_q16_max
#define ema_alpha_q16_max     (uint32_t)(0.60 * 65536.0)  // very snappy (upper bound for safety)
#endif

// ---- Frequency-bin centers (Hz): 64 half-steps from 110 Hz (A2) to ~4186 Hz (C8) ----
static constexpr float freq_bin_centers_hz[freq_bins] = {
  110.0000000000f, 116.5409403795f, 123.4708253140f, 130.8127826503f,
  138.5913154884f, 146.8323839587f, 155.5634918610f, 164.8137784564f,
  174.6141157165f, 184.9972113558f, 195.9977179909f, 207.6523487900f,
  220.0000000000f, 233.0818807590f, 246.9416506281f, 261.6255653006f,
  277.1826309769f, 293.6647679174f, 311.1269837221f, 329.6275569129f,
  349.2282314330f, 369.9944227116f, 391.9954359817f, 415.3046975800f,
  440.0000000000f, 466.1637615181f, 493.8833012561f, 523.2511306012f,
  554.3652619537f, 587.3295358348f, 622.2539674442f, 659.2551138257f,
  698.4564628660f, 739.9888454233f, 783.9908719635f, 830.6093951599f,
  880.0000000000f, 932.3275230362f, 987.7666025122f, 1046.5022612024f,
  1108.7305239075f, 1174.6590716696f, 1244.5079348883f, 1318.5102276515f,
  1396.9129257320f, 1479.9776908465f, 1567.9817439270f, 1661.2187903198f,
  1760.0000000000f, 1864.6550460724f, 1975.5332050245f, 2093.0045224048f,
  2217.4610478150f, 2349.3181433393f, 2489.0158697766f, 2637.0204553030f,
  2793.8258514640f, 2959.9553816931f, 3135.9634878540f, 3322.4375806396f,
  3520.0000000000f, 3729.3100921447f, 3951.0664100490f, 4186.0090448096f
};

// --------- Compile-time sanity for EMA alpha bounds ---------
static_assert(ema_alpha_q16_min <= ema_alpha_q16_default &&
              ema_alpha_q16_default <= ema_alpha_q16_max,
              "EMA alpha default must lie within [min,max]");

static_assert(ema_alpha_q16_max <= (uint32_t)(0.60 * 65536.0),
              "ema_alpha_q16_max too high; risk of overflow in (alpha*delta)>>16");




/* ===================== End of Audio Config ===================== */


/*
sfa_config.h

constexpr uint32_t SFA_FS;

constexpr uint16_t SFA_CHUNK; (e.g., 128)

constexpr uint16_t SFA_BINS; (e.g., keep your current NUM_FREQS initially)

extern const float SFA_BIN_CENTERS_HZ[SFA_BINS]; (stable table)

constexpr q16 SFA_EMA_ALPHA_Q16; for spec_smooth

constexpr uint16_t SFA_BAND_LO_START/END, etc. (indices, not Hz)

All constants resolved at compile-time. VP can read SFA_BIN_CENTERS_HZ to label knobs.
*/

#include "lightshow_modes.h"

namespace {
constexpr uint32_t kPaletteTraceMagic = 0xBEEF0000u;
}

void cache_frame_config() {
  frame_config.PHOTONS = CONFIG.PHOTONS;
  frame_config.CHROMA = CONFIG.CHROMA;
  frame_config.MOOD = CONFIG.MOOD;
  frame_config.LIGHTSHOW_MODE = CONFIG.LIGHTSHOW_MODE;
  frame_config.SQUARE_ITER = CONFIG.SQUARE_ITER;
  frame_config.SATURATION = CONFIG.SATURATION;
  frame_config.PALETTE_INDEX = CONFIG.PALETTE_INDEX;

  const CRGB16* palette_lut = palette_lut_for_index(frame_config.PALETTE_INDEX);
  frame_config.palette_ptr = palette_lut;
  frame_config.palette_size = palette_lut_size(frame_config.PALETTE_INDEX);

  uint32_t pal_state = frame_config.palette_ptr ? (kPaletteTraceMagic | frame_config.palette_size) : 0u;
  TRACE_INFO(LED_FRAME_START, pal_state);
}

uint8_t scale_component_u8(SQ15x16 value, uint16_t scale) {
  if (value < SQ15x16(0)) {
    return 0;
  }
  if (value > SQ15x16(1)) {
    value = SQ15x16(1);
  }
  return (value * SQ15x16(scale)).getInteger();
}

uint32_t pack_led_sample(uint16_t index, const CRGB16& color, bool palette_enabled) {
  const uint8_t r = scale_component_u8(color.r, 255);
  const uint8_t g = scale_component_u8(color.g, 127);
  const uint8_t b = scale_component_u8(color.b, 127);
  const uint32_t any = (r | g | b) ? 1u : 0u;
  return (uint32_t(index) << 24) |
         (any << 23) |
         (uint32_t(palette_enabled ? 1u : 0u) << 22) |
         (uint32_t(r) << 14) |
         (uint32_t(g) << 7) |
         uint32_t(b);
}

void get_smooth_spectrogram() {
  static SQ15x16 spectrogram_smooth_last[NUM_FREQS];

  static uint32_t last_timing_print = 0;
  if (millis() - last_timing_print > 1000) {
    last_timing_print = millis();
  }

  for (uint8_t bin = 0; bin < NUM_FREQS; bin++) {
    SQ15x16 note_brightness = spectrogram[bin];

    if (spectrogram_smooth[bin] < note_brightness) {
      SQ15x16 distance = note_brightness - spectrogram_smooth[bin];
      spectrogram_smooth[bin] += distance * SQ15x16(0.5);

    } else if (spectrogram_smooth[bin] > note_brightness) {
      SQ15x16 distance = spectrogram_smooth[bin] - note_brightness;
      spectrogram_smooth[bin] -= distance * SQ15x16(0.25);
    }
  }

  (void)spectrogram_smooth_last;
}

CRGB calc_chromagram_color() {
  CRGB sum_color = CRGB(0, 0, 0);
  for (uint8_t i = 0; i < 12; i++) {
    float prog = i / 12.0f;

    float bright = note_chromagram[i];
    bright = bright * bright * 0.8f;

    if (bright > 1.0f) {
      bright = 1.0f;
    }

    if (chromatic_mode == true) {
      CRGB out_col = CHSV(uint8_t(prog * 255), uint8_t(frame_config.SATURATION * 255), uint8_t(bright * 255));
      sum_color += out_col;
    }
  }

  if (chromatic_mode == false) {
    sum_color = force_saturation(sum_color, uint8_t(frame_config.SATURATION * 255));
  }

  return sum_color;
}

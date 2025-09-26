#include <Arduino.h>

// L1 mic bring-up (unchanged namespace)
#include "audio/sph0645.h"

// L2 audio producer + public bus
#include "audio/audio_producer.h"
#include "audio/audio_bus.h"
#include "audio/audio_config.h"
#include "storage/NVS.h"

namespace {

void vp_test_render() {
  const uint32_t now = millis();
  static uint32_t last_timing = 0;
  static uint32_t last_tempo  = 0;
  static uint32_t last_energy = 0;
  static uint32_t last_input  = 0;

  const AudioFrame* f = acquire_spectral_frame();
  if (!f) {
    return;
  }

  const float bpm = (float)f->tempo_bpm / 65536.0f;
  const float str = (float)f->beat_strength / 65536.0f;
  const float phs = (float)f->beat_phase / 65536.0f;
  const float conf = (float)f->tempo_confidence / 65536.0f;
  const float sil = (float)f->tempo_silence / 65536.0f;

  constexpr const char* kTiming  = "\x1b[95m";  // bright magenta
  constexpr const char* kTempo   = "\x1b[38;5;208m";
  constexpr const char* kEnergy  = "\x1b[36m";
  constexpr const char* kInput   = "\x1b[38;5;205m";
  constexpr const char* kWhite   = "\x1b[37m";
  constexpr const char* kReset   = "\x1b[0m";

  if ((now - last_timing) >= 7000u) {
    Serial.printf("%sTiming%s  : %sepoch=%lu | ready=%u | beat=%u%s\n",
                  kTiming, kWhite,
                  kWhite,
                  static_cast<unsigned long>(f->audio_frame_epoch),
                  (unsigned)f->tempo_ready,
                  (unsigned)f->beat_flag,
                  kReset);
    last_timing = now;
  }

  if ((now - last_tempo) >= 7100u) {
    Serial.printf("%sTempo%s   : %sbpm=%.1f | phase=%.2f%s\n",
                  kTempo, kWhite,
                  kWhite,
                  bpm,
                  phs,
                  kReset);
    last_tempo = now;
  }

  if ((now - last_energy) >= 7200u) {
    Serial.printf("%sEnergy%s  : %sstrength=%.2f | conf=%.2f%s\n",
                  kEnergy, kWhite,
                  kWhite,
                  str,
                  conf,
                  kReset);
    last_energy = now;
  }

  if ((now - last_input) >= 7300u) {
    Serial.printf("%sAP Input%s: %ssilence=%.2f%s\n",
                  kInput, kWhite,
                  kWhite,
                  sil,
                  kReset);
    last_input = now;
  }
}

} // namespace

static int32_t q24_chunk[chunk_size]; // Q24 buffer (24-bit in 32, after >>8)

void setup() {
  Serial.begin(115200);
  K1Lightwave::Audio::Sph0645::setup();     // Layer 1
  audio_pipeline_init();                    // Layer 2
}

void loop() {
  // Pull exactly one full chunk in Q24 from Layer 1
  if (K1Lightwave::Audio::Sph0645::read_q24_chunk(q24_chunk, chunk_size)) {
    audio_pipeline_tick(q24_chunk, millis());   // Produce one AudioFrame for VP
    vp_test_render();
  }

  storage::nvs::poll();

  // If you want to peek (optional, keep commented for tidy main):
  // const AudioFrame* f = acquire_spectral_frame();
  // (void)f;
}

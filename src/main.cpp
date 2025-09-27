#include <Arduino.h>

// L1 mic bring-up (unchanged namespace)
#include "AP/sph0645.h"

// L2 audio producer + public bus
#include "AP/audio_producer.h"
#include "AP/audio_bus.h"
#include "AP/audio_config.h"
#include "debug/debug_flags.h"
#include "storage/NVS.h"
#include "VP/vp.h"

namespace {

void print_debug_help();
void print_debug_status();
void handle_debug_key(char c);

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

  if (debug_flags::enabled(debug_flags::kGroupTempoEnergy) &&
      (now - last_timing) >= 7000u) {
    Serial.printf("%sTiming%s  : %sepoch=%lu | ready=%u | beat=%u%s\n",
                  kTiming, kWhite,
                  kWhite,
                  static_cast<unsigned long>(f->audio_frame_epoch),
                  (unsigned)f->tempo_ready,
                  (unsigned)f->beat_flag,
                  kReset);
    last_timing = now;
  }

  if (debug_flags::enabled(debug_flags::kGroupTempoEnergy) &&
      (now - last_tempo) >= 7100u) {
    Serial.printf("%sTempo%s   : %sbpm=%.1f | phase=%.2f%s\n",
                  kTempo, kWhite,
                  kWhite,
                  bpm,
                  phs,
                  kReset);
    last_tempo = now;
  }

  if (debug_flags::enabled(debug_flags::kGroupTempoEnergy) &&
      (now - last_energy) >= 7200u) {
    Serial.printf("%sEnergy%s  : %sstrength=%.2f | conf=%.2f%s\n",
                  kEnergy, kWhite,
                  kWhite,
                  str,
                  conf,
                  kReset);
    last_energy = now;
  }

  if (debug_flags::enabled(debug_flags::kGroupAPInput) &&
      (now - last_input) >= 7300u) {
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
  vp::init();                               // Visual Pipeline init
  print_debug_status();
  print_debug_help();
}

void loop() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    handle_debug_key(c);
  }

  // Pull exactly one full chunk in Q24 from Layer 1
  if (K1Lightwave::Audio::Sph0645::read_q24_chunk(q24_chunk, chunk_size)) {
    audio_pipeline_tick(q24_chunk, millis());   // Produce one AudioFrame for VP
    vp_test_render();                           // Existing AP debug stream
    vp::tick();                                 // VP consumer + renderer
  }

  storage::nvs::poll();

  // If you want to peek (optional, keep commented for tidy main):
  // const AudioFrame* f = acquire_spectral_frame();
  // (void)f;
}

namespace {

void print_debug_status() {
  const uint32_t mask = debug_flags::mask();
  Serial.printf("[debug] mask=0x%08lX [1:%s 2:%s 3:%s 0:%s]\n",
                static_cast<unsigned long>(mask),
                debug_flags::enabled(debug_flags::kGroupAPInput)     ? "ON " : "off",
                debug_flags::enabled(debug_flags::kGroupTempoEnergy) ? "ON " : "off",
                debug_flags::enabled(debug_flags::kGroupTempoFlux)   ? "ON " : "off",
                debug_flags::enabled(debug_flags::kGroupDCAndDrift)  ? "ON " : "off");
  Serial.printf("[debug] VP: 4:%s\n",
                debug_flags::enabled(debug_flags::kGroupVP) ? "ON " : "off");
}

void print_debug_help() {
  Serial.println("[debug] controls: 1=AP Input+AC  2=Tempo+Energy  3=[tempo]/[flux]  4=VP  0=DC/Drift  ?=help");
  Serial.println("[HMI]   controls: +/- brightness   [/]= speed    </> lightshow mode");
}

void handle_debug_key(char c) {
  switch (c) {
    case '1':
      debug_flags::toggle(debug_flags::kGroupAPInput);
      Serial.printf("[debug] group 1 -> %s\n",
                    debug_flags::enabled(debug_flags::kGroupAPInput) ? "ON" : "OFF");
      print_debug_status();
      return;
    case '2':
      debug_flags::toggle(debug_flags::kGroupTempoEnergy);
      Serial.printf("[debug] group 2 -> %s\n",
                    debug_flags::enabled(debug_flags::kGroupTempoEnergy) ? "ON" : "OFF");
      print_debug_status();
      return;
    case '3':
      debug_flags::toggle(debug_flags::kGroupTempoFlux);
      Serial.printf("[debug] group 3 -> %s\n",
                    debug_flags::enabled(debug_flags::kGroupTempoFlux) ? "ON" : "OFF");
      print_debug_status();
      return;
    case '4':
      debug_flags::toggle(debug_flags::kGroupVP);
      Serial.printf("[debug] group 4 -> %s\n",
                    debug_flags::enabled(debug_flags::kGroupVP) ? "ON" : "OFF");
      print_debug_status();
      return;
    case '0':
      debug_flags::toggle(debug_flags::kGroupDCAndDrift);
      Serial.printf("[debug] group 0 -> %s\n",
                    debug_flags::enabled(debug_flags::kGroupDCAndDrift) ? "ON" : "OFF");
      print_debug_status();
      return;
    case '+':
      vp::brightness_up();
      {
        auto s = vp::hmi_status();
        Serial.printf("[HMI] Brightness -> %u\n", s.brightness);
      }
      return;
    case '-':
      vp::brightness_down();
      {
        auto s = vp::hmi_status();
        Serial.printf("[HMI] Brightness -> %u\n", s.brightness);
      }
      return;
    case ']':
      vp::speed_up();
      {
        auto s = vp::hmi_status();
        Serial.printf("[HMI] Speed -> %.2fx\n", s.speed);
      }
      return;
    case '[':
      vp::speed_down();
      {
        auto s = vp::hmi_status();
        Serial.printf("[HMI] Speed -> %.2fx\n", s.speed);
      }
      return;
    case '>':
      vp::next_mode();
      {
        auto s = vp::hmi_status();
        Serial.printf("[HMI] Mode -> %u\n", s.mode);
      }
      return;
    case '<':
      vp::prev_mode();
      {
        auto s = vp::hmi_status();
        Serial.printf("[HMI] Mode -> %u\n", s.mode);
      }
      return;
    case '?':
    case 'h':
    case 'H':
      print_debug_help();
      return;
    case '\r':
    case '\n':
      return;
    default:
      Serial.printf("[debug] unhandled key 0x%02X ('%c')\n",
                    static_cast<unsigned char>(c),
                    (c >= 32 && c < 127) ? c : '.');
      return;
  }
}

} // namespace

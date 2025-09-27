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

// Forward decls
void print_debug_help();
void print_debug_status();
void handle_debug_key(char c);

// Simple AP debug stream for tempo/energy (optional; safe to keep)
void vp_test_render() {
  const uint32_t now = millis();
  static uint32_t last_timing = 0;
  static uint32_t last_tempo  = 0;
  static uint32_t last_energy = 0;
  static uint32_t last_input  = 0;

  // Lock-free peek at the latest published AudioFrame
  const AudioFrame* f = acquire_spectral_frame();
  if (!f) {
    return;
  }

  const float bpm  = static_cast<float>(f->tempo_bpm)        / 65536.0f;
  const float str  = static_cast<float>(f->beat_strength)    / 65536.0f;
  const float phs  = static_cast<float>(f->beat_phase)       / 65536.0f;
  const float conf = static_cast<float>(f->tempo_confidence) / 65536.0f;
  const float sil  = static_cast<float>(f->tempo_silence)    / 65536.0f;

  constexpr const char* kTiming = "\x1b[95m";        // bright magenta
  constexpr const char* kTempo  = "\x1b[38;5;208m";  // orange
  constexpr const char* kEnergy = "\x1b[36m";        // cyan
  constexpr const char* kInput  = "\x1b[38;5;205m";  // pink
  constexpr const char* kWhite  = "\x1b[37m";
  constexpr const char* kReset  = "\x1b[0m";

  if (debug_flags::enabled(debug_flags::kGroupTempoEnergy) &&
      (now - last_timing) >= 7000u) {
    Serial.printf("%sTiming%s  : epoch=%lu | ready=%u | beat=%u%s\n",
                  kTiming, kWhite,
                  static_cast<unsigned long>(f->audio_frame_epoch),
                  static_cast<unsigned>(f->tempo_ready),
                  static_cast<unsigned>(f->beat_flag),
                  kReset);
    last_timing = now;
  }

  if (debug_flags::enabled(debug_flags::kGroupTempoEnergy) &&
      (now - last_tempo) >= 7100u) {
    Serial.printf("%sTempo%s   : bpm=%.1f | phase=%.2f%s\n",
                  kTempo, kWhite, bpm, phs, kReset);
    last_tempo = now;
  }

  if (debug_flags::enabled(debug_flags::kGroupTempoEnergy) &&
      (now - last_energy) >= 7200u) {
    Serial.printf("%sEnergy%s  : strength=%.2f | conf=%.2f%s\n",
                  kEnergy, kWhite, str, conf, kReset);
    last_energy = now;
  }

  if (debug_flags::enabled(debug_flags::kGroupAPInput) &&
      (now - last_input) >= 7300u) {
    Serial.printf("%sAP Input%s: silence=%.2f%s\n",
                  kInput, kWhite, sil, kReset);
    last_input = now;
  }
}

} // namespace (anon)

// Q24 buffer (24-bit mic data widened to 32-bit after >>8)
static int32_t q24_chunk[chunk_size];

void setup() {
  Serial.begin(115200);

  // Layer 1: mic / I2S
  K1Lightwave::Audio::Sph0645::setup();

  // Layer 2: audio producer
  audio_pipeline_init();

  // Visual pipeline
  vp::init();

  print_debug_status();
  print_debug_help();
}

void loop() {
  // HMI / debug key handling
  while (Serial.available() > 0) {
    char c = static_cast<char>(Serial.read());
    handle_debug_key(c);
  }

  // Producer: pull exactly one full 128-sample chunk (Q24)
  if (K1Lightwave::Audio::Sph0645::read_q24_chunk(q24_chunk, chunk_size)) {
    // Publish one AudioFrame (producer)
    audio_pipeline_tick(q24_chunk, millis());

    // Optional: AP-side telemetry
    vp_test_render();
  }

  // *** IMPORTANT CHANGE ***
  // Consumer: always tick VP every loop, even if mic read missed this pass.
  // VP will internally acquire the latest published AudioFrame (if any).
  vp::tick();

  // Non-RT upkeep
  storage::nvs::poll();

  // (Optional peek for bring-up; keep disabled for tidy main)
  // const AudioFrame* f = acquire_spectral_frame();
  // (void)f;
}

namespace { // -------- Debug UI / Controls --------

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
  Serial.println("[HMI]   controls: +/- brightness   [/]= speed   </> mode   p/o palette   m/n sensitivity");
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
                    debug_flags::enabled(debug_flags::kGroupVP) ? "ON " : "OFF");
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

    case 'p':
      vp::next_palette();
      {
        auto s = vp::hmi_status();
        Serial.printf("[HMI] Palette -> %u (%s)\n",
                      s.palette,
                      (s.palette_name && s.palette_name[0]) ? s.palette_name : "");
      }
      return;

    case 'o':
      vp::prev_palette();
      {
        auto s = vp::hmi_status();
        Serial.printf("[HMI] Palette -> %u (%s)\n",
                      s.palette,
                      (s.palette_name && s.palette_name[0]) ? s.palette_name : "");
      }
      return;

    case 'm':
      vp::sensitivity_up();
      {
        auto s = vp::hmi_status();
        Serial.printf("[HMI] Sensitivity -> %.2fx\n", s.sensitivity);
      }
      return;

    case 'n':
      vp::sensitivity_down();
      {
        auto s = vp::hmi_status();
        Serial.printf("[HMI] Sensitivity -> %.2fx\n", s.sensitivity);
      }
      return;

    case '}': // duplicate next_palette for some keyboards
      vp::next_palette();
      {
        auto s = vp::hmi_status();
        Serial.printf("[HMI] Palette -> %u (%s)\n",
                      s.palette,
                      (s.palette_name && s.palette_name[0]) ? s.palette_name : "");
      }
      return;

    case '{': // duplicate prev_palette for some keyboards
      vp::prev_palette();
      {
        auto s = vp::hmi_status();
        Serial.printf("[HMI] Palette -> %u (%s)\n",
                      s.palette,
                      (s.palette_name && s.palette_name[0]) ? s.palette_name : "");
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


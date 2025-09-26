#include "debug_ui.h"

#include <Arduino.h>

#include "debug/debug_flags.h"
#include "AP/audio_bus.h"

namespace debug_ui {

namespace {

void print_help() {
  Serial.println("[debug] controls: 1=AP Input+AC  2=Tempo+Energy  3=[tempo]/[flux]  4=VP  0=DC/Drift  ?=help");
}

void print_status() {
  const uint32_t mask = debug_flags::mask();
  Serial.printf("[debug] mask=0x%08lX [1:%s 2:%s 3:%s 0:%s]  VP: 4:%s\n",
                static_cast<unsigned long>(mask),
                debug_flags::enabled(debug_flags::kGroupAPInput)     ? "ON " : "off",
                debug_flags::enabled(debug_flags::kGroupTempoEnergy) ? "ON " : "off",
                debug_flags::enabled(debug_flags::kGroupTempoFlux)   ? "ON " : "off",
                debug_flags::enabled(debug_flags::kGroupDCAndDrift)  ? "ON " : "off",
                debug_flags::enabled(debug_flags::kGroupVP)          ? "ON " : "off");
}

// Periodic debug emitters
uint32_t last_timing = 0;
uint32_t last_tempo  = 0;
uint32_t last_energy = 0;
uint32_t last_input  = 0;

} // namespace

void init() {
  print_status();
  print_help();
}

bool handle_key(char c) {
  switch (c) {
    case '1':
      debug_flags::toggle(debug_flags::kGroupAPInput);
      Serial.printf("[debug] group 1 -> %s\n",
                    debug_flags::enabled(debug_flags::kGroupAPInput) ? "ON" : "OFF");
      print_status();
      return true;
    case '2':
      debug_flags::toggle(debug_flags::kGroupTempoEnergy);
      Serial.printf("[debug] group 2 -> %s\n",
                    debug_flags::enabled(debug_flags::kGroupTempoEnergy) ? "ON" : "OFF");
      print_status();
      return true;
    case '3':
      debug_flags::toggle(debug_flags::kGroupTempoFlux);
      Serial.printf("[debug] group 3 -> %s\n",
                    debug_flags::enabled(debug_flags::kGroupTempoFlux) ? "ON" : "OFF");
      print_status();
      return true;
    case '4':
      debug_flags::toggle(debug_flags::kGroupVP);
      Serial.printf("[debug] group 4 -> %s\n",
                    debug_flags::enabled(debug_flags::kGroupVP) ? "ON" : "OFF");
      print_status();
      return true;
    case '0':
      debug_flags::toggle(debug_flags::kGroupDCAndDrift);
      Serial.printf("[debug] group 0 -> %s\n",
                    debug_flags::enabled(debug_flags::kGroupDCAndDrift) ? "ON" : "OFF");
      print_status();
      return true;
    case '?':
    case 'h':
    case 'H':
      print_help();
      return true;
    case '\r':
    case '\n':
      return true;
    default:
      return false;
  }
}

void tick() {
  const uint32_t now = millis();
  const AudioFrame* f = acquire_spectral_frame();
  if (!f) return;

  const float bpm  = (float)f->tempo_bpm / 65536.0f;
  const float str  = (float)f->beat_strength / 65536.0f;
  const float phs  = (float)f->beat_phase / 65536.0f;
  const float conf = (float)f->tempo_confidence / 65536.0f;
  const float sil  = (float)f->tempo_silence / 65536.0f;

  if (debug_flags::enabled(debug_flags::kGroupTempoEnergy) && (now - last_timing) >= 7000u) {
    Serial.printf("Timing  : epoch=%lu | ready=%u | beat=%u\n",
                  static_cast<unsigned long>(f->audio_frame_epoch),
                  (unsigned)f->tempo_ready,
                  (unsigned)f->beat_flag);
    last_timing = now;
  }

  if (debug_flags::enabled(debug_flags::kGroupTempoEnergy) && (now - last_tempo) >= 7100u) {
    Serial.printf("Tempo   : bpm=%.1f | phase=%.2f\n", bpm, phs);
    last_tempo = now;
  }

  if (debug_flags::enabled(debug_flags::kGroupTempoEnergy) && (now - last_energy) >= 7200u) {
    Serial.printf("Energy  : strength=%.2f | conf=%.2f\n", str, conf);
    last_energy = now;
  }

  if (debug_flags::enabled(debug_flags::kGroupAPInput) && (now - last_input) >= 7300u) {
    Serial.printf("AP Input: silence=%.2f\n", sil);
    last_input = now;
  }
}

} // namespace debug_ui


#include "ap_profile.h"

#if AP_PROFILE_ENABLE

#include <Arduino.h>  // micros(), Serial
#include <string.h>

namespace aprof {

struct Slot {
  const char* tag;
  uint32_t acc_us;
  uint32_t max_us;
  uint32_t count;
  uint32_t t0;
};

static Slot s_slots[12];     // up to 12 tags
static uint32_t s_last_emit = 0;
static uint32_t s_interval  = 500;

static Slot* find(const char* tag) {
  for (auto& s : s_slots) if (s.tag && strcmp(s.tag, tag)==0) return &s;
  for (auto& s : s_slots) if (!s.tag) { s.tag = tag; return &s; }
  return &s_slots[0];
}

void init(uint32_t interval_ms){ s_interval = interval_ms ? interval_ms : 500; s_last_emit = millis(); }

void begin(const char* tag) { Slot* s = find(tag); s->t0 = micros(); }

void end(const char* tag) {
  Slot* s = find(tag);
  uint32_t dt = micros() - s->t0;
  s->acc_us += dt;
  if (dt > s->max_us) s->max_us = dt;
  s->count  += 1;
}

void tick() {
  uint32_t now = millis();
  if ((now - s_last_emit) < s_interval) return;
  s_last_emit = now;
  Serial.print("[aprof]");
  for (auto& s : s_slots) {
    if (!s.tag || s.count == 0) continue;
    uint32_t avg = s.acc_us / s.count;
    Serial.print(" "); Serial.print(s.tag); Serial.print("=");
    Serial.print(avg); Serial.print("/"); Serial.print(s.max_us); Serial.print("us");
    s.acc_us = s.max_us = s.count = 0;
  }
  Serial.println();
}

} // namespace aprof

#endif // AP_PROFILE_ENABLE

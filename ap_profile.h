#pragma once
#include <stdint.h>

/* ============================================================================
   ap_profile.h — ultra-light micro-profiler, OFF by default.
   Usage:
     AP_PROF_INIT(500 /* ms interval */);
     AP_PROF_BEGIN(Goertzel); ... AP_PROF_END(Goertzel);
     AP_PROF_TICK();  // once per frame to emit rate-limited stats
   Enable by defining AP_PROFILE_ENABLE 1 in build flags or before include.
   ==========================================================================*/

#ifndef AP_PROFILE_ENABLE
#define AP_PROFILE_ENABLE 0
#endif

#if AP_PROFILE_ENABLE
namespace aprof {
void init(uint32_t interval_ms);
void begin(const char* tag);
void end(const char* tag);
void tick();
}
#define AP_PROF_INIT(ms)  ::aprof::init(ms)
#define AP_PROF_BEGIN(tag) ::aprof::begin(#tag)
#define AP_PROF_END(tag)   ::aprof::end(#tag)
#define AP_PROF_TICK()     ::aprof::tick()
#else
#define AP_PROF_INIT(ms)   do{}while(0)
#define AP_PROF_BEGIN(tag) do{}while(0)
#define AP_PROF_END(tag)   do{}while(0)
#define AP_PROF_TICK()     do{}while(0)
#endif

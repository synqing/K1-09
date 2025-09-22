#ifndef FRAME_SYNC_H
#define FRAME_SYNC_H

#include <cstring>
#include "globals.h"

// Begin rendering a new LED frame. This increments the writer sequence and
// clears the working buffer so modes always start from a clean slate unless
// they explicitly seed from prior state.
inline void begin_frame()
{
  g_frame_seq_write++;
  std::memset(leds_16, 0, sizeof(CRGB16) * NATIVE_RESOLUTION);
}

// Publish the fully rendered frame so the LED consumer can safely process it.
inline void publish_frame()
{
  g_frame_seq_ready = g_frame_seq_write;
}

#endif // FRAME_SYNC_H

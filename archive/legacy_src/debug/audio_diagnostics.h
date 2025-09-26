#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>

namespace AudioDiagnostics {

struct ChunkStats {
  uint32_t timestamp_ms = 0;
  float max_raw = 0.0f;
  float max_processed = 0.0f;
  float rms = 0.0f;
  float vu_level = 0.0f;
  float silent_scale = 1.0f;
  bool silence = false;
  int8_t sweet_spot_state = 0;
  float agc_floor = 0.0f;
  float silence_threshold = 0.0f;
  float loud_threshold = 0.0f;
};

void setEnabled(bool enabled);
bool isEnabled();

void setDetailed(bool enabled);
bool isDetailed();

void setIntervalMs(uint32_t interval_ms);
uint32_t getIntervalMs();

void setWaveformLogging(bool enabled);
void requestSnapshot();

void onChunkCaptured(const ChunkStats& stats, const int16_t* waveform, size_t length);
void onSpectralUpdate(uint32_t timestamp_ms);

}  // namespace AudioDiagnostics

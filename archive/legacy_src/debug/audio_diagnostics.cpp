#include "audio_diagnostics.h"

#include "../globals.h"

#if ARDUINO_USB_CDC_ON_BOOT
  #define USBSerial Serial
#endif

#include <math.h>
#include <string.h>
#include <algorithm>

namespace AudioDiagnostics {
namespace {

constexpr size_t kMaxWaveformSamples = 1024;
constexpr size_t kBase64BufferSize = 4096;  // Enough for 1024 samples * 2 bytes -> 2048 bytes -> 2732 chars
constexpr size_t kLiteTopBins = 8;

bool g_enabled = false;
bool g_detailed_mode = false;
bool g_waveform_logging = false;
bool g_snapshot_requested = false;
bool g_waveform_snapshot_requested = false;
bool g_chunk_ready = false;

uint32_t g_emit_interval_ms = 500;
uint32_t g_last_emit_ms = 0;

ChunkStats g_last_stats;
size_t g_waveform_length = 0;
int16_t g_waveform_buffer[kMaxWaveformSamples] = {0};
char g_waveform_base64[kBase64BufferSize] = {0};

uint32_t g_last_warning_ms = 0;


inline float sanitize_float(float value, const char* label, size_t index = SIZE_MAX) {
  if (isnan(value) || isinf(value)) {
    uint32_t now = millis();
    if (now - g_last_warning_ms > 500) {
      g_last_warning_ms = now;
      if (index == SIZE_MAX) {
        USBSerial.printf("[AUDIO_DIAG] WARNING: %s is invalid (NaN/Inf)\n", label);
      } else {
        USBSerial.printf("[AUDIO_DIAG] WARNING: %s[%u] is invalid (NaN/Inf)\n", label, static_cast<unsigned>(index));
      }
    }
    return 0.0f;
  }
  return value;
}

inline double sanitize_double(double value, const char* label, size_t index = SIZE_MAX) {
  return static_cast<double>(sanitize_float(static_cast<float>(value), label, index));
}

template <typename T>
void print_array_as_floats(const T* data, size_t length, const char* label) {
  USBSerial.print('[');
  for (size_t i = 0; i < length; ++i) {
    if (i != 0) {
      USBSerial.print(',');
    }
    double value = sanitize_double(static_cast<double>(static_cast<float>(data[i])), label, i);
    USBSerial.printf("%.5f", value);
  }
  USBSerial.print(']');
}

size_t base64_encode(const uint8_t* input, size_t input_len, char* output, size_t output_capacity) {
  static const char kAlphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  if (input_len == 0) {
    if (output_capacity > 0) {
      output[0] = '\0';
    }
    return 0;
  }

  size_t required = ((input_len + 2) / 3) * 4;
  if (output_capacity <= required) {
    return 0;
  }

  size_t out_index = 0;
  size_t i = 0;
  while (i + 2 < input_len) {
    uint32_t triple = (static_cast<uint32_t>(input[i]) << 16) |
                      (static_cast<uint32_t>(input[i + 1]) << 8) |
                      static_cast<uint32_t>(input[i + 2]);

    output[out_index++] = kAlphabet[(triple >> 18) & 0x3F];
    output[out_index++] = kAlphabet[(triple >> 12) & 0x3F];
    output[out_index++] = kAlphabet[(triple >> 6) & 0x3F];
    output[out_index++] = kAlphabet[triple & 0x3F];
    i += 3;
  }

  size_t remaining = input_len - i;
  if (remaining) {
    uint32_t triple = static_cast<uint32_t>(input[i]) << 16;
    if (remaining == 2) {
      triple |= static_cast<uint32_t>(input[i + 1]) << 8;
    }

    output[out_index++] = kAlphabet[(triple >> 18) & 0x3F];
    output[out_index++] = kAlphabet[(triple >> 12) & 0x3F];

    if (remaining == 2) {
      output[out_index++] = kAlphabet[(triple >> 6) & 0x3F];
      output[out_index++] = '=';
    } else {  // remaining == 1
      output[out_index++] = '=';
      output[out_index++] = '=';
    }
  }

  output[out_index] = '\0';
  return out_index;
}

}  // namespace

static void emit_chunk(uint32_t timestamp_ms);

void setEnabled(bool enabled) {
  g_enabled = enabled;
  if (!enabled) {
    g_snapshot_requested = false;
    g_waveform_snapshot_requested = false;
  }
}

bool isEnabled() {
  return g_enabled;
}

void setDetailed(bool enabled) {
  g_detailed_mode = enabled;
}

bool isDetailed() {
  return g_detailed_mode;
}

void setIntervalMs(uint32_t interval_ms) {
  g_emit_interval_ms = interval_ms;
  if (g_emit_interval_ms == 0) {
    g_last_emit_ms = 0;
  }
}

uint32_t getIntervalMs() {
  return g_emit_interval_ms;
}

void setWaveformLogging(bool enabled) {
  g_waveform_logging = enabled;
  if (!enabled) {
    g_waveform_snapshot_requested = false;
  }
}

void requestSnapshot() {
  g_snapshot_requested = true;
  g_waveform_snapshot_requested = true;
}

void onChunkCaptured(const ChunkStats& stats, const int16_t* waveform, size_t length) {
  if (!(g_enabled || g_snapshot_requested || g_waveform_logging || g_waveform_snapshot_requested)) {
    return;
  }

  g_last_stats = stats;
  g_waveform_length = length > kMaxWaveformSamples ? kMaxWaveformSamples : length;
  if (waveform != nullptr && g_waveform_length > 0) {
    memcpy(g_waveform_buffer, waveform, g_waveform_length * sizeof(int16_t));
  }
  g_chunk_ready = true;
}

void onSpectralUpdate(uint32_t timestamp_ms) {
  if (!g_chunk_ready) {
    return;
  }

  const bool forced_emit = g_snapshot_requested || g_waveform_snapshot_requested;
  uint32_t now_ms = millis();
  if (!forced_emit && g_emit_interval_ms > 0) {
    uint32_t delta = now_ms - g_last_emit_ms;
    if (delta < g_emit_interval_ms) {
      g_chunk_ready = false;
      return;
    }
  }

  const bool emit_metrics = g_enabled || g_snapshot_requested;
  const bool include_waveform = g_waveform_logging || g_waveform_snapshot_requested;

  if (!(emit_metrics || include_waveform)) {
    g_chunk_ready = false;
    return;
  }

  BaseType_t taken = pdFALSE;
  if (serial_mutex != nullptr) {
    taken = xSemaphoreTake(serial_mutex, pdMS_TO_TICKS(2));
    if (taken != pdTRUE) {
      return;
    }
  }

  emit_chunk(timestamp_ms);

  if (taken == pdTRUE) {
    xSemaphoreGive(serial_mutex);
  }

  g_chunk_ready = false;
  g_snapshot_requested = false;
  g_waveform_snapshot_requested = false;

  if (g_emit_interval_ms > 0) {
    g_last_emit_ms = forced_emit ? millis() : now_ms;
  }
}

static void emit_chunk(uint32_t timestamp_ms) {
  const bool include_waveform = g_waveform_logging || g_waveform_snapshot_requested;

  USBSerial.print("{\"type\":\"audioChunk\"");
  USBSerial.printf(",\"timestampCapture\":%lu", static_cast<unsigned long>(g_last_stats.timestamp_ms));
  USBSerial.printf(",\"timestampProcess\":%lu", static_cast<unsigned long>(timestamp_ms));

  USBSerial.print(",\"stats\":{");
  USBSerial.printf("\"maxRaw\":%.2f", sanitize_double(g_last_stats.max_raw, "stats.maxRaw"));
  USBSerial.printf(",\"maxProcessed\":%.2f", sanitize_double(g_last_stats.max_processed, "stats.maxProcessed"));
  USBSerial.printf(",\"rms\":%.4f", sanitize_double(g_last_stats.rms, "stats.rms"));
  USBSerial.printf(",\"vuLevel\":%.4f", sanitize_double(g_last_stats.vu_level, "stats.vuLevel"));
  USBSerial.printf(",\"silentScale\":%.4f", sanitize_double(g_last_stats.silent_scale, "stats.silentScale"));
  USBSerial.print(",\"silence\":");
  USBSerial.print(g_last_stats.silence ? "true" : "false");
  USBSerial.printf(",\"sweetSpotState\":%d", static_cast<int>(g_last_stats.sweet_spot_state));
  USBSerial.printf(",\"agcFloor\":%.4f", sanitize_double(g_last_stats.agc_floor, "stats.agcFloor"));
  USBSerial.printf(",\"silenceThreshold\":%.4f", sanitize_double(g_last_stats.silence_threshold, "stats.silenceThreshold"));
  USBSerial.printf(",\"loudThreshold\":%.4f", sanitize_double(g_last_stats.loud_threshold, "stats.loudThreshold"));
  USBSerial.print('}');

  USBSerial.print(",\"globals\":{");
  USBSerial.printf("\"vuFloor\":%.4f", sanitize_double(CONFIG.VU_LEVEL_FLOOR, "globals.vuFloor"));
  USBSerial.printf(",\"waveformPeakScaled\":%.4f", sanitize_double(waveform_peak_scaled, "globals.waveformPeakScaled"));
  USBSerial.printf(",\"minSilentLevelTracker\":%.4f", sanitize_double(static_cast<float>(min_silent_level_tracker), "globals.minSilentLevelTracker"));
  USBSerial.printf(",\"currentPunch\":%.4f", sanitize_double(current_punch, "globals.currentPunch"));
  USBSerial.printf(",\"audioVu\":%.4f", sanitize_double(static_cast<float>(audio_vu_level), "globals.audioVu"));
  USBSerial.printf(",\"audioVuAvg\":%.4f", sanitize_double(static_cast<float>(audio_vu_level_average), "globals.audioVuAvg"));
  USBSerial.print('}');

  USBSerial.print(",\"config\":{");
  USBSerial.printf("\"sampleRate\":%u", CONFIG.SAMPLE_RATE);
  USBSerial.printf(",\"samplesPerChunk\":%u", CONFIG.SAMPLES_PER_CHUNK);
  USBSerial.printf(",\"sensitivity\":%.4f", sanitize_double(CONFIG.SENSITIVITY, "config.sensitivity"));
  USBSerial.printf(",\"dcOffset\":%ld", static_cast<long>(CONFIG.DC_OFFSET));
  USBSerial.printf(",\"sweetSpotMin\":%u", CONFIG.SWEET_SPOT_MIN_LEVEL);
  USBSerial.printf(",\"sweetSpotMax\":%u", CONFIG.SWEET_SPOT_MAX_LEVEL);
  USBSerial.printf(",\"sweetSpotState\":%.2f", sanitize_double(sweet_spot_state, "config.sweetSpotState"));
  USBSerial.print('}');

  USBSerial.print(",\"noise\":{");
  USBSerial.print("\"complete\":");
  USBSerial.print(noise_complete ? "true" : "false");
  USBSerial.printf(",\"iterations\":%u", noise_iterations);
  if (g_detailed_mode) {
    USBSerial.print(",\"samples\":");
    print_array_as_floats(noise_samples, NUM_FREQS, "noise.samples");
  }
  USBSerial.print('}');

  if (g_detailed_mode) {
    USBSerial.print(",\"spectral\":{");
    USBSerial.print("\"spectrogram\":");
    print_array_as_floats(spectrogram, NUM_FREQS, "spectral.spectrogram");
    USBSerial.print(",\"spectrogramSmooth\":");
    print_array_as_floats(spectrogram_smooth, NUM_FREQS, "spectral.spectrogramSmooth");
    USBSerial.print(",\"chromagram\":");
    print_array_as_floats(chromagram_smooth, 12, "spectral.chromagram");
    USBSerial.print(",\"noveltyCurve\":");
    print_array_as_floats(novelty_curve, SPECTRAL_HISTORY_LENGTH, "spectral.noveltyCurve");
    USBSerial.printf(",\"historyIndex\":%u", spectral_history_index);
    USBSerial.print('}');
  } else {
    struct TopBin { uint16_t index; float magnitude; };
    TopBin bins[NUM_FREQS];
    size_t total_bins = 0;
    for (uint16_t i = 0; i < NUM_FREQS; ++i) {
      bins[total_bins++] = {i, static_cast<float>(spectrogram[i])};
    }
    std::sort(bins, bins + total_bins, [](const TopBin& a, const TopBin& b) {
      return a.magnitude > b.magnitude;
    });
    size_t top_count = std::min(kLiteTopBins, total_bins);
    uint8_t chroma_index = 0;
    float chroma_value = 0.0f;
    for (uint8_t i = 0; i < 12; ++i) {
      float value = static_cast<float>(chromagram_smooth[i]);
      if (value > chroma_value) {
        chroma_value = value;
        chroma_index = i;
      }
    }
    uint16_t latest_index = (spectral_history_index + SPECTRAL_HISTORY_LENGTH - 1) % SPECTRAL_HISTORY_LENGTH;
    float novelty = static_cast<float>(novelty_curve[latest_index]);

    USBSerial.print(",\"spectral\":{");
    USBSerial.print("\"top\":[");
    for (size_t i = 0; i < top_count && i < total_bins; ++i) {
      if (i != 0) {
        USBSerial.print(',');
      }
      const auto& bin = bins[i];
      double mag = sanitize_double(bin.magnitude, "spectral.topMag", i);
      double hz = sanitize_double(frequencies[bin.index].target_freq, "spectral.topHz", i);
      USBSerial.print('{');
      USBSerial.print("\"bin\":");
      USBSerial.print(bin.index);
      USBSerial.print(",\"hz\":");
      USBSerial.printf("%.2f", hz);
      USBSerial.print(",\"mag\":");
      USBSerial.printf("%.5f", mag);
      USBSerial.print('}');
    }
    USBSerial.print(']');
    USBSerial.printf(",\"chromagramPeak\":{\"index\":%u,\"value\":%.5f}", static_cast<unsigned>(chroma_index), sanitize_double(chroma_value, "spectral.chromagramPeak"));
    USBSerial.printf(",\"novelty\":%.5f", sanitize_double(novelty, "spectral.novelty"));
    USBSerial.print('}');
  }

  if (include_waveform && g_waveform_length > 0) {
    size_t bytes_to_encode = g_waveform_length * sizeof(int16_t);
    const uint8_t* raw_bytes = reinterpret_cast<const uint8_t*>(g_waveform_buffer);
    size_t encoded = base64_encode(raw_bytes, bytes_to_encode, g_waveform_base64, kBase64BufferSize);
    if (encoded == 0) {
      USBSerial.print(",\"waveform\":\"\"");
    } else {
      USBSerial.print(",\"waveform\":\"");
      USBSerial.print(g_waveform_base64);
      USBSerial.print("\"");
    }
  }

  USBSerial.println('}');
}

}  // namespace AudioDiagnostics

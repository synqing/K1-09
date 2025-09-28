// SPH0645 reader with DC preserved +
// AC_RMS_dBFS + DC %FS + calibration delta + quantization step check
// + DRIFT tracker/alerts + rail detector + quiet-epoch DC stability
// + NVS persistence of calibration + optional CSV snapshot (debug_mode)

#include "sph0645.h"
#include "audio_config.h"
#include "debug/performance_trace.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <Arduino.h>
#if ESP_IDF_VERSION_MAJOR >= 5
#include "driver/i2s_std.h"
#else
#include "driver/i2s.h"
#endif
#include "driver/gpio.h"
#include <math.h>
#include <Preferences.h>   // NVS persistence for per-unit calibration
#include "esp_idf_version.h"
#include "debug/debug_flags.h"

namespace K1Lightwave {
namespace Audio {
namespace Sph0645 {

namespace {
float g_i2s_rate_hz = 0.0f;
AudioFrameTracer g_audio_tracer;
TaskHandle_t g_trace_task = nullptr;

// Latest-only queue item
struct CaptureChunk {
  uint32_t enqueued_us;
  uint16_t flags;
  uint16_t _pad;
  int32_t  samples[chunk_size];
};

static QueueHandle_t g_capture_queue = nullptr;  // depth=1 latest-only
static TaskHandle_t  g_capture_task  = nullptr;
}

// -------------------- External flags (used elsewhere in your system) --------------------
extern bool debug_mode;  // If true, we emit an extra CSV line each second
bool debug_mode __attribute__((weak)) = false;

// ANSI color accents for grouped telemetry (safe if monitor supports ANSI).
static constexpr const char* kColorInput = "\x1b[96m";    // bright cyan
static constexpr const char* kColorDC    = "\x1b[95m";    // bright magenta
static constexpr const char* kColorDrift = "\x1b[38;5;205m";  // pink
static constexpr const char* kColorAC    = "\x1b[38;5;208m";  // orange
static constexpr const char* kColorDiag  = "\x1b[38;5;177m";  // light purple
static constexpr const char* kColorWhite = "\x1b[37m";
static constexpr const char* kColorReset = "\x1b[0m";

// -------------------- I2S port selection --------------------
static const i2s_port_t I2S_PORT = I2S_NUM_0;
#if ESP_IDF_VERSION_MAJOR >= 5
static i2s_chan_handle_t g_i2s_rx_channel = nullptr;
static int32_t g_dma_buffer[chunk_size];
#endif

// -------------------- DC estimators (preserve DC in data path) --------------------
static constexpr size_t kDCWindow = 1024;           // power-of-two moving-average window
static int32_t  dc_ring[kDCWindow] = {0};
static size_t   dc_idx = 0;
static bool     dc_full = false;
static int64_t  dc_accum = 0;
static int32_t  dc_est_q24 = 0;

static int64_t  sec_sum   = 0;     // sum of samples in the last 1 s
static uint32_t sec_count = 0;     // number of samples in the last 1 s
static int32_t  dc_1s_q24 = 0;

// -------------------- AC RMS + quant step telemetry + calibration delta --------------------
static long double sec_sqsum = 0.0L;       // sum of squares for variance/RMS
static uint32_t    offgrid_count = 0;      // samples not multiple of 64 after >>8
static const double Q24_FS = 8388607.0;    // full-scale magnitude for signed 24-bit (2^23-1)

// Sensitivity line and assumed test SPL for 1 kHz tone
static const double kSensitivity_dBFS_at_94dB = -26.0; // SPH0645 typical @ 94 dB SPL
static double       kCalToneSPL_dB = 71.0;             // set to your test SPL (dB(A) ~ dB SPL @ 1 kHz)

// ---- Drift tracking (counts and %FS) ----
static const double DRIFT_TAU_SEC = 90.0;
static const double DRIFT_ALPHA   = 1.0 - exp(-1.0 / DRIFT_TAU_SEC);

static bool   drift_ema_inited   = false;
static double dc_ema_q24_d       = 0.0;

static bool   drift_baseline_set = false;
static double dc_baseline_q24_d  = 0.0;
static uint32_t baseline_age_sec = 0;

static uint32_t uptime_sec       = 0;
static uint32_t last_rebase_sec  = 0;

// Quiet detection for safe rebase
static uint32_t quiet_secs       = 0;
static const double QUIET_DBFS   = -70.0;
static const uint32_t QUIET_HOLD = 5;
static const uint32_t REBASE_COOLDOWN = 60;

// Alert thresholds (policy)
static const double ALERT_ABS_DC_PCT = 20.0;
static const double ALERT_DRIFT_PCT  = 2.0;

// -------------------- Saturation / rail detector --------------------
static const double kRailPctFS = 99.0;             // within 1%FS considered near-rail
static uint32_t rail_count = 0;                    // samples near rails in the last second

// -------------------- Per-unit NVS persistence for calibration --------------------
static Preferences prefs;
static const char* kNvsNS  = "sph0645";
static const char* kKeyCal = "cal_db";
static const char* kKeySPL = "cal_spl";
static bool   nvs_ready = false;
static double nvs_cal_db = NAN;
static double nvs_cal_spl = NAN;
static const bool kAutoPersistCal = true;          // overwrite when meaningful change
static const double kPersistEps_dB = 0.10;         // write to NVS if |Δ| >= 0.1 dB

// -------------------- Print cadence --------------------
static unsigned long lastPrint = 0;
static const unsigned long printInterval = 7000;   // 7 s

// Raw extrema (post-unpack) for human diagnostics
static int32_t min_sample = INT32_MAX;
static int32_t max_sample = INT32_MIN;

// Forward declarations for shared sample processing/summary helpers
static void maybe_emit_summary();
static inline void process_sample_q24(int32_t s_q24) {
  const int32_t old = dc_ring[dc_idx];
  dc_ring[dc_idx] = s_q24;
  dc_idx = (dc_idx + 1) & (kDCWindow - 1);
  dc_accum += static_cast<int64_t>(s_q24) - static_cast<int64_t>(old);
  if (!dc_full && dc_idx == 0) dc_full = true;
  const size_t denom = dc_full ? kDCWindow : (dc_idx ? dc_idx : 1);
  dc_est_q24 = static_cast<int32_t>(dc_accum / static_cast<int64_t>(denom));

  const double rail_threshold = Q24_FS * (kRailPctFS / 100.0);
  if (static_cast<double>(llabs(static_cast<long long>(s_q24))) >= rail_threshold) {
    rail_count++;
  }

  sec_sum   += s_q24;
  sec_sqsum += static_cast<long double>(s_q24) * static_cast<long double>(s_q24);

  if ((s_q24 & 0x3F) != 0) {
    offgrid_count++;
  }

  sec_count++;

  if (s_q24 < min_sample) min_sample = s_q24;
  if (s_q24 > max_sample) max_sample = s_q24;

  maybe_emit_summary();
}

// -------------------- Accessors (public API) --------------------
int32_t read_dc_offset_q24()    { return dc_est_q24; }
int32_t read_dc_offset_1s_q24() { return dc_1s_q24; }

// -------------------- Helpers --------------------
static inline double pctFS_from_q24(double q24) {
  return (Q24_FS > 0.0) ? (100.0 * fabs(q24) / Q24_FS) : 0.0;
}

// -------------------- Setup --------------------
void setup() {
  Serial.begin(921600);

#if ESP_IDF_VERSION_MAJOR >= 5
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_PORT, I2S_ROLE_MASTER);
  chan_cfg.auto_clear = true;
  chan_cfg.dma_desc_num = 2;             // <=16ms reservoir
  chan_cfg.dma_frame_num = chunk_size;

  esp_err_t err = i2s_new_channel(&chan_cfg, nullptr, &g_i2s_rx_channel);
  if (err != ESP_OK || !g_i2s_rx_channel) {
    Serial.printf("i2s_new_channel err=%d\n", err);
    while (true) { delay(10); }
  }

  i2s_std_config_t std_cfg = {};
  std_cfg.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(audio_sample_rate);
  std_cfg.slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO);
  std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;

  std_cfg.gpio_cfg = {
      .mclk = I2S_GPIO_UNUSED,
      .bclk = static_cast<gpio_num_t>(7),
      .ws   = static_cast<gpio_num_t>(13),
      .dout = I2S_GPIO_UNUSED,
      .din  = static_cast<gpio_num_t>(8),
      .invert_flags = {
          .mclk_inv = false,
          .bclk_inv = false,
          .ws_inv   = false,
      },
  };
  std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
  std_cfg.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT;
  std_cfg.slot_cfg.ws_width = I2S_SLOT_BIT_WIDTH_32BIT;

  err = i2s_channel_init_std_mode(g_i2s_rx_channel, &std_cfg);
  if (err != ESP_OK) {
    Serial.printf("i2s_channel_init_std_mode err=%d\n", err);
    while (true) { delay(10); }
  }

  err = i2s_channel_enable(g_i2s_rx_channel);
  if (err != ESP_OK) {
    Serial.printf("i2s_channel_enable err=%d\n", err);
    while (true) { delay(10); }
  }

  g_i2s_rate_hz = static_cast<float>(audio_sample_rate);
  Serial.printf("I2S clk => rate=%uHz (std)\n", static_cast<unsigned>(audio_sample_rate));

#else
  // Legacy I2S configuration (Arduino core 2.0.9 compatible)
  i2s_config_t i2s_config = {};
  i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
  i2s_config.sample_rate = audio_sample_rate;             // WS == sample rate
  i2s_config.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT; // 32-bit slot; 24-bit payload
  i2s_config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;  // left slot
  i2s_config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  i2s_config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  i2s_config.dma_buf_count = 8;
  i2s_config.dma_buf_len   = chunk_size * 2;  // larger ring per legacy baseline
  i2s_config.use_apll = false;
#if ESP_IDF_VERSION_MAJOR >= 5
  i2s_config.tx_desc_auto_clear = false;
#endif
  i2s_config.fixed_mclk = 0;

  i2s_pin_config_t pin_config = {};
  pin_config.bck_io_num   = 7;     // BCLK
  pin_config.ws_io_num    = 13;    // LRCLK
  pin_config.data_out_num = I2S_PIN_NO_CHANGE;
  pin_config.data_in_num  = 8;     // SD
#if ESP_IDF_VERSION_MAJOR >= 5
  pin_config.mck_io_num   = I2S_PIN_NO_CHANGE;
#endif

  esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (err != ESP_OK) { Serial.printf("I2S install err=%d\n", err); while (true) { delay(10); } }
  err = i2s_set_pin(I2S_PORT, &pin_config);
  if (err != ESP_OK) { Serial.printf("I2S set_pin err=%d\n", err); while (true) { delay(10); } }
  i2s_zero_dma_buffer(I2S_PORT);

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  const float actual_rate = i2s_get_clk(I2S_PORT);
  Serial.printf("I2S clk => rate=%.2fHz (std)\n", static_cast<double>(actual_rate));
  g_i2s_rate_hz = actual_rate;
#else
  uint32_t actual_rate = 0;
  i2s_bits_per_sample_t actual_bits = I2S_BITS_PER_SAMPLE_32BIT;
  i2s_channel_t actual_channels = I2S_CHANNEL_MONO;
  if (i2s_get_clk(I2S_PORT, &actual_rate, &actual_bits, &actual_channels) == ESP_OK) {
    Serial.printf("I2S clk => rate=%uHz bits=%u channels=%u\n",
                  static_cast<unsigned>(actual_rate),
                  static_cast<unsigned>(actual_bits),
                  static_cast<unsigned>(actual_channels));
    g_i2s_rate_hz = static_cast<float>(actual_rate);
  }
#endif

#endif  // ESP_IDF_VERSION_MAJOR >= 5

  // Latest-only capture queue (depth = 1)
  if (!g_capture_queue) {
    g_capture_queue = xQueueCreate(1, sizeof(CaptureChunk));
    if (!g_capture_queue) {
      Serial.println("[sph0645] FATAL: capture queue allocation failed");
      while (true) { delay(10); }
    }
  }

  // High-priority capture task on same core as Arduino loop (core=1)
  if (!g_capture_task) {
    const BaseType_t ok = xTaskCreatePinnedToCore(
        [](void*) {
          const size_t bytes_per_chunk = chunk_size * sizeof(int32_t);
          for (;;) {
            size_t bytes_read = 0;
            g_audio_tracer.start_i2s();
            const uint32_t t0 = micros();
#if ESP_IDF_VERSION_MAJOR >= 5
            const esp_err_t err = i2s_channel_read(g_i2s_rx_channel,
                                                   g_dma_buffer,
                                                   bytes_per_chunk,
                                                   &bytes_read,
                                                   portMAX_DELAY);
#else
            const esp_err_t err = i2s_read(I2S_PORT,
                                           g_dma_buffer,
                                           bytes_per_chunk,
                                           &bytes_read,
                                           portMAX_DELAY);
#endif
            const uint32_t elapsed_us = micros() - t0;
            g_audio_tracer.end_i2s(elapsed_us, static_cast<uint32_t>(bytes_read));
            if (err != ESP_OK || bytes_read != bytes_per_chunk) {
              TRACE_ERROR(ERROR_I2S_TIMEOUT,
                          ((err & 0xFFFF) << 16) | static_cast<uint32_t>(bytes_read & 0xFFFF));
              continue;
            }

            // Latest-only publish (overwrite slot)
            CaptureChunk chunk{};
            chunk.enqueued_us = micros();
            chunk.flags = 0;
            for (size_t i = 0; i < chunk_size; ++i) {
              chunk.samples[i] = g_dma_buffer[i] >> 8;
            }
            (void)xQueueOverwrite(g_capture_queue, &chunk);
          }
        },
        "sph0645_capture",
        4096,
        nullptr,
        tskIDLE_PRIORITY + 18,
        &g_capture_task,
        1);
    if (ok != pdPASS) {
      Serial.println("[sph0645] FATAL: capture task creation failed");
      while (true) { delay(100); }
    }
  }

  init_performance_trace(TRACE_CAT_AUDIO | TRACE_CAT_I2S | TRACE_CAT_PERF |
                         TRACE_CAT_ERROR | TRACE_CAT_CRITICAL);
  if (!g_trace_task) {
    xTaskCreatePinnedToCore(trace_consumer_task, "trace_consumer", 2048, nullptr, 1,
                            &g_trace_task, 1);
    export_trace_buffer_serial();
  }

  // NVS init (per-unit calibration persistence)
  nvs_ready = prefs.begin(kNvsNS, false); // RW
  if (nvs_ready) {
    nvs_cal_db  = prefs.getDouble(kKeyCal, NAN);
    nvs_cal_spl = prefs.getDouble(kKeySPL, NAN);
    Serial.print("NVS calib: cal_db=");
    if (isnan(nvs_cal_db)) Serial.print("NaN"); else Serial.print(nvs_cal_db, 2);
    Serial.print(" dB, cal_spl=");
    if (isnan(nvs_cal_spl)) Serial.println("NaN"); else Serial.println(nvs_cal_spl, 1);
  } else {
    Serial.println("NVS init failed; calibration will not persist.");
  }

  Serial.println("I2S driver installed.");
}

// -------------------- Loop --------------------
void loop() {
  // no-op; capture handled by dedicated task
}

// -------------------- Shared once-per-second summary --------------------
static void maybe_emit_summary() {
  const unsigned long now = millis();
  if (now - lastPrint < printInterval) {
    return;
  }
  lastPrint = now;
  uptime_sec++;

  dc_1s_q24 = (sec_count > 0) ? static_cast<int32_t>(sec_sum / static_cast<long double>(sec_count)) : 0;

  long double mean = (sec_count > 0) ? (static_cast<long double>(sec_sum) / static_cast<long double>(sec_count)) : 0.0L;
  long double var  = (sec_count > 0) ? (sec_sqsum / static_cast<long double>(sec_count) - mean * mean) : 0.0L;
  if (var < 0) var = 0;
  double ac_rms_counts = sqrt(static_cast<double>(var));
  double ac_rms_dbfs   = (ac_rms_counts > 0.0) ? 20.0 * log10(ac_rms_counts / Q24_FS) : -INFINITY;

  double qstep_ok = (sec_count > 0)
                        ? 100.0 * (1.0 - (static_cast<double>(offgrid_count) / static_cast<double>(sec_count)))
                        : 100.0;

  // EMA of DC(1s)
  const double dc_1s_q24_d = static_cast<double>(dc_1s_q24);
  if (!drift_ema_inited) {
    dc_ema_q24_d     = dc_1s_q24_d;
    drift_ema_inited = true;
  } else {
    dc_ema_q24_d += DRIFT_ALPHA * (dc_1s_q24_d - dc_ema_q24_d);
  }

  if (!drift_baseline_set && uptime_sec >= 10) {
    dc_baseline_q24_d = dc_ema_q24_d;
    drift_baseline_set = true;
    baseline_age_sec   = 0;
    last_rebase_sec    = uptime_sec;
  } else if (drift_baseline_set) {
    baseline_age_sec++;
  }

  double drift_counts = dc_ema_q24_d - dc_baseline_q24_d;
  double drift_pct    = 100.0 * fabs(drift_counts) / Q24_FS;

  // Quiet tracking
  if (ac_rms_dbfs < QUIET_DBFS) {
    if (quiet_secs < 0xFFFF'FFFFu) {
      quiet_secs++;
    }
  } else {
    quiet_secs = 0;
  }

  bool pending_rebase = (quiet_secs >= QUIET_HOLD) && (drift_pct > ALERT_DRIFT_PCT) &&
                        (uptime_sec - last_rebase_sec >= REBASE_COOLDOWN) && drift_baseline_set;

  const char* rebase_note = "";
  if (pending_rebase) {
    dc_baseline_q24_d = dc_ema_q24_d;
    baseline_age_sec  = 0;
    last_rebase_sec   = uptime_sec;
    rebase_note = " [REBASED]";
  } else if (quiet_secs >= QUIET_HOLD && drift_pct > ALERT_DRIFT_PCT) {
    rebase_note = " [REBASING CANDIDATE]";
  }

  double dc_pct_fs = 100.0 * fabs(static_cast<double>(dc_1s_q24)) / Q24_FS;
  double clip_rate_pct = (sec_count > 0)
                             ? 100.0 * (static_cast<double>(rail_count) / static_cast<double>(sec_count))
                             : 0.0;

  // Calibration delta (unchanged policy)
  double predicted_dbfs = kSensitivity_dBFS_at_94dB + (kCalToneSPL_dB - 94.0);
  double cal_db = predicted_dbfs - ac_rms_dbfs;

  if (nvs_ready && kAutoPersistCal && isfinite(cal_db)) {
    bool need_write = false;
    if (isnan(nvs_cal_db) || fabs(nvs_cal_db - cal_db) >= kPersistEps_dB) {
      nvs_cal_db = cal_db;
      need_write = true;
    }
    if (isnan(nvs_cal_spl) || fabs(nvs_cal_spl - kCalToneSPL_dB) >= 0.01) {
      nvs_cal_spl = kCalToneSPL_dB;
      need_write = true;
    }
    if (need_write) {
      prefs.putDouble(kKeyCal, nvs_cal_db);
      prefs.putDouble(kKeySPL, nvs_cal_spl);
    }
  }

  const char* rebase_note_clean = (*rebase_note == ' ') ? (rebase_note + 1) : rebase_note;

  if (debug_flags::enabled(debug_flags::kGroupAPInput)) {
    Serial.printf("%sAP Input%s : %sMin=%ld | Max=%ld | QstepOK=%.1f%% | Clip=%.3f%%%s\n",
                  kColorInput, kColorWhite, kColorWhite,
                  (long)min_sample, (long)max_sample, qstep_ok, clip_rate_pct, kColorReset);
  }

  if (debug_flags::enabled(debug_flags::kGroupDCAndDrift)) {
    Serial.printf("%sDC Stats%s : %sWin=%u -> %ld | 1s=%ld | %%FS=%.2f%s\n",
                  kColorDC, kColorWhite, kColorWhite,
                  static_cast<unsigned>(kDCWindow), (long)dc_est_q24, (long)dc_1s_q24, dc_pct_fs, kColorReset);

    Serial.printf("%sDrift%s    : %sEMA=%.0f | Cnt=%.0f | %%FS=%.2f | Age=%lus%s\n",
                  kColorDrift, kColorWhite, kColorWhite,
                  dc_ema_q24_d, drift_counts, drift_pct, (unsigned long)baseline_age_sec, kColorReset);
  }

  if (debug_flags::enabled(debug_flags::kGroupAPInput)) {
    Serial.printf("%sAC/Cal%s   : %sRMS=%0.0f | dBFS=%.1f | CAL=%+.2f%s\n",
                  kColorAC, kColorWhite, kColorWhite,
                  ac_rms_counts, ac_rms_dbfs, cal_db, kColorReset);
  }

  if (debug_flags::enabled(debug_flags::kGroupDCAndDrift)) {
    Serial.printf("%sDiagnostics%s : %sQuiet=%lus | Note=%s%s\n",
                  kColorDiag, kColorWhite, kColorWhite,
                  (unsigned long)quiet_secs, rebase_note_clean, kColorReset);
  }

  if (debug_mode) {
    Serial.printf("CSV,%lu,%ld,%.3f,%.2f,%.2f,%.3f,%.3f,%lu,%.0f,%.2f,%lu,%+.2f,%.1f\n",
                  static_cast<unsigned long>(uptime_sec),
                  (long)dc_1s_q24,
                  dc_pct_fs,
                  ac_rms_dbfs,
                  qstep_ok,
                  clip_rate_pct,
                  drift_pct,
                  (unsigned long)quiet_secs,
                  drift_counts,
                  drift_pct,
                  (unsigned long)baseline_age_sec,
                  cal_db,
                  kCalToneSPL_dB);
  }

  min_sample = INT32_MAX;
  max_sample = INT32_MIN;
  sec_sum = 0;
  sec_sqsum = 0.0L;
  sec_count = 0;
  offgrid_count = 0;
  rail_count = 0;
}

// -------------------- Bulk chunk reader --------------------
bool read_q24_chunk(int32_t* out_q24, size_t n) {
  if (!out_q24 || n == 0 || n > chunk_size || !g_capture_queue) {
    return false;
  }

  CaptureChunk chunk{};
  if (xQueueReceive(g_capture_queue, &chunk, portMAX_DELAY) != pdTRUE) {
    TRACE_ERROR(ERROR_I2S_TIMEOUT, 0);
    return false;
  }
  // Optional: single flush to prefer newest
  CaptureChunk newer{};
  if (xQueueReceive(g_capture_queue, &newer, 0) == pdTRUE) {
    chunk = newer;
  }

  for (size_t i = 0; i < n; ++i) {
    out_q24[i] = chunk.samples[i];
    process_sample_q24(out_q24[i]);
  }
  return true;
}

} // namespace Sph0645
} // namespace Audio
} // namespace K1Lightwave

/*
DC(%FS): SPH0645 typical DC ≈ 5%FS. Alerts fire at 20%FS or 2%FS drift
AC_RMS(dBFS): For 1 kHz @ 71 dB SPL, prediction is −49 dBFS
Clip(%): Keep at 0.
QstepOK: ~100%
DC(win1024) responds fast (sub-frame changes).
DC(1s) is stable and forms the basis for drift, %FS, and quiet-epoch checks.
Using both gives responsiveness and noise immunity without touching the stream.

*/

float K1Lightwave::Audio::Sph0645::read_sample_rate_hz() {
  return g_i2s_rate_hz;
}

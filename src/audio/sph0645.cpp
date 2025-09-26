#include "sph0645.h"
#include <Arduino.h>
#include "driver/i2s.h"

namespace K1Lightwave {
namespace Audio {
namespace Sph0645 {

// ---- Static module state ----
static bool     s_inited     = false;
static bool     s_useAPLL    = false;
static int32_t  s_minSample  = INT32_MAX;
static int32_t  s_maxSample  = INT32_MIN;
static uint32_t s_lastCount  = 0;
static int32_t  s_chunk[kChunkSize];   // 128 x 32-bit words
static unsigned long s_lastPrintMs = 0;

// Forward
static bool reinit();

// Public API --------------------------------------------------------------

bool setup() {
  if (s_inited) return true;
  return reinit();
}

void setUseAPLL(bool on) {
  if (s_useAPLL == on && s_inited) return;
  s_useAPLL = on;
  reinit(); // re-install driver with updated APLL choice
}

void resetMinMax() {
  s_minSample = INT32_MAX;
  s_maxSample = INT32_MIN;
}

void getMinMax(int32_t& mn, int32_t& mx) {
  mn = s_minSample;
  mx = s_maxSample;
}

const int32_t* lastChunk() { return s_chunk; }
uint32_t       lastCount() { return s_lastCount; }

void loop() {
  if (!s_inited) return;

  // Read exactly kChunkSize 32-bit words (blocking)
  size_t bytesRead = 0;
  size_t wantBytes = kChunkSize * sizeof(int32_t);
  size_t gotTotal  = 0;

  while (gotTotal < wantBytes) {
    size_t got = 0;
    esp_err_t err = i2s_read((i2s_port_t)kI2SPort,
                             ((uint8_t*)s_chunk) + gotTotal,
                             wantBytes - gotTotal,
                             &got,
                             portMAX_DELAY);
    if (err != ESP_OK) {
      // Don’t spam; brief note then bail
      static unsigned long lastErr = 0;
      unsigned long now = millis();
      if (now - lastErr > 1000) {
        Serial.printf("i2s_read err=%d\n", (int)err);
        lastErr = now;
      }
      return;
    }
    gotTotal += got;
  }

  s_lastCount = kChunkSize;

  // Convert 24-bit-in-32 layout to 24-bit signed by shifting (>> 8)
  // Update min/max on the shifted values
  for (uint32_t i = 0; i < s_lastCount; ++i) {
    int32_t v = s_chunk[i] >> 8;
    if (v < s_minSample) s_minSample = v;
    if (v > s_maxSample) s_maxSample = v;
  }

  // Print once per second to match your expectations
  unsigned long now = millis();
  if (now - s_lastPrintMs >= 1000) {
    Serial.printf("Min sample: %ld, Max sample: %ld\n",
                  (long)s_minSample, (long)s_maxSample);
    resetMinMax();
    s_lastPrintMs = now;
  }
}

// Internals ---------------------------------------------------------------

static bool reinit() {
  // If already installed, uninstall first
  if (s_inited) {
    i2s_driver_uninstall((i2s_port_t)kI2SPort);
    s_inited = false;
    delay(10);
  }

  // I2S configuration (16 kHz, 32-bit samples, left-only, 128-sample DMA buf)
  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = kSampleRate,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = (int)kChunkSize,  // 128 samples per DMA buffer
    .use_apll = s_useAPLL,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  // Pins (your exact GPIOs)
  i2s_pin_config_t pins = {
    .bck_io_num   = kPinBCLK,
    .ws_io_num    = kPinLRCK,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num  = kPinDIN
  };

  esp_err_t err = i2s_driver_install((i2s_port_t)kI2SPort, &cfg, 0, nullptr);
  if (err != ESP_OK) {
    Serial.printf("i2s_driver_install failed: %d\n", (int)err);
    return false;
  }

  err = i2s_set_pin((i2s_port_t)kI2SPort, &pins);
  if (err != ESP_OK) {
    Serial.printf("i2s_set_pin failed: %d\n", (int)err);
    i2s_driver_uninstall((i2s_port_t)kI2SPort);
    return false;
  }

  // Force the peripheral to the exact sample rate again (some SDKs do this)
  i2s_set_clk((i2s_port_t)kI2SPort, kSampleRate,
              I2S_BITS_PER_SAMPLE_32BIT, I2S_CHANNEL_MONO);

  resetMinMax();
  s_lastPrintMs = millis();
  s_inited = true;

  Serial.printf("I2S ready @ %d Hz, chunk=%u, APLL=%s\n",
                kSampleRate, (unsigned)kChunkSize, s_useAPLL ? "on" : "off");
  return true;
}

} // namespace Sph0645
} // namespace Audio
} // namespace K1Lightwave

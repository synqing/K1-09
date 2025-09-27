#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "driver/gpio.h"
#include "driver/i2s.h"
#include "freertos/FreeRTOS.h"

namespace SensoryBridge::Audio {

struct Sph0645Pins {
    gpio_num_t bclk = GPIO_NUM_NC;
    gpio_num_t ws = GPIO_NUM_NC;
    gpio_num_t din = GPIO_NUM_NC;
};

struct Sph0645Config {
    i2s_port_t port = I2S_NUM_0;
    int sample_rate = 44'100;                // Hz
    size_t samples_per_frame = 64;           // Samples we expect per read
    size_t dma_buffer_count = 4;             // DMA buffers for i2s_driver_install
    size_t dma_buffer_len = 64;              // Samples per DMA buffer
    Sph0645Pins pins = {};
    bool use_apll = false;                   // Set true when low-jitter clock required
    int intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
};

struct FrameStats {
    int16_t min_value = std::numeric_limits<int16_t>::max();
    int16_t max_value = std::numeric_limits<int16_t>::min();

    inline void reset() {
        min_value = std::numeric_limits<int16_t>::max();
        max_value = std::numeric_limits<int16_t>::min();
    }

    inline void ingest(int16_t sample) {
        if (sample < min_value) {
            min_value = sample;
        }
        if (sample > max_value) {
            max_value = sample;
        }
    }
};

class Sph0645Capture {
public:
    explicit Sph0645Capture(const Sph0645Config &config);
    ~Sph0645Capture();

    esp_err_t begin();
    void end();

    bool isRunning() const { return running_; }
    size_t frameSize() const { return config_.samples_per_frame; }
    const Sph0645Config &config() const { return config_; }

    esp_err_t readFrame(int16_t *dest,
                        size_t sample_count,
                        FrameStats *stats = nullptr,
                        float gain = 1.0f,
                        int32_t dc_offset = 0,
                        TickType_t timeout = portMAX_DELAY);

    static int16_t convertRawSample(int32_t raw_sample, float gain, int32_t dc_offset);

private:
    Sph0645Config config_;
    bool running_ = false;
    std::vector<int32_t> raw_buffer_;
};

}  // namespace SensoryBridge::Audio

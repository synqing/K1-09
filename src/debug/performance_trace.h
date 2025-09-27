#pragma once

// Performance-Optimized Trace System for K1-07 SensoryBridge
// Minimal overhead implementation maintaining real-time constraints
// Author: Performance Engineering Team
// Date: 2025-09-17

#ifndef DEBUG_BUILD
#define DEBUG_BUILD 1
#endif

#include <Arduino.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#define TRACE_BUFFER_SIZE 1024
#define TRACE_QUEUE_SIZE 64
#define TRACE_STATIC_BUFFERS 4
#define TRACE_BUFFER_MASK (TRACE_BUFFER_SIZE - 1)

#define TRACE_LEVEL_NONE    0
#define TRACE_LEVEL_ERROR   1
#define TRACE_LEVEL_WARNING 2
#define TRACE_LEVEL_INFO    3
#define TRACE_LEVEL_DEBUG   4
#define TRACE_LEVEL_VERBOSE 5

#ifndef TRACE_LEVEL
#ifdef DEBUG_BUILD
#define TRACE_LEVEL TRACE_LEVEL_DEBUG
#else
#define TRACE_LEVEL TRACE_LEVEL_WARNING
#endif
#endif

enum TraceCategory : uint16_t {
  TRACE_CAT_AUDIO     = 0x0001,
  TRACE_CAT_LED       = 0x0002,
  TRACE_CAT_I2S       = 0x0004,
  TRACE_CAT_MUTEX     = 0x0008,
  TRACE_CAT_TASK      = 0x0010,
  TRACE_CAT_TIMING    = 0x0020,
  TRACE_CAT_MEMORY    = 0x0040,
  TRACE_CAT_ERROR     = 0x0080,
  TRACE_CAT_PERF      = 0x0100,
  TRACE_CAT_CRITICAL  = 0x8000
};

enum TraceEventID : uint16_t {
  AUDIO_FRAME_START       = 0x1000,
  AUDIO_I2S_READ_START    = 0x1001,
  AUDIO_I2S_READ_DONE     = 0x1002,
  AUDIO_PROCESS_START     = 0x1003,
  AUDIO_GDFT_START        = 0x1004,
  AUDIO_GDFT_DONE         = 0x1005,
  AUDIO_VU_CALC           = 0x1006,
  AUDIO_FRAME_DONE        = 0x1007,

  LED_FRAME_START         = 0x2000,
  LED_CALC_START          = 0x2001,
  LED_BUFFER_UPDATE       = 0x2002,
  LED_SHOW_START          = 0x2003,
  LED_SHOW_DONE           = 0x2004,
  LED_FRAME_DONE          = 0x2005,

  PERF_DEADLINE_MISS      = 0x4000,
  PERF_BUFFER_OVERFLOW    = 0x4001,
  PERF_HIGH_LATENCY       = 0x4002,

  ERROR_I2S_TIMEOUT       = 0x5000,
};

struct TraceEvent {
  uint32_t timestamp;
  uint16_t event_id;
  uint8_t core_id;
  uint8_t level;
  uint32_t data;
} __attribute__((packed));

template <size_t SIZE>
class LockFreeTraceBuffer {
 public:
  static_assert((SIZE & (SIZE - 1)) == 0, "Size must be power of 2");

  __attribute__((always_inline))
  inline bool push(uint16_t event_id, uint32_t data, uint8_t level) {
    uint32_t current_head = head.load(std::memory_order_relaxed);
    uint32_t next_head = (current_head + 1) & TRACE_BUFFER_MASK;
    if (next_head == tail.load(std::memory_order_acquire)) {
      dropped_events.fetch_add(1, std::memory_order_relaxed);
      return false;
    }
    buffer[current_head].timestamp = micros();
    buffer[current_head].event_id = event_id;
    buffer[current_head].core_id = xPortGetCoreID();
    buffer[current_head].level = level;
    buffer[current_head].data = data;
    head.store(next_head, std::memory_order_release);
    return true;
  }

  bool pop(TraceEvent& event) {
    uint32_t current_tail = tail.load(std::memory_order_relaxed);
    if (current_tail == head.load(std::memory_order_acquire)) {
      return false;
    }
    event = buffer[current_tail];
    tail.store((current_tail + 1) & TRACE_BUFFER_MASK, std::memory_order_release);
    return true;
  }

  uint32_t get_dropped_count() const {
    return dropped_events.load(std::memory_order_relaxed);
  }

  uint32_t get_used_count() const {
    uint32_t h = head.load(std::memory_order_relaxed);
    uint32_t t = tail.load(std::memory_order_relaxed);
    return (h - t) & TRACE_BUFFER_MASK;
  }

  void reset_stats() {
    dropped_events.store(0, std::memory_order_relaxed);
  }

 private:
  alignas(64) std::atomic<uint32_t> head{0};
  alignas(64) std::atomic<uint32_t> tail{0};
  alignas(64) TraceEvent buffer[SIZE];
  std::atomic<uint32_t> dropped_events{0};
};

extern LockFreeTraceBuffer<TRACE_BUFFER_SIZE> g_trace_buffer;

struct TraceConfig {
  uint16_t active_categories = TRACE_CAT_ERROR | TRACE_CAT_CRITICAL;
  uint8_t min_level = TRACE_LEVEL_WARNING;
  bool enable_serial = false;
  bool enable_wifi = false;
  bool enable_sd = false;
};

extern TraceConfig g_trace_config;

#if TRACE_LEVEL == TRACE_LEVEL_NONE
#define TRACE_EVENT(cat, id, data) ((void)0)
#define TRACE_ERROR(id, data) ((void)0)
#define TRACE_WARNING(id, data) ((void)0)
#define TRACE_INFO(id, data) ((void)0)
#define TRACE_DEBUG(id, data) ((void)0)
#else
#define TRACE_EVENT(cat, id, data) \
  do { \
    if (g_trace_config.enable_serial && \
        (g_trace_config.active_categories & (cat)) && \
        (TRACE_LEVEL >= TRACE_LEVEL_DEBUG)) { \
      g_trace_buffer.push(id, data, TRACE_LEVEL_DEBUG); \
    } \
  } while (0)

#if TRACE_LEVEL >= TRACE_LEVEL_ERROR
#define TRACE_ERROR(id, data) g_trace_buffer.push(id, data, TRACE_LEVEL_ERROR)
#else
#define TRACE_ERROR(id, data) ((void)0)
#endif

#if TRACE_LEVEL >= TRACE_LEVEL_WARNING
#define TRACE_WARNING(id, data) g_trace_buffer.push(id, data, TRACE_LEVEL_WARNING)
#else
#define TRACE_WARNING(id, data) ((void)0)
#endif

#if TRACE_LEVEL >= TRACE_LEVEL_INFO
#define TRACE_INFO(id, data) g_trace_buffer.push(id, data, TRACE_LEVEL_INFO)
#else
#define TRACE_INFO(id, data) ((void)0)
#endif

#if TRACE_LEVEL >= TRACE_LEVEL_DEBUG
#define TRACE_DEBUG(id, data) g_trace_buffer.push(id, data, TRACE_LEVEL_DEBUG)
#else
#define TRACE_DEBUG(id, data) ((void)0)
#endif
#endif

class AudioFrameTracer {
 public:
  __attribute__((always_inline)) inline void start_i2s() {
#if TRACE_LEVEL >= TRACE_LEVEL_DEBUG
    TRACE_EVENT(TRACE_CAT_AUDIO | TRACE_CAT_I2S, AUDIO_I2S_READ_START, micros());
#endif
  }

  __attribute__((always_inline)) inline void end_i2s(uint32_t elapsed_us,
                                                     uint32_t bytes_read) {
#if TRACE_LEVEL >= TRACE_LEVEL_DEBUG
    TRACE_EVENT(TRACE_CAT_AUDIO | TRACE_CAT_I2S, AUDIO_I2S_READ_DONE,
                (elapsed_us << 16) | (bytes_read & 0xFFFF));
    if (elapsed_us > 12000) {
      TRACE_WARNING(PERF_HIGH_LATENCY, elapsed_us);
    }
#endif
  }
};

void trace_consumer_task(void* param);
void init_performance_trace(uint16_t categories = TRACE_CAT_ERROR | TRACE_CAT_CRITICAL);
void export_trace_buffer_serial();
void export_trace_buffer_wifi(const char* host, uint16_t port);
void export_trace_buffer_sd(const char* filename);
void set_trace_categories(uint16_t categories);
void set_trace_level(uint8_t level);
void enable_trace_output(bool serial, bool wifi, bool sd);

struct TraceStats {
  uint32_t events_logged;
  uint32_t events_dropped;
  uint32_t buffer_utilization;
  uint32_t avg_event_time_us;
  uint32_t max_event_time_us;
};

TraceStats get_trace_statistics();
void reset_trace_statistics();

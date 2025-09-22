// Performance-Optimized Trace System for K1-07 SensoryBridge
// Minimal overhead implementation maintaining real-time constraints
// Author: Performance Engineering Team
// Date: 2025-09-17

#ifndef PERFORMANCE_OPTIMIZED_TRACE_H
#define PERFORMANCE_OPTIMIZED_TRACE_H

#include <Arduino.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// Performance configuration
#define TRACE_BUFFER_SIZE 1024      // Power of 2 for fast modulo
#define TRACE_QUEUE_SIZE 64          // For async processing
#define TRACE_STATIC_BUFFERS 4       // Pre-allocated format buffers
#define TRACE_BUFFER_MASK (TRACE_BUFFER_SIZE - 1)

// Compile-time trace levels
#define TRACE_LEVEL_NONE    0
#define TRACE_LEVEL_ERROR   1
#define TRACE_LEVEL_WARNING 2
#define TRACE_LEVEL_INFO    3
#define TRACE_LEVEL_DEBUG   4
#define TRACE_LEVEL_VERBOSE 5

// Default to WARNING in production
#ifndef TRACE_LEVEL
  #ifdef DEBUG_BUILD
    #define TRACE_LEVEL TRACE_LEVEL_DEBUG
  #else
    #define TRACE_LEVEL TRACE_LEVEL_WARNING
  #endif
#endif

// Event categories for runtime filtering
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
    TRACE_CAT_CRITICAL  = 0x8000   // Always enabled
};

// Event IDs for critical path monitoring
enum TraceEventID : uint16_t {
    // Audio pipeline events (Core 0)
    AUDIO_FRAME_START       = 0x1000,
    AUDIO_I2S_READ_START    = 0x1001,
    AUDIO_I2S_READ_DONE     = 0x1002,
    AUDIO_PROCESS_START     = 0x1003,
    AUDIO_GDFT_START        = 0x1004,
    AUDIO_GDFT_DONE         = 0x1005,
    AUDIO_VU_CALC           = 0x1006,
    AUDIO_FRAME_DONE        = 0x1007,

    // LED pipeline events (Core 1)
    LED_FRAME_START         = 0x2000,
    LED_CALC_START          = 0x2001,
    LED_BUFFER_UPDATE       = 0x2002,
    LED_SHOW_START          = 0x2003,
    LED_SHOW_DONE           = 0x2004,
    LED_FRAME_DONE          = 0x2005,

    // Synchronization events
    MUTEX_LOCK_ATTEMPT      = 0x3000,
    MUTEX_LOCK_SUCCESS      = 0x3001,
    MUTEX_UNLOCK            = 0x3002,
    QUEUE_SEND              = 0x3003,
    QUEUE_RECEIVE           = 0x3004,

    // Performance events
    PERF_DEADLINE_MISS      = 0x4000,
    PERF_BUFFER_OVERFLOW    = 0x4001,
    PERF_HIGH_LATENCY       = 0x4002,

    // Error events
    ERROR_I2S_TIMEOUT       = 0x5000,
    ERROR_LED_FAILURE       = 0x5001,
    ERROR_MEMORY_ALLOC      = 0x5002,
    ERROR_SYSTEM_RESTART    = 0x5003,

    // Mutex specific events
    MUTEX_LOCK_TIMEOUT      = 0x3005,
    MUTEX_LOCK_CONTENDED    = 0x3006,

    // Mutex lifecycle events (NEW)
    MUTEX_CREATE            = 0x3007,  // Track semaphore creation
    MUTEX_DESTROY           = 0x3008,  // Track semaphore deletion
    MUTEX_OWNER_CHANGE      = 0x3009,  // Track ownership transfers

    // Memory architecture events (LED-REFACTOR-001)
    MEMORY_BUFFER_INIT      = 0x4010,  // Static buffer initialization
    MEMORY_DMA_VALIDATION   = 0x4011,  // DMA region validation check
    MEMORY_BUFFER_RESET     = 0x4012,  // Buffer cleanup/reset
    MEMORY_BOUNDS_CHECK     = 0x4013,  // LED count bounds validation
    MEMORY_ALIGNMENT_CHECK  = 0x4014   // 32-byte alignment verification
};

// Compact trace event structure (12 bytes)
struct TraceEvent {
    uint32_t timestamp;      // Microseconds since boot
    uint16_t event_id;       // Event identifier
    uint8_t core_id;         // Core that generated event
    uint8_t level;           // Trace level
    uint32_t data;           // Event-specific data
} __attribute__((packed));

// Lock-free ring buffer for minimum overhead
template<size_t SIZE>
class LockFreeTraceBuffer {
private:
    static_assert((SIZE & (SIZE - 1)) == 0, "Size must be power of 2");

    alignas(64) std::atomic<uint32_t> head{0};  // Cache line aligned
    alignas(64) std::atomic<uint32_t> tail{0};  // Separate cache line
    alignas(64) TraceEvent buffer[SIZE];        // Data cache line aligned

    std::atomic<uint32_t> dropped_events{0};

public:
    // Fast, lock-free push (safe from any context including ISR)
    __attribute__((always_inline))
    inline bool push(uint16_t event_id, uint32_t data, uint8_t level = TRACE_LEVEL_INFO) {
        uint32_t current_head = head.load(std::memory_order_relaxed);
        uint32_t next_head = (current_head + 1) & TRACE_BUFFER_MASK;

        // Check for buffer full (conservative check)
        if (next_head == tail.load(std::memory_order_acquire)) {
            dropped_events.fetch_add(1, std::memory_order_relaxed);
            return false;
        }

        // Fill event data
        buffer[current_head].timestamp = micros();
        buffer[current_head].event_id = event_id;
        buffer[current_head].core_id = xPortGetCoreID();
        buffer[current_head].level = level;
        buffer[current_head].data = data;

        // Commit the write
        head.store(next_head, std::memory_order_release);
        return true;
    }

    // Pop for trace consumer (typically on Core 1)
    bool pop(TraceEvent& event) {
        uint32_t current_tail = tail.load(std::memory_order_relaxed);

        if (current_tail == head.load(std::memory_order_acquire)) {
            return false; // Buffer empty
        }

        event = buffer[current_tail];
        tail.store((current_tail + 1) & TRACE_BUFFER_MASK, std::memory_order_release);
        return true;
    }

    // Get statistics
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
};

// Global trace buffer instance
extern LockFreeTraceBuffer<TRACE_BUFFER_SIZE> g_trace_buffer;

// Runtime trace configuration
struct TraceConfig {
    uint16_t active_categories = TRACE_CAT_ERROR | TRACE_CAT_CRITICAL;
    uint8_t min_level = TRACE_LEVEL_WARNING;
    bool enable_serial = false;
    bool enable_wifi = false;
    bool enable_sd = false;
};

extern TraceConfig g_trace_config;

// Performance-optimized trace macros

// Zero-overhead when disabled at compile time
#if TRACE_LEVEL == TRACE_LEVEL_NONE
    #define TRACE_EVENT(cat, id, data) ((void)0)
    #define TRACE_ERROR(id, data) ((void)0)
    #define TRACE_WARNING(id, data) ((void)0)
    #define TRACE_INFO(id, data) ((void)0)
    #define TRACE_DEBUG(id, data) ((void)0)
    #define TRACE_TIMING_START(id) ((void)0)
    #define TRACE_TIMING_END(id) ((void)0)
#else
    // Minimal overhead trace - compile-time level check
    #define TRACE_EVENT(cat, id, data) \
        do { \
            if ((g_trace_config.active_categories & (cat)) && \
                (TRACE_LEVEL >= TRACE_LEVEL_DEBUG)) { \
                g_trace_buffer.push(id, data, TRACE_LEVEL_DEBUG); \
            } \
        } while(0)

    // Level-specific macros with compile-time optimization
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

    // Timing macros for performance monitoring
    #define TRACE_TIMING_START(id) \
        uint32_t _trace_start_##id = micros(); \
        TRACE_EVENT(TRACE_CAT_TIMING, id, _trace_start_##id)

    #define TRACE_TIMING_END(id) \
        do { \
            uint32_t _elapsed = micros() - _trace_start_##id; \
            TRACE_EVENT(TRACE_CAT_TIMING, id | 0x8000, _elapsed); \
        } while(0)
#endif

// Critical path instrumentation example
class AudioFrameTracer {
private:
    uint32_t frame_start;
    uint32_t phase_start;

public:
    __attribute__((always_inline))
    inline void start_frame() {
        #if TRACE_LEVEL >= TRACE_LEVEL_INFO
        frame_start = micros();
        TRACE_EVENT(TRACE_CAT_AUDIO, AUDIO_FRAME_START, frame_start);
        #endif
    }

    __attribute__((always_inline))
    inline void start_i2s() {
        #if TRACE_LEVEL >= TRACE_LEVEL_DEBUG
        phase_start = micros();
        TRACE_EVENT(TRACE_CAT_AUDIO | TRACE_CAT_I2S, AUDIO_I2S_READ_START, phase_start);
        #endif
    }

    __attribute__((always_inline))
    inline void end_i2s(size_t bytes_read) {
        #if TRACE_LEVEL >= TRACE_LEVEL_DEBUG
        uint32_t elapsed = micros() - phase_start;
        TRACE_EVENT(TRACE_CAT_AUDIO | TRACE_CAT_I2S, AUDIO_I2S_READ_DONE,
                   (elapsed << 16) | (bytes_read & 0xFFFF));
        #endif
    }

    __attribute__((always_inline))
    inline void start_gdft() {
        #if TRACE_LEVEL >= TRACE_LEVEL_DEBUG
        phase_start = micros();
        TRACE_EVENT(TRACE_CAT_AUDIO, AUDIO_GDFT_START, phase_start);
        #endif
    }

    __attribute__((always_inline))
    inline void end_gdft() {
        #if TRACE_LEVEL >= TRACE_LEVEL_DEBUG
        uint32_t elapsed = micros() - phase_start;
        TRACE_EVENT(TRACE_CAT_AUDIO, AUDIO_GDFT_DONE, elapsed);
        #endif
    }

    __attribute__((always_inline))
    inline void end_frame() {
        #if TRACE_LEVEL >= TRACE_LEVEL_INFO
        uint32_t total_elapsed = micros() - frame_start;
        TRACE_EVENT(TRACE_CAT_AUDIO | TRACE_CAT_PERF, AUDIO_FRAME_DONE, total_elapsed);

        // Check for deadline miss
        if (total_elapsed > 9000) {  // 9ms deadline
            TRACE_ERROR(PERF_DEADLINE_MISS, total_elapsed);
        }
        #endif
    }
};

// LED frame tracer
class LEDFrameTracer {
private:
    uint32_t frame_start;
    uint32_t show_start;

public:
    __attribute__((always_inline))
    inline void start_frame() {
        #if TRACE_LEVEL >= TRACE_LEVEL_INFO
        frame_start = micros();
        TRACE_EVENT(TRACE_CAT_LED, LED_FRAME_START, frame_start);
        #endif
    }

    __attribute__((always_inline))
    inline void start_show() {
        #if TRACE_LEVEL >= TRACE_LEVEL_DEBUG
        show_start = micros();
        TRACE_EVENT(TRACE_CAT_LED, LED_SHOW_START, show_start);
        #endif
    }

    __attribute__((always_inline))
    inline void end_show() {
        #if TRACE_LEVEL >= TRACE_LEVEL_DEBUG
        uint32_t elapsed = micros() - show_start;
        TRACE_EVENT(TRACE_CAT_LED, LED_SHOW_DONE, elapsed);
        #endif
    }

    __attribute__((always_inline))
    inline void end_frame(uint8_t fps) {
        #if TRACE_LEVEL >= TRACE_LEVEL_INFO
        uint32_t total_elapsed = micros() - frame_start;
        TRACE_EVENT(TRACE_CAT_LED | TRACE_CAT_PERF, LED_FRAME_DONE,
                   (total_elapsed << 8) | fps);
        #endif
    }
};

// Trace consumer task (runs on Core 1)
void trace_consumer_task(void* param);

// Initialize trace system
void init_performance_trace(uint16_t categories = TRACE_CAT_ERROR | TRACE_CAT_CRITICAL);

// Export functions for analysis
void export_trace_buffer_serial();
void export_trace_buffer_wifi(const char* host, uint16_t port);
void export_trace_buffer_sd(const char* filename);

// Runtime control
void set_trace_categories(uint16_t categories);
void set_trace_level(uint8_t level);
void enable_trace_output(bool serial, bool wifi, bool sd);

// Performance statistics
struct TraceStats {
    uint32_t events_logged;
    uint32_t events_dropped;
    uint32_t buffer_utilization;
    uint32_t avg_event_time_us;
    uint32_t max_event_time_us;
};

TraceStats get_trace_statistics();
void reset_trace_statistics();

// Example integration for SensoryBridge
inline void trace_acquire_sample_chunk(uint32_t bytes_read, uint32_t elapsed_us) {
    #if TRACE_LEVEL >= TRACE_LEVEL_DEBUG
    if (elapsed_us > 200) {  // Log slow I2S reads
        TRACE_WARNING(AUDIO_I2S_READ_DONE, (elapsed_us << 16) | (bytes_read & 0xFFFF));
    } else {
        TRACE_DEBUG(AUDIO_I2S_READ_DONE, (elapsed_us << 16) | (bytes_read & 0xFFFF));
    }
    #endif
}

inline void trace_fastled_show(uint32_t elapsed_us) {
    #if TRACE_LEVEL >= TRACE_LEVEL_INFO
    TRACE_EVENT(TRACE_CAT_LED | TRACE_CAT_TIMING, LED_SHOW_DONE, elapsed_us);
    if (elapsed_us > 4000) {  // Alert if > 4ms
        TRACE_WARNING(PERF_HIGH_LATENCY, elapsed_us);
    }
    #endif
}

inline void trace_mutex_operation(void* mutex, bool acquired, uint32_t wait_us) {
    #if TRACE_LEVEL >= TRACE_LEVEL_DEBUG
    if (acquired) {
        if (wait_us > 100) {  // Log contended mutexes
            TRACE_WARNING(MUTEX_LOCK_SUCCESS, (uint32_t)mutex);
        } else {
            TRACE_DEBUG(MUTEX_LOCK_SUCCESS, wait_us);
        }
    } else {
        TRACE_DEBUG(MUTEX_UNLOCK, (uint32_t)mutex);
    }
    #endif
}

#endif // PERFORMANCE_OPTIMIZED_TRACE_H

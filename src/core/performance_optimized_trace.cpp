#ifndef DEBUG_BUILD
#define DEBUG_BUILD 1
#endif
#include "performance_optimized_trace.h"
#include <freertos/task.h>

LockFreeTraceBuffer<TRACE_BUFFER_SIZE> g_trace_buffer;
TraceConfig g_trace_config;

void trace_consumer_task(void* /*param*/)
{
  TraceEvent event;
  for (;;) {
    while (g_trace_buffer.pop(event)) {
      // Trace events can be exported by separate tasks when outputs are enabled.
      // This consumer simply drains the buffer to minimize back-pressure.
    }
    vTaskDelay(1);
  }
}

void init_performance_trace(uint16_t categories)
{
  g_trace_config.active_categories = categories;
  g_trace_config.min_level = TRACE_LEVEL;
  g_trace_config.enable_serial = false;
  g_trace_config.enable_wifi = false;
  g_trace_config.enable_sd = false;
  g_trace_buffer.reset_stats();
}

void export_trace_buffer_serial()
{
  g_trace_config.enable_serial = true;
}

void export_trace_buffer_wifi(const char* /*host*/, uint16_t /*port*/)
{
  g_trace_config.enable_wifi = true;
}

void export_trace_buffer_sd(const char* /*filename*/)
{
  g_trace_config.enable_sd = true;
}

void set_trace_categories(uint16_t categories)
{
  g_trace_config.active_categories = categories;
}

void set_trace_level(uint8_t level)
{
  g_trace_config.min_level = level;
}

void enable_trace_output(bool serial, bool wifi, bool sd)
{
  g_trace_config.enable_serial = serial;
  g_trace_config.enable_wifi = wifi;
  g_trace_config.enable_sd = sd;
}

TraceStats get_trace_statistics()
{
  TraceStats stats{};
  stats.events_logged = g_trace_buffer.get_used_count();
  stats.events_dropped = g_trace_buffer.get_dropped_count();
  stats.buffer_utilization = g_trace_buffer.get_used_count();
  stats.avg_event_time_us = 0;
  stats.max_event_time_us = 0;
  return stats;
}

void reset_trace_statistics()
{
  g_trace_buffer.reset_stats();
}

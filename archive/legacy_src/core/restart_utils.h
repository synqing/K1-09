#pragma once

#include <Arduino.h>
#include <esp_debug_helpers.h>
#include <esp_system.h>
#include <esp_rom_sys.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "performance_optimized_trace.h"

namespace sb {

inline void restart_impl(const char* reason, const char* file, int line) {
#if defined(USBSerial)
  if (USBSerial) {
    USBSerial.printf("[RESTART] %s (%s:%d)\r\n", reason ? reason : "(null)", file, line);
    USBSerial.flush();
  }
#else
  Serial.printf("[RESTART] %s (%s:%d)\r\n", reason ? reason : "(null)", file, line);
  Serial.flush();
#endif
  ets_printf("[RESTART] %s (%s:%d)\r\n", reason ? reason : "(null)", file, line);
  ets_printf("[TRACE] ERROR_SYSTEM_RESTART line=%d\r\n", line);
  TRACE_ERROR(ERROR_SYSTEM_RESTART, static_cast<uint32_t>(line));
  esp_backtrace_print(100);
  vTaskDelay(pdMS_TO_TICKS(20));
  ets_delay_us(50000);
  esp_restart();
}

}  // namespace sb

#define SB_RESTART(reason) ::sb::restart_impl((reason), __FILE__, __LINE__)

// Compile p2p.h bodies exactly once.
#define SB_P2P_IMPL 1

#include <Arduino.h>
#include <esp_now.h>

#if defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT && !defined(USBSerial)
#define USBSerial Serial
#endif

#include "../p2p.h"
#include "../led_utilities.h"
#include "../noise_cal.h"

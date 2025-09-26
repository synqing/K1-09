#define SB_LED_COLOR_DEBUG_IMPL 1
#include "../globals.h"
#if defined(ARDUINO_USB_CDC_ON_BOOT) && !defined(USBSerial)
#define USBSerial Serial
#endif
#include "../led_color_debug.h"

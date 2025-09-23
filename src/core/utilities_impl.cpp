#define SB_UTILITIES_IMPL 1
#include <Arduino.h>
#if defined(ARDUINO_USB_CDC_ON_BOOT) && !defined(USBSerial)
#define USBSerial Serial
#endif
#include <math.h>
#include <cstdio>
#include "../globals.h"
#include "../utilities.h"

// Build serial_menu.h bodies exactly once.
#define SB_SERIAL_MENU_IMPL 1

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <driver/i2s.h>

#if defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT && !defined(USBSerial)
#define USBSerial Serial
#endif

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION 40101
#endif

#include "../serial_menu.h"

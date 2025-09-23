#define SB_BRIDGE_FS_IMPL 1
#include "../globals.h"
#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#if defined(ARDUINO_USB_CDC_ON_BOOT) && !defined(USBSerial)
#define USBSerial Serial
#endif
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION 40101
#endif
#ifndef SB_PASS
#define SB_PASS "PASS"
#endif
#ifndef SB_FAIL
#define SB_FAIL "FAIL ###################"
#endif
#include "../bridge_fs.h"

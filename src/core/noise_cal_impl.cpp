// Build noise_cal.h bodies once here.
#define SB_NOISE_CAL_IMPL 1

#include "../globals.h"
#if defined(ARDUINO_USB_CDC_ON_BOOT) && !defined(USBSerial)
#define USBSerial Serial
#endif
#ifndef SB_PASS
#define SB_PASS "PASS"
#endif
#ifndef SB_FAIL
#define SB_FAIL "FAIL ###################"
#endif
#include "../bridge_fs.h"
#include "../utilities.h"
#include "../i2s_audio.h"
#include "../noise_cal.h"

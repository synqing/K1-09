#define SB_BRIDGE_FS_IMPL 1
#include "../globals.h"
#include <FS.h>
#include <LittleFS.h>
#ifndef SB_PASS
#define SB_PASS "PASS"
#endif
#ifndef SB_FAIL
#define SB_FAIL "FAIL ###################"
#endif
#include "../bridge_fs.h"

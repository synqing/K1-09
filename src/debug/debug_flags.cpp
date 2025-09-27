#include "debug/debug_flags.h"

namespace debug_flags {

// Default: no verbose debug groups enabled at boot.
volatile uint32_t g_debug_mask = 0u;

} // namespace debug_flags

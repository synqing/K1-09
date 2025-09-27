#pragma once

#include <stdint.h>

namespace debug_ui {

// Initialize debug UI (prints status + help)
void init();

// Handle a single key intended for debug controls (1/2/3/4/0/?/h)
// Returns true if the key was handled by debug UI.
bool handle_key(char c);

// Emit periodic debug telemetry (tempo/energy/AP input), called once per loop
void tick();

} // namespace debug_ui


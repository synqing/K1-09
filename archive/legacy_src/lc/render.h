#pragma once

#include "audio_bridge.h"

namespace lc {

// Placeholder LC renderer entry point. For now it just exists so we can
// toggle the legacy pipeline without behaviour changes.
void render_lc_frame(const AudioMetrics& metrics);

}  // namespace lc


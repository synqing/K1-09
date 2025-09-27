#include "vp_config.h"

#include "storage/NVS.h"

namespace vp_config {

namespace {
VPConfig g_cfg{true, 1000u};
constexpr const char* kNvsNS = "vp";
constexpr const char* kKeyUseSmoothed = "use_smoothed";
constexpr const char* kKeyDbgPeriod   = "dbg_period";
}

void init() {
  storage::nvs::init(kNvsNS);
  uint32_t v = 0;
  if (storage::nvs::read_u32(kKeyUseSmoothed, v)) {
    g_cfg.use_smoothed_spectrum = (v != 0u);
  }
  if (storage::nvs::read_u32(kKeyDbgPeriod, v)) {
    if (v < 50u) v = 50u; // guardrail
    g_cfg.debug_period_ms = v;
  }
}

const VPConfig& get() {
  return g_cfg;
}

bool set(const VPConfig& cfg, bool persist) {
  g_cfg = cfg;
  if (persist) {
    storage::nvs::write_u32(kKeyUseSmoothed, g_cfg.use_smoothed_spectrum ? 1u : 0u);
    storage::nvs::write_u32(kKeyDbgPeriod, g_cfg.debug_period_ms);
  }
  return true;
}

} // namespace vp_config


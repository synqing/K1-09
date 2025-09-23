#include "dual_coordinator.h"
#include "globals.h"
#include <Arduino.h>

// Router FSM scaffolding for Agent B: expand as per docs/phase3_dual_router_task.md
// Note: Keep this file dependency-light; actual integration should use coordinator_update.

struct RouterFsmDebug {
  uint32_t last_log_ms = 0;
};

static RouterFsmDebug g_router_dbg;

// Placeholder: call from coordinator_update() once policy is ready
void router_fsm_tick(const SQ15x16* novelty_curve,
                     SQ15x16 audio_vu,
                     uint32_t now_ms,
                     RouterState& state,
                     CouplingPlan& plan) {
  // TODO(AgentB): Implement dwell/cooldown + pair whitelist + variation policy
  (void)novelty_curve;
  (void)audio_vu;
  (void)now_ms;
  (void)state;
  (void)plan;

  // Throttled debug
  if (now_ms - g_router_dbg.last_log_ms > 4000) {
    g_router_dbg.last_log_ms = now_ms;
    USBSerial.println("[ROUTER] tick placeholder – implement FSM");
  }
}


#include "NVS.h"

#include <cstring>

namespace storage {
namespace nvs {

namespace {
Preferences g_prefs;
bool g_ready = false;
const char* g_namespace = nullptr;

struct PendingEntry {
  const char* key;
  bool in_use;
  bool pending;
  uint32_t value;
  uint32_t last_write_ms;
  uint32_t min_interval_ms;
};

constexpr size_t kMaxPending = 4;
PendingEntry g_pending[kMaxPending] = {};

PendingEntry* find_or_create_slot(const char* key) {
  for (auto& entry : g_pending) {
    if (entry.in_use && std::strcmp(entry.key, key) == 0) {
      return &entry;
    }
  }
  for (auto& entry : g_pending) {
    if (!entry.in_use) {
      entry.in_use = true;
      entry.key = key;
      entry.pending = false;
      entry.last_write_ms = 0;
      entry.min_interval_ms = 0;
      return &entry;
    }
  }
  return nullptr;
}

bool ensure_ready(const char* ns) {
  if (g_ready) {
    if (g_namespace && std::strcmp(g_namespace, ns) == 0) {
      return true;
    }
    g_prefs.end();
    g_ready = false;
  }
  g_ready = g_prefs.begin(ns, false);
  if (g_ready) {
    g_namespace = ns;
  }
  return g_ready;
}

bool write_immediate(const char* key, uint32_t value) {
  if (!g_ready) return false;
  bool ok = g_prefs.putUInt(key, value) >= 0;
  if (ok) {
    auto* slot = find_or_create_slot(key);
    if (slot) {
      slot->pending = false;
      slot->last_write_ms = millis();
    }
  }
  return ok;
}

} // namespace

bool init(const char* ns) {
  return ensure_ready(ns);
}

bool read_u32(const char* key, uint32_t& out_value) {
  if (!g_ready) return false;
  if (!g_prefs.isKey(key)) return false;
  out_value = g_prefs.getUInt(key, 0u);
  return true;
}

bool write_u32(const char* key, uint32_t value) {
  return write_immediate(key, value);
}

void write_u32_debounced(const char* key,
                         uint32_t value,
                         uint32_t minimum_interval_ms,
                         bool force) {
  if (!g_ready) {
    return;
  }
  if (force || minimum_interval_ms == 0u) {
    write_immediate(key, value);
    return;
  }

  PendingEntry* slot = find_or_create_slot(key);
  if (!slot) {
    write_immediate(key, value);
    return;
  }
  slot->min_interval_ms = minimum_interval_ms;
  slot->value = value;
  slot->pending = true;

  const uint32_t now = millis();
  if ((now - slot->last_write_ms) >= slot->min_interval_ms) {
    write_immediate(key, slot->value);
  }
}

void poll() {
  if (!g_ready) return;
  const uint32_t now = millis();
  for (auto& entry : g_pending) {
    if (!entry.in_use || !entry.pending) continue;
    if ((now - entry.last_write_ms) >= entry.min_interval_ms) {
      write_immediate(entry.key, entry.value);
    }
  }
}

} // namespace nvs
} // namespace storage


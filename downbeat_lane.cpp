#include "downbeat_lane.h"
#include <string.h>
#include <math.h>

namespace downbeat {

static constexpr float EPS = 1e-6f;

struct DBState {
  // Config
  uint8_t bpb = 4;            // beats per bar
  float   conf_on  = 0.60f;   // confidence on
  float   conf_off = 0.42f;   // confidence off
  float   env_decay = 0.92f;  // accent envelope decay per visual frame

  // Runtime
  bool    armed = false;      // true in confident / non-silent state
  uint8_t beat_idx = 0;       // 0..bpb-1, advanced on beat_flag
  uint8_t est_db  = 0;        // estimated downbeat position 0..bpb-1

  // Energy pattern: per-beat energy aggregated over last few bars
  static constexpr int MAX_BPB = 8;
  static constexpr int BARS_ACC = 8; // integrate ~8 bars of history
  float energy_hist[MAX_BPB][BARS_ACC]; // [beat_pos][ring of bars]
  uint8_t bar_head = 0;       // where to write next bar sample
  bool    has_bar = false;    // true once at least one full bar seen

  // Per-bar accumulation
  float   cur_bar_energy[MAX_BPB];    // accumulate energy per beat within current bar

  // Outputs
  bool    downbeat_edge = false;
  float   accent_env = 0.0f;          // decaying 0..1

  // Phase bookkeeping
  // We reconstruct bar-phase from beat_phase in [0,1) and beat_idx.
  int32_t bar_phase_q16 = 0;
};

static DBState g;

static inline float q16_to_lin(int32_t q){ return (q <= 0) ? 0.0f : ((float)q / 65536.0f); }
static inline int32_t lin_to_q16(float x){
  if (x <= 0.0f) return 0;
  float y = (x >= 0.99998474f) ? 0.99998474f : x;
  return (int32_t)lrintf(y * 65536.0f);
}

bool init(uint8_t beats_per_bar) {
  if (beats_per_bar == 0 || beats_per_bar > DBState::MAX_BPB) beats_per_bar = 4;
  memset(&g, 0, sizeof(g));
  g.bpb = beats_per_bar;
  return true;
}

void set_conf_threshold(float on, float off) {
  if (on < 0.1f) on = 0.1f; if (on > 0.95f) on = 0.95f;
  if (off < 0.0f) off = 0.0f; if (off > on - 0.05f) off = on - 0.05f;
  g.conf_on = on; g.conf_off = off;
}

void set_env_decay(float per_frame){
  if (per_frame < 0.80f) per_frame = 0.80f;
  if (per_frame > 0.99f) per_frame = 0.99f;
  g.env_decay = per_frame;
}

static inline float clamp01(float x){ return (x<0.0f?0.0f:(x>1.0f?1.0f:x)); }

// Called on each beat (when beat_flag=1) to commit per-beat energy into current bar
static void on_beat_commit(float e_val){
  // Accumulate energy for current beat slot
  g.cur_bar_energy[g.beat_idx] += e_val;

  // If this beat_idx rolled over to 0 (next call) it's a new bar; we commit on downbeat edge below.
}

// Commit end-of-bar: rotate hist ring and determine new downbeat estimate
static void commit_bar() {
  // Write current bar energies into hist at bar_head
  for (uint8_t i=0;i<g.bpb;++i){
    g.energy_hist[i][g.bar_head] = g.cur_bar_energy[i];
    g.cur_bar_energy[i] = 0.0f; // reset for next bar
  }
  g.bar_head = (g.bar_head + 1u) % DBState::BARS_ACC;
  g.has_bar  = true;

  // Determine strongest consistent position
  // Sum across bars with mild forgetting towards older bars
  float best = -1.0f; uint8_t best_i = 0;
  for (uint8_t i=0;i<g.bpb;++i){
    float acc = 0.0f; float w = 1.0f;
    // newest bar is at (bar_head-1)
    for (int k=0;k<DBState::BARS_ACC;++k){
      int idx = (int)g.bar_head - 1 - k; if (idx < 0) idx += DBState::BARS_ACC;
      acc += w * g.energy_hist[i][idx];
      w *= 0.85f; // forget older bars
    }
    if (acc > best) { best = acc; best_i = i; }
  }
  g.est_db = best_i;
}

void ingest(const AudioFrame& f) {
  // Confidence / silence arming
  float conf = q16_to_lin(f.tempo_confidence);
  float sil  = q16_to_lin(f.tempo_silence);
  if (!g.armed) {
    if (conf >= g.conf_on && sil < 0.7f) g.armed = true;
  } else {
    if (conf <= g.conf_off || sil >= 0.9f) g.armed = false;
  }

  // Always decay accent envelope
  g.accent_env *= g.env_decay;

  // Derive a per-beat energy sample:
  // blend of beat_strength and low-band energy proportionally
  float beat_str = q16_to_lin(f.beat_strength);
  // If you publish band_low etc. as int32 sums, normalise roughly:
  float low_lin  = 0.0f;
  // Heuristic normalisation: map large sums into ~0..1 via soft-knee
  {
    // Use band_low if present; otherwise rely on beat_strength only.
    // (We expect band_low to be >=0)
    float lo = 0.0f;
    // Take absolute in case of signed pack
    lo = (float)(f.band_low >= 0 ? f.band_low : -f.band_low);
    // very light compression to 0..1-ish
    low_lin = lo / (lo + 250000.0f);
  }
  float energy_sample = clamp01(0.6f*beat_str + 0.4f*low_lin);

  // Compute bar phase from beat index and beat_phase
  float beat_phase = q16_to_lin(f.beat_phase);          // 0..1
  float bar_phase  = ( (float)g.beat_idx + beat_phase ) / (float)g.bpb;
  bar_phase -= floorf(bar_phase);                        // wrap [0,1)
  g.bar_phase_q16 = lin_to_q16(bar_phase);

  // Beat edge processing
  g.downbeat_edge = false;
  if (f.beat_flag && g.armed) {
    // If current position matches our downbeat estimate → pulse + accent
    if (g.beat_idx == g.est_db) {
      g.downbeat_edge = true;
      // Stronger accent on downbeat; re-seed the envelope
      g.accent_env = fmaxf(g.accent_env, clamp01(0.75f + 0.25f*beat_str));
      // We are starting a new bar now → commit the previous bar and rotate
      commit_bar();
    } else {
      // We’re at a non-downbeat beat → lighter accent
      g.accent_env = fmaxf(g.accent_env, clamp01(0.35f + 0.30f*beat_str));
      // Accumulate per-beat energy inside this ongoing bar
      on_beat_commit(energy_sample);
    }

    // Advance beat index within bar for next frame
    g.beat_idx = (uint8_t)((g.beat_idx + 1u) % g.bpb);
  }

  // If not armed / in silence, slowly forget energy
  if (!g.armed) {
    for (uint8_t i=0;i<g.bpb;++i) {
      g.cur_bar_energy[i] *= 0.95f;
      for (int k=0;k<DBState::BARS_ACC;++k)
        g.energy_hist[i][k] *= 0.97f;
    }
  }
}

// Signals ----------------------------------------------------------------
bool     downbeat_pulse(){ return g.downbeat_edge; }
uint8_t  bar_index()     { return g.beat_idx % g.bpb; }
int32_t  bar_phase_q16() { return g.bar_phase_q16; }
float    accent()        { return g.accent_env;    }

} // namespace downbeat

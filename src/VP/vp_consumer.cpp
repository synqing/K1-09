#include "vp_consumer.h"

namespace vp_consumer {

bool acquire(VPFrame& out) {
  if (!audio_frame_utils::snapshot_audio_frame(out.af)) {
    return false;
  }
  out.epoch = out.af.audio_frame_epoch;
  out.t_ms = out.af.t_ms;
  return true;
}

} // namespace vp_consumer


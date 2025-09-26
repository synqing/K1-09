#include "vp.h"

#include "vp_consumer.h"
#include "vp_renderer.h"
#include "vp_config.h"

namespace vp {

void init() {
  vp_config::init();
  vp_renderer::init();
}

void tick() {
  vp_consumer::VPFrame f{};
  if (!vp_consumer::acquire(f)) {
    return;
  }
  vp_renderer::render(f);
}

} // namespace vp

// --------- HMI proxies ---------
namespace vp {

void brightness_up()   { vp_renderer::adjust_brightness(+16); }
void brightness_down() { vp_renderer::adjust_brightness(-16); }
void speed_up()        { vp_renderer::adjust_speed(1.15f); }
void speed_down()      { vp_renderer::adjust_speed(1.0f/1.15f); }
void next_mode()       { vp_renderer::next_mode(); }
void prev_mode()       { vp_renderer::prev_mode(); }

HMIStatus hmi_status() {
  auto s = vp_renderer::status();
  return HMIStatus{ (unsigned)s.brightness, s.speed, (unsigned)s.mode };
}

} // namespace vp

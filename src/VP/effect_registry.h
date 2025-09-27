#pragma once

#include <memory>
#include <vector>

#include "effect.h"

namespace vp {

class EffectRegistry {
 public:
  EffectRegistry();

  Effect& current();
  const Effect& current() const;

  void next();
  void prev();
  void set(uint8_t index);
  uint8_t index() const { return current_index_; }
  uint8_t count() const { return static_cast<uint8_t>(effects_.size()); }

 private:
  void register_default_effects();

  std::vector<std::unique_ptr<Effect>> effects_;
  uint8_t current_index_ = 0;
};

}  // namespace vp


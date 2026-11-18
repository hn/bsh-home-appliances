#pragma once

#include "esphome/core/automation.h"
#include "bshdbus.h"

#include <vector>

namespace esphome::bshdbus {

class FrameTrigger final
    : public Trigger<const std::vector<uint8_t> &, uint8_t, uint16_t, const std::vector<uint8_t> &> {
 public:
  explicit FrameTrigger(BSHDBus *bshdbus) {
    bshdbus->add_on_frame_callback(
        [this](const std::vector<uint8_t> &frame, uint8_t dest, uint16_t command, const std::vector<uint8_t> &message) {
          this->trigger(frame, dest, command, message);
        });
  }
};

}  // namespace esphome::bshdbus

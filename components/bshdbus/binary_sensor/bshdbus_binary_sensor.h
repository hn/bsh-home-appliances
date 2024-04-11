#pragma once

#include "../bshdbus.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace bshdbus {

class BSHDBusCustomSubBSensor;

class BSHDBusCustomBSensor : public BSHDBusListener, public Component {
 public:
  void dump_config() override;
  void set_bsensors(std::vector<BSHDBusCustomSubBSensor *> bsensors) { this->bsensors_ = std::move(bsensors); };

 protected:
  std::vector<BSHDBusCustomSubBSensor *> bsensors_;
  void handle_message(std::vector<uint8_t> &message) override;
};

class BSHDBusCustomSubBSensor : public binary_sensor::BinarySensor, public Component {
 public:
  void set_message_parser(message_parser_t parser) { this->message_parser_ = std::move(parser); };
  void parse_message(std::vector<uint8_t> &message);

 protected:
  message_parser_t message_parser_;
};

}  // namespace bshdbus
}  // namespace esphome

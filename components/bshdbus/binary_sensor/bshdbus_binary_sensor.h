#pragma once

#include "../bshdbus.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace bshdbus {

class BSHDBusSubBinarySensor;

class BSHDBusBinarySensor : public BSHDBusListener, public Component {
 public:
  void dump_config() override;
  void set_bsensors(std::vector<BSHDBusSubBinarySensor *> bsensors) { this->bsensors_ = std::move(bsensors); };

 protected:
  std::vector<BSHDBusSubBinarySensor *> bsensors_;
  void handle_message(const std::vector<uint8_t> &message) override;
};

class BSHDBusSubBinarySensor : public binary_sensor::BinarySensor {
 public:
  void set_message_parser(binary_message_parser_t parser) { this->message_parser_ = std::move(parser); };
  void parse_message(const std::vector<uint8_t> &message);

 protected:
  binary_message_parser_t message_parser_;
};

}  // namespace bshdbus
}  // namespace esphome

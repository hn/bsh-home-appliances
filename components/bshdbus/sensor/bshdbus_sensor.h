#pragma once

#include "../bshdbus.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace bshdbus {

class BSHDBusCustomSubSensor;

class BSHDBusCustomSensor : public BSHDBusListener, public Component {
 public:
  void dump_config() override;
  void set_sensors(std::vector<BSHDBusCustomSubSensor *> sensors) { this->sensors_ = std::move(sensors); };

 protected:
  std::vector<BSHDBusCustomSubSensor *> sensors_;
  void handle_message(std::vector<uint8_t> &message) override;
};

class BSHDBusCustomSubSensor : public sensor::Sensor, public Component {
 public:
  void set_message_parser(message_parser_t parser) { this->message_parser_ = std::move(parser); };
  void parse_message(std::vector<uint8_t> &message);

 protected:
  message_parser_t message_parser_;
};

}  // namespace bshdbus
}  // namespace esphome

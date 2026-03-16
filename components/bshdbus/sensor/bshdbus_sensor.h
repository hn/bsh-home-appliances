#pragma once

#include "../bshdbus.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace bshdbus {

class BSHDBusSubSensor;

class BSHDBusSensor : public BSHDBusListener, public Component {
 public:
  void dump_config() override;
  void set_sensors(std::vector<BSHDBusSubSensor *> sensors) { this->sensors_ = std::move(sensors); };

 protected:
  std::vector<BSHDBusSubSensor *> sensors_;
  void handle_message(std::vector<uint8_t> &message) override;
};

class BSHDBusSubSensor : public sensor::Sensor, public Component {
 public:
  void set_message_parser(message_parser_t parser) { this->message_parser_ = std::move(parser); };
  void parse_message(std::vector<uint8_t> &message);

 protected:
  message_parser_t message_parser_;
};

}  // namespace bshdbus
}  // namespace esphome

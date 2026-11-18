#pragma once

#include "../bshdbus.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome::bshdbus {

class BSHDBusSubTextSensor;

class BSHDBusTextSensor final : public BSHDBusListener, public Component {
 public:
  void dump_config() override;
  void set_tsensors(std::vector<BSHDBusSubTextSensor *> tsensors) { this->tsensors_ = std::move(tsensors); };

 protected:
  std::vector<BSHDBusSubTextSensor *> tsensors_;
  void handle_message(const std::vector<uint8_t> &message) override;
};

class BSHDBusSubTextSensor final : public text_sensor::TextSensor {
 public:
  void set_message_parser(textmessage_parser_t parser) { this->message_parser_ = std::move(parser); };
  void parse_message(const std::vector<uint8_t> &message);

 protected:
  textmessage_parser_t message_parser_;
};

}  // namespace esphome::bshdbus

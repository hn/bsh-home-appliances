#pragma once

#include "../bshdbus.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome {
namespace bshdbus {

class BSHDBusSubTextSensor;

class BSHDBusTextSensor : public BSHDBusListener, public Component {
 public:
  void dump_config() override;
  void set_tsensors(std::vector<BSHDBusSubTextSensor *> tsensors) { this->tsensors_ = std::move(tsensors); };

 protected:
  std::vector<BSHDBusSubTextSensor *> tsensors_;
  void handle_message(std::vector<uint8_t> &message) override;
};

class BSHDBusSubTextSensor : public text_sensor::TextSensor, public Component {
 public:
  void set_message_parser(textmessage_parser_t parser) { this->message_parser_ = std::move(parser); };
  void parse_message(std::vector<uint8_t> &message);

 protected:
  textmessage_parser_t message_parser_;
};

}  // namespace bshdbus
}  // namespace esphome

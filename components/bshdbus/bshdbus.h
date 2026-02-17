#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace bshdbus {

using message_parser_t = std::function<float(std::vector<uint8_t> &)>;
using tmessage_parser_t = std::function<std::string(std::vector<uint8_t> &)>;

class BSHDBus;

class BSHDBusListener {
 public:
  void set_dest(uint8_t dest) { this->dest_ = dest; }
  void set_command(uint16_t command) { this->command_ = command; }

  void on_message(uint8_t dest, uint16_t command, std::vector<uint8_t> &message);

 protected:
  uint8_t dest_{0xff};
  uint16_t command_{0xffff};

  virtual void handle_message(std::vector<uint8_t> &message) = 0;
};

class BSHDBus : public uart::UARTDevice, public Component {
 public:
  void dump_config() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void register_listener(BSHDBusListener *listener) { this->listeners_.push_back(listener); }
  void add_on_frame_callback(std::function<void(std::vector<uint8_t>, uint8_t, uint16_t,
                             std::vector<uint8_t>)> &&frame_callback);

 protected:
  std::vector<uint8_t> rx_buffer_;
  uint32_t last_rx_{0};
  uint8_t expect_ack_for_{0};
  std::vector<BSHDBusListener *> listeners_{};
  CallbackManager<void(const std::vector<uint8_t> &, uint8_t, uint16_t,
                       const std::vector<uint8_t> &)> frame_callbacks_{};
};

}  // namespace bshdbus
}  // namespace esphome

#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/version.h"

#include <functional>
#include <string>
#include <utility>
#include <vector>

#if ESPHOME_VERSION_CODE < VERSION_CODE(2026, 1, 0)
#error "BSHDBus component requires ESPHome >= 2026.1.0"
#endif

namespace esphome {
namespace bshdbus {

using message_parser_t = std::function<float(const std::vector<uint8_t> &)>;
using binary_message_parser_t = std::function<bool(const std::vector<uint8_t> &)>;
using textmessage_parser_t = std::function<std::string(const std::vector<uint8_t> &)>;

class BSHDBus;

class BSHDBusListener {
 public:
  void set_dest(uint8_t dest) { this->dest_ = dest; }
  void set_command(uint16_t command) { this->command_ = command; }

  void on_message(uint8_t dest, uint16_t command, const std::vector<uint8_t> &message);
  void dump_filter_config(const char *tag) const;

 protected:
  uint8_t dest_{0xff};
  uint16_t command_{0xffff};

  virtual void handle_message(const std::vector<uint8_t> &message) = 0;
};

class BSHDBus : public uart::UARTDevice, public Component {
 public:
  void setup() override;
  void dump_config() override;
  void loop() override;

  void register_listener(BSHDBusListener *listener) { this->listeners_.push_back(listener); }
  template<typename F> void add_on_frame_callback(F &&frame_callback) {
    this->frame_callbacks_.add(std::forward<F>(frame_callback));
  }

 protected:
  std::vector<uint8_t> rx_buffer_;
  std::vector<uint8_t> message_;
  uint32_t last_rx_{0};
  uint8_t last_dest_{0};
  bool expect_ack_{false};
  std::vector<BSHDBusListener *> listeners_{};
  CallbackManager<void(const std::vector<uint8_t> &, uint8_t, uint16_t, const std::vector<uint8_t> &)>
      frame_callbacks_{};
};

}  // namespace bshdbus
}  // namespace esphome

#include "bshdbus_binary_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace bshdbus {

static const char *const TAG = "bshdbus.binary_sensor";

void BSHDBusBinarySensor::dump_config() {
  ESP_LOGCONFIG(TAG, "BSH D-Bus Binary Sensor:");
  if (this->dest_ == 0xff) {
    ESP_LOGCONFIG(TAG, "  Dest node: ANY");
  } else {
    ESP_LOGCONFIG(TAG, "  Dest node: 0x%02x", this->dest_);
  }
  if (this->command_ == 0xffff) {
    ESP_LOGCONFIG(TAG, "  Command: ANY");
  } else {
    ESP_LOGCONFIG(TAG, "  Command: 0x%04x", this->command_);
  }
  ESP_LOGCONFIG(TAG, "  Binary Sensors:");
  for (BSHDBusSubBinarySensor *bsensor : this->bsensors_) {
    LOG_BINARY_SENSOR("  ", "-", bsensor);
  }
}

void BSHDBusBinarySensor::handle_message(std::vector<uint8_t> &message) {
  for (BSHDBusSubBinarySensor *bsensor : this->bsensors_)
    bsensor->parse_message(message);
}

void BSHDBusSubBinarySensor::parse_message(std::vector<uint8_t> &message) {
  this->publish_state(this->message_parser_(message));
}

}  // namespace bshdbus
}  // namespace esphome

#include "bshdbus_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace bshdbus {

static const char *const TAG = "bshdbus.sensor";

void BSHDBusCustomSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "BSHDBus Custom Sensor:");
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
  ESP_LOGCONFIG(TAG, "  Sensors:");
  for (BSHDBusCustomSubSensor *sensor : this->sensors_) {
    LOG_SENSOR("  ", "-", sensor);
  }
}

void BSHDBusCustomSensor::handle_message(std::vector<uint8_t> &message) {
  for (BSHDBusCustomSubSensor *sensor : this->sensors_)
    sensor->parse_message(message);
}

void BSHDBusCustomSubSensor::parse_message(std::vector<uint8_t> &message) {
  this->publish_state(this->message_parser_(message));
}

}  // namespace bshdbus
}  // namespace esphome

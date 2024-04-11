#include "bshdbus_text_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace bshdbus {

static const char *const TAG = "bshdbus.text_sensor";

void BSHDBusCustomTSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "BSHDBus Custom Text Sensor:");
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
  ESP_LOGCONFIG(TAG, "  Text Sensors:");
  for (BSHDBusCustomSubTSensor *tsensor : this->tsensors_) {
    LOG_TEXT_SENSOR("  ", "-", tsensor);
  }
}

void BSHDBusCustomTSensor::handle_message(std::vector<uint8_t> &message) {
  for (BSHDBusCustomSubTSensor *tsensor : this->tsensors_)
    tsensor->parse_message(message);
}

void BSHDBusCustomSubTSensor::parse_message(std::vector<uint8_t> &message) {
  this->publish_state(this->message_parser_(message));
}

}  // namespace bshdbus
}  // namespace esphome

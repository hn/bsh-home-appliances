#include "bshdbus_text_sensor.h"
#include "esphome/core/log.h"

namespace esphome::bshdbus {

static const char *const TAG = "bshdbus.text_sensor";

void BSHDBusTextSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "BSH D-Bus Text Sensor:");
  this->dump_filter_config(TAG);
  ESP_LOGCONFIG(TAG, "  Text Sensors:");
  for (BSHDBusSubTextSensor *tsensor : this->tsensors_) {
    LOG_TEXT_SENSOR("  ", "-", tsensor);
  }
}

void BSHDBusTextSensor::handle_message(const std::vector<uint8_t> &message) {
  for (BSHDBusSubTextSensor *tsensor : this->tsensors_)
    tsensor->parse_message(message);
}

void BSHDBusSubTextSensor::parse_message(const std::vector<uint8_t> &message) {
  this->publish_state(this->message_parser_(message));
}

}  // namespace esphome::bshdbus

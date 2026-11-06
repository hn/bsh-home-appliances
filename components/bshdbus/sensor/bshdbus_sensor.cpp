#include "bshdbus_sensor.h"
#include "esphome/core/log.h"

namespace esphome::bshdbus {

static const char *const TAG = "bshdbus.sensor";

void BSHDBusSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "BSH D-Bus Sensor:");
  this->dump_filter_config(TAG);
  ESP_LOGCONFIG(TAG, "  Sensors:");
  for (BSHDBusSubSensor *sensor : this->sensors_) {
    LOG_SENSOR("  ", "-", sensor);
  }
}

void BSHDBusSensor::handle_message(const std::vector<uint8_t> &message) {
  for (BSHDBusSubSensor *sensor : this->sensors_)
    sensor->parse_message(message);
}

void BSHDBusSubSensor::parse_message(const std::vector<uint8_t> &message) {
  this->publish_state(this->message_parser_(message));
}

}  // namespace esphome::bshdbus

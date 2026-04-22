#pragma once

#if defined(WIFI_SSID) && defined(MQTT_BROKER)

  #include "DBus2Node.h"
  #include <mqtt_client.h>

namespace DBus2::Nodes {

class MqttGateway : public Node {
public:
  MqttGateway();
  void setup() override;
  void handleFrame(const Frame &f, int64_t rxTimestamp) override;

private:
  void publishFrame(const Frame &f);
  void handleIncomingMqtt(const char *data, int len);

  static void mqttEventHandler(void *arg, esp_event_base_t base, int32_t id, void *data);

  esp_mqtt_client_handle_t _client = nullptr;
  char                     _topicPrefix[40]; // "dbus2/aabbccddeeff"
  bool                     _connected = false;
};

} // namespace DBus2::Nodes

#endif

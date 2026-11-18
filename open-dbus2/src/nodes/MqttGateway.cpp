#if defined(WIFI_SSID) && defined(MQTT_BROKER)

  #include "nodes/MqttGateway.h"
  #include "DBus2Log.h"
  #include <WiFi.h>
  #include <cJSON.h>

static const char *TAG = "MQTT";

namespace DBus2::Nodes {

// --- Helpers -----------------------------------------------------------------

// Convert a byte to two lowercase hex chars
static void byteToHex(uint8_t b, char *out) {
  static const char hex[] = "0123456789abcdef";
  out[0]                  = hex[b >> 4];
  out[1]                  = hex[b & 0x0F];
}

// Convert a byte buffer to a lowercase hex string (caller provides buffer of len*2+1)
static void bytesToHex(const uint8_t *src, size_t len, char *out) {
  for (size_t i = 0; i < len; i++)
    byteToHex(src[i], out + i * 2);
  out[len * 2] = '\0';
}

// Parse a hex string into bytes. Returns number of bytes parsed, or -1 on error.
static int hexToBytes(const char *hex, size_t hexLen, uint8_t *out, size_t outMax) {
  if ((hexLen % 2) != 0)
    return -1;
  size_t count = hexLen / 2;
  if (count > outMax)
    return -1;
  for (size_t i = 0; i < count; i++) {
    char    hi  = hex[i * 2];
    char    lo  = hex[i * 2 + 1];
    uint8_t val = 0;
    if ((hi >= '0') && (hi <= '9'))
      val = (hi - '0') << 4;
    else if ((hi >= 'a') && (hi <= 'f'))
      val = (hi - 'a' + 10) << 4;
    else if ((hi >= 'A') && (hi <= 'F'))
      val = (hi - 'A' + 10) << 4;
    else
      return -1;
    if ((lo >= '0') && (lo <= '9'))
      val |= lo - '0';
    else if ((lo >= 'a') && (lo <= 'f'))
      val |= lo - 'a' + 10;
    else if ((lo >= 'A') && (lo <= 'F'))
      val |= lo - 'A' + 10;
    else
      return -1;
    out[i] = val;
  }
  return (int)count;
}

// --- MQTT event handler ------------------------------------------------------

void MqttGateway::mqttEventHandler(void *arg, esp_event_base_t base, int32_t id, void *data) {
  auto *self  = static_cast<MqttGateway *>(arg);
  auto *event = static_cast<esp_mqtt_event_handle_t>(data);

  switch (id) {
    case MQTT_EVENT_CONNECTED: {
      self->_connected = true;
      DBUS2_LOGI("Connected to broker");

      // Subscribe to TX topic: dbus2/<mac>/tx
      char topic[60];
      snprintf(topic, sizeof(topic), "%s/tx", self->_topicPrefix);
      esp_mqtt_client_subscribe(self->_client, topic, 0);
      DBUS2_LOGI("Subscribed to %s", topic);
      break;
    }
    case MQTT_EVENT_DISCONNECTED:
      self->_connected = false;
      DBUS2_LOGW("Disconnected from broker");
      break;
    case MQTT_EVENT_DATA:
      if (event->data_len > 0)
        self->handleIncomingMqtt(event->data, event->data_len);
      break;
    default:
      break;
  }
}

// --- Command handlers --------------------------------------------------------

void MqttGateway::publishFrame(const Frame &f) {
  if (!_connected)
    return;

  cJSON *json = cJSON_CreateObject();
  if (!json)
    return;

  // dest as 2 hex chars
  char destHex[3];
  byteToHex(f.dest, destHex);
  destHex[2] = '\0';
  cJSON_AddStringToObject(json, "dest", destHex);

  // Topic depends on whether this is a command frame
  char topic[80];

  if (f.data.size() >= 2) {
    // Command frame: add cmd + payload
    char cmdHex[5];
    snprintf(cmdHex, sizeof(cmdHex), "%04x", f.getCommand());
    cJSON_AddStringToObject(json, "cmd", cmdHex);

    auto payload = f.getPayload();
    if (!payload.empty()) {
      char payloadHex[payload.size() * 2 + 1];
      bytesToHex(payload.data(), payload.size(), payloadHex);
      cJSON_AddStringToObject(json, "payload", payloadHex);
    }

    snprintf(topic, sizeof(topic), "%s/rx/%s/%s", _topicPrefix, destHex, cmdHex);
  } else {
    // Non-command frame: add data
    if (!f.data.empty()) {
      char dataHex[f.data.size() * 2 + 1];
      bytesToHex(f.data.data(), f.data.size(), dataHex);
      cJSON_AddStringToObject(json, "data", dataHex);
    }

    snprintf(topic, sizeof(topic), "%s/rx/%s", _topicPrefix, destHex);
  }

  char *payload = cJSON_PrintUnformatted(json);
  if (payload) {
    esp_mqtt_client_publish(_client, topic, payload, 0, 0, 0);
    cJSON_free(payload);
  }
  cJSON_Delete(json);
}

void MqttGateway::handleIncomingMqtt(const char *data, int len) {
  cJSON *json = cJSON_ParseWithLength(data, len);
  if (!json) {
    DBUS2_LOGW("Invalid JSON on TX topic");
    return;
  }

  // Parse dest (required)
  cJSON *destItem = cJSON_GetObjectItem(json, "dest");
  if (!cJSON_IsString(destItem) || (strlen(destItem->valuestring) != 2)) {
    DBUS2_LOGW("TX: missing or invalid \"dest\"");
    cJSON_Delete(json);
    return;
  }
  uint8_t dest;
  if (hexToBytes(destItem->valuestring, 2, &dest, 1) != 1) {
    DBUS2_LOGW("TX: invalid hex in \"dest\"");
    cJSON_Delete(json);
    return;
  }

  // Check for cmd (command frame) or data (raw frame)
  cJSON *cmdItem  = cJSON_GetObjectItem(json, "cmd");
  cJSON *dataItem = cJSON_GetObjectItem(json, "data");

  if (cJSON_IsString(cmdItem)) {
    // Command frame: sendCmd()
    size_t cmdLen = strlen(cmdItem->valuestring);
    if (cmdLen != 4) {
      DBUS2_LOGW("TX: \"cmd\" must be 4 hex chars");
      cJSON_Delete(json);
      return;
    }
    uint8_t cmdBytes[2];
    if (hexToBytes(cmdItem->valuestring, 4, cmdBytes, 2) != 2) {
      DBUS2_LOGW("TX: invalid hex in \"cmd\"");
      cJSON_Delete(json);
      return;
    }
    uint16_t cmd = (cmdBytes[0] << 8) | cmdBytes[1];

    // Parse optional payload
    std::vector<uint8_t> payload;
    cJSON               *payloadItem = cJSON_GetObjectItem(json, "payload");
    if (cJSON_IsString(payloadItem)) {
      size_t pLen = strlen(payloadItem->valuestring);
      if ((pLen % 2) != 0) {
        DBUS2_LOGW("TX: odd-length \"payload\" hex");
        cJSON_Delete(json);
        return;
      }
      payload.resize(pLen / 2);
      if (hexToBytes(payloadItem->valuestring, pLen, payload.data(), payload.size()) < 0) {
        DBUS2_LOGW("TX: invalid hex in \"payload\"");
        cJSON_Delete(json);
        return;
      }
    }

    DBUS2_LOGD("TX cmd 0x%04X to 0x%02X (%d payload bytes)", cmd, dest, payload.size());
    sendCmd(dest, cmd, std::move(payload));

  } else if (cJSON_IsString(dataItem)) {
    // Raw frame: send()
    size_t dLen = strlen(dataItem->valuestring);
    if ((dLen % 2) != 0) {
      DBUS2_LOGW("TX: odd-length \"data\" hex");
      cJSON_Delete(json);
      return;
    }
    std::vector<uint8_t> rawData(dLen / 2);
    if (hexToBytes(dataItem->valuestring, dLen, rawData.data(), rawData.size()) < 0) {
      DBUS2_LOGW("TX: invalid hex in \"data\"");
      cJSON_Delete(json);
      return;
    }

    DBUS2_LOGD("TX raw to 0x%02X (%d data bytes)", dest, rawData.size());
    send(dest, std::move(rawData));

  } else {
    DBUS2_LOGW("TX: need \"cmd\" or \"data\"");
  }

  cJSON_Delete(json);
}

// --- Lifecycle ---------------------------------------------------------------

MqttGateway::MqttGateway() : Node(0x9, "MQTT Gateway") {
  setPromiscuous(true);
}

void MqttGateway::setup() {
  Node::setup();

  // Build topic prefix from WiFi MAC: "dbus2/aabbccddeeff"
  String mac = WiFi.macAddress(); // "AA:BB:CC:DD:EE:FF"
  int    pos = 0;
  pos += snprintf(_topicPrefix + pos, sizeof(_topicPrefix) - pos, "dbus2/");
  for (int i = 0; i < (int)mac.length(); i++) {
    if (mac[i] != ':') {
      _topicPrefix[pos++] = tolower(mac[i]);
      if (pos >= (int)sizeof(_topicPrefix) - 1)
        break;
    }
  }
  _topicPrefix[pos] = '\0';

  DBUS2_LOGI("Topic prefix: %s", _topicPrefix);

  // Configure MQTT client
  esp_mqtt_client_config_t cfg = {};
  char                     uri[80];

  #ifdef MQTT_PORT
  snprintf(uri, sizeof(uri), "mqtt://" MQTT_BROKER ":%d", MQTT_PORT);
  #else
  snprintf(uri, sizeof(uri), "mqtt://" MQTT_BROKER);
  #endif
  cfg.broker.address.uri = uri;

  #ifdef MQTT_USER
  cfg.credentials.username = MQTT_USER;
  #endif
  #ifdef MQTT_PASS
  cfg.credentials.authentication.password = MQTT_PASS;
  #endif

  _client = esp_mqtt_client_init(&cfg);
  if (!_client) {
    DBUS2_LOGE("Failed to init MQTT client");
    return;
  }

  esp_mqtt_client_register_event(_client, MQTT_EVENT_ANY, mqttEventHandler, this);
  esp_mqtt_client_start(_client);
  DBUS2_LOGI("Client started, broker: %s", uri);
}

void MqttGateway::handleFrame(const Frame &f, int64_t rxTimestamp) {
  publishFrame(f);
}

} // namespace DBus2::Nodes

#endif

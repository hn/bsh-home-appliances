#include "nodes/PingDemo.h"
#include "DBus2Log.h"

static const char *TAG = "Ping";

namespace DBus2::Nodes {

// --- Command handlers ---------------------------------------------------

void PingDemo::handlePong(const Frame &f) {
  auto payload = f.getPayload();
  if (payload.empty()) {
    DBUS2_LOGW("PONG frame has no payload");
    return;
  }
  uint8_t counter = payload[0];
  DBUS2_LOGI("PONG #%d received from Node 0x%X", counter, f.getNodeId());
}

void PingDemo::handlePingControl(const Frame &f) {
  auto payload = f.getPayload();
  if (payload.empty()) {
    DBUS2_LOGW("PING_CONTROL frame has no payload");
    return;
  }
  _pingEnabled = (payload[0] != 0);
  DBUS2_LOGI("Ping %s", _pingEnabled ? "enabled" : "disabled");
}

// --- Lifecycle ----------------------------------------------------------

PingDemo::PingDemo() : Node(0x7, "Ping Demo") {
  // Subsystem 1: Ping Service
  _sub1.registerCommand(0x5678, DBUS2_HANDLE(handlePong));
  _sub1.registerCommand(0x7777, DBUS2_HANDLE(handlePingControl));
  registerSubsystem(&_sub1);
}

void PingDemo::loop() {
  Node::loop();
  if (_pingEnabled && ((millis() - _lastPing) > 5000)) {
    static uint8_t counter = 0;
    DBUS2_LOGI("Sending PING #%d to 0x%02X", counter, 0x81);
    _lastPing = millis();
    sendCmd(0x81, 0x1234, {counter++});
  }
}

} // namespace DBus2::Nodes

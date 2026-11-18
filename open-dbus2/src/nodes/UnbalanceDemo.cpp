#include "nodes/UnbalanceDemo.h"
#include "DBus2Log.h"

static const char *TAG = "Unbal";

namespace DBus2::Nodes {

// --- Command handlers ---------------------------------------------------

void UnbalanceDemo::handleResponse(const Frame &f) {
  auto payload = f.getPayload();
  if (payload.size() < 7) {
    DBUS2_LOGW("Unbalance response too short (%d bytes)", payload.size());
    return;
  }
  int16_t x = (int16_t)((payload[1] << 8) | payload[2]);
  int16_t y = (int16_t)((payload[3] << 8) | payload[4]);
  int16_t z = (int16_t)((payload[5] << 8) | payload[6]);
  DBUS2_LOGI("X=%d Y=%d Z=%d", x, y, z);
}

void UnbalanceDemo::handleControl(const Frame &f) {
  auto payload = f.getPayload();
  if (payload.empty()) {
    DBUS2_LOGW("Control frame has no payload");
    return;
  }
  _requestEnabled = (payload[0] != 0);
  DBUS2_LOGI("Unbalance requests %s", _requestEnabled ? "enabled" : "disabled");
}

// --- Lifecycle ----------------------------------------------------------

UnbalanceDemo::UnbalanceDemo() : Node(0x5, "Unbalance Demo") {
  // Subsystem 7: Unbalance Service
  _sub1.registerCommand(0x4010, DBUS2_HANDLE(handleResponse));
  _sub1.registerCommand(0x7777, DBUS2_HANDLE(handleControl));
  registerSubsystem(&_sub1);
}

void UnbalanceDemo::loop() {
  Node::loop();
  if (_requestEnabled && ((millis() - _lastRequest) > 15000)) {
    _lastRequest = millis();
    sendCmd(0x47, 0x4002, {0x57, 0x80, 0x05});
  }
}

} // namespace DBus2::Nodes

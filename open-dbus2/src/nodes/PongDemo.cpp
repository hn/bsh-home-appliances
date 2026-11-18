#include "nodes/PongDemo.h"
#include "DBus2Log.h"

static const char *TAG = "Pong";

namespace DBus2::Nodes {

// --- Command handlers ---------------------------------------------------

void PongDemo::handlePing(const Frame &f) {
  auto payload = f.getPayload();
  if (payload.empty()) {
    DBUS2_LOGW("PING frame has no payload");
    return;
  }
  uint8_t counter = payload[0];
  DBUS2_LOGI("PING #%d from Node 0x%X, sending PONG #%d", counter, f.getNodeId(), counter);
  sendCmd(0x71, 0x5678, {counter});
}

// --- Lifecycle ----------------------------------------------------------

PongDemo::PongDemo() : Node(0x8, "Pong Demo") {
  // Subsystem 1: Pong Service
  _sub1.registerCommand(0x1234, DBUS2_HANDLE(handlePing));
  registerSubsystem(&_sub1);
}

} // namespace DBus2::Nodes

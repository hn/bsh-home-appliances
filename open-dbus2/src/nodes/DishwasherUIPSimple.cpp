#include "nodes/DishwasherUIPSimple.h"
#include "DBus2Log.h"

static const char *TAG = "DWUIPSim";

namespace DBus2::Nodes {

// --- Command handlers ---------------------------------------------------

void DishwasherUIPSimple::handlePresence(const Frame &f) {
  auto payload = f.getPayload();
  if (payload.empty()) {
    DBUS2_LOGW("Presence check has no payload");
    return;
  }
  DBUS2_LOGI("Presence check from 0x%02X", payload[0]);
}

// --- Lifecycle ----------------------------------------------------------

DishwasherUIPSimple::DishwasherUIPSimple() : Node(0x2, "Dishwasher UIP Simple") {
  // Subsystem 2: Basic Functions
  _sub2.registerCommand(0x7ff1, DBUS2_HANDLE(handlePresence));
  registerSubsystem(&_sub2);
}

} // namespace DBus2::Nodes

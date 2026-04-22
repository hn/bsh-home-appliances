#include "nodes/LedStatus.h"
#include "DBus2Log.h"

static const char *TAG = "LED";

namespace DBus2::Nodes {

// --- Lifecycle ---------------------------------------------------------------

LedStatus::LedStatus() : Node(0xD1, "LED Status") {
  setPromiscuous(true);
}

void LedStatus::setup() {
  Node::setup();
#ifdef DBUS_PIN_LED_HEARTBEAT
  pinMode(DBUS_PIN_LED_HEARTBEAT, OUTPUT);
#endif
#ifdef DBUS_PIN_LED_ACTIVITY
  pinMode(DBUS_PIN_LED_ACTIVITY, OUTPUT);
#endif
}

void LedStatus::loop() {
  Node::loop();

  // Heartbeat: 1 Hz blink
#ifdef DBUS_PIN_LED_HEARTBEAT
  uint32_t now = millis();
  if (now - _lastHeartbeat >= 1000) {
    _lastHeartbeat  = now;
    _heartbeatState = !_heartbeatState;
    digitalWrite(DBUS_PIN_LED_HEARTBEAT, _heartbeatState);
  }
#endif

  // Activity: turn off after 100 ms pulse
#ifdef DBUS_PIN_LED_ACTIVITY
  if (_activityState && (millis() - _activityOnTime >= 100)) {
    _activityState = false;
    digitalWrite(DBUS_PIN_LED_ACTIVITY, LOW);
  }
#endif
}

void LedStatus::handleFrame(const Frame &f, int64_t rxTimestamp) {
  // Activity LED pulse on any bus frame
#ifdef DBUS_PIN_LED_ACTIVITY
  _activityOnTime = millis();
  if (!_activityState) {
    _activityState = true;
    digitalWrite(DBUS_PIN_LED_ACTIVITY, HIGH);
  }
#endif
}

} // namespace DBus2::Nodes

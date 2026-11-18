#pragma once

#include "DBus2Node.h"

namespace DBus2::Nodes {

class LedStatus : public Node {
public:
  LedStatus();
  void setup() override;
  void loop() override;
  void handleFrame(const Frame &f, int64_t rxTimestamp) override;

private:
  uint32_t _lastHeartbeat  = 0;
  bool     _heartbeatState = false;

  uint32_t _activityOnTime = 0;
  bool     _activityState  = false;
};

} // namespace DBus2::Nodes

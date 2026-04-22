#pragma once

#include "DBus2Node.h"

namespace DBus2::Nodes {

class PingDemo : public Node {
public:
  PingDemo();
  void loop() override;

private:
  // Subsystem 1: Ping Service
  void handlePong(const Frame &f);
  void handlePingControl(const Frame &f);

  Subsystem _sub1{1, "Ping Service"};

  uint32_t _lastPing    = 0;
  bool     _pingEnabled = true;
};

} // namespace DBus2::Nodes

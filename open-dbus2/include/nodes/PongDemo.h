#pragma once

#include "DBus2Node.h"

namespace DBus2::Nodes {

class PongDemo : public Node {
public:
  PongDemo();

private:
  // Subsystem 1: Pong Service
  void handlePing(const Frame &f);

  Subsystem _sub1{1, "Pong Service"};
};

} // namespace DBus2::Nodes

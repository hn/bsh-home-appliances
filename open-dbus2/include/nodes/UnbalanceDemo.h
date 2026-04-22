#pragma once

#include "DBus2Node.h"

namespace DBus2::Nodes {

class UnbalanceDemo : public Node {
public:
  UnbalanceDemo();
  void loop() override;

private:
  // Subsystem 7: Unbalance Service
  void handleResponse(const Frame &f);
  void handleControl(const Frame &f);

  Subsystem _sub1{7, "Unbalance Service"};

  uint32_t _lastRequest    = 0;
  bool     _requestEnabled = true;
};

} // namespace DBus2::Nodes

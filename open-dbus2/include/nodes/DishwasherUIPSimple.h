#pragma once

#include "DBus2Node.h"

namespace DBus2::Nodes {

class DishwasherUIPSimple : public Node {
public:
  DishwasherUIPSimple();

private:
  // Subsystem 2: Basic Functions
  void handlePresence(const Frame &f);

  Subsystem _sub2{2, "Basic Functions"};
};

} // namespace DBus2::Nodes

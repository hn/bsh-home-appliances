#pragma once

#include "DBus2Node.h"

namespace DBus2::Nodes {

class Monitor : public Node {
public:
  Monitor();
  void handleFrame(const Frame &f, int64_t rxTimestamp) override;
};

} // namespace DBus2::Nodes

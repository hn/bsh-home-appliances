#pragma once

#include "DBus2Node.h"

namespace DBus2::Nodes {

class Dishwasher39C3 : public Node {
public:
  Dishwasher39C3();
  void setup() override;
  void loop() override;

private:
  // Subsystem 1: 39C3 Demo
  // (no incoming commands)

  Subsystem _sub1{1, "39C3 Demo"};

  uint32_t _lastTick = 0;
  uint16_t _tick     = 0;
  char     _state    = '0';

  static constexpr uint8_t PIN_BLUE   = 8;
  static constexpr uint8_t PIN_WHITE  = 9;
  static constexpr uint8_t CANVAS_LEN = 9;
};

} // namespace DBus2::Nodes

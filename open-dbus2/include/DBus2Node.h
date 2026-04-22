#pragma once

#include "DBus2Subsystem.h"
#include <Arduino.h>

namespace DBus2 {

class Manager;

class Node {
  friend class Subsystem;

public:
  Node(uint8_t id, const char *name = "") : _id(id), _name(name) {
  }

  // Default: delegates to all registered subsystems.
  // Override and call Node::setup() to keep subsystem lifecycle.
  virtual void setup() {
    for (auto &[id, s] : _subsystems)
      s->setup();
  }

  // Default: delegates to all registered subsystems.
  // Override and call Node::loop() to keep subsystem lifecycle.
  virtual void loop() {
    for (auto &[id, s] : _subsystems)
      s->loop();
  }

  // Default: dispatch to matching subsystem.
  // Override for custom routing or pre-processing.
  virtual void handleFrame(const Frame &f, int64_t rxTimestamp);

  uint8_t getNodeId() const {
    return _id;
  }
  const char *getName() const {
    return _name;
  }
  const std::map<uint8_t, Subsystem *> &getSubsystems() const {
    return _subsystems;
  }
  void setManager(Manager *m) {
    _manager = m;
  }
  void setPromiscuous(bool p) {
    _promiscuous = p;
  }
  bool isPromiscuous() const {
    return _promiscuous;
  }
  bool isObserver() const {
    return _id > 0xF;
  }

protected:
  uint8_t     _id;
  const char *_name        = "";
  Manager    *_manager     = nullptr;
  bool        _promiscuous = false;

  std::map<uint8_t, Subsystem *> _subsystems;

  void registerSubsystem(Subsystem *s) {
    s->_node                = this;
    _subsystems[s->getId()] = s;
  }

  bool send(uint8_t dest, std::vector<uint8_t> data, bool blocking = true, int retries = 3);
  bool sendCmd(uint8_t dest, uint16_t cmd, std::vector<uint8_t> payload = {}, bool blocking = true, int retries = 3);
};

// Defined here because Node is only forward-declared in DBus2Subsystem.h
inline uint8_t Subsystem::getAddress() const {
  return (_node->getNodeId() << 4) | _id;
}

} // namespace DBus2

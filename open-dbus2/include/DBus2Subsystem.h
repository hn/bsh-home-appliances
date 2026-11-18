#pragma once

#include "DBus2Frame.h"
#include <functional>
#include <initializer_list>
#include <map>
#include <utility>
#include <vector>

namespace DBus2 {

class Node;

class Subsystem {
  friend class Node;

public:
  using CommandCb    = std::function<void(const Frame &)>;
  using CommandEntry = std::pair<uint16_t, CommandCb>;

  Subsystem(uint8_t id, const char *name = "") : _id(id), _name(name) {
  }

  Subsystem(uint8_t id, const char *name, std::initializer_list<CommandEntry> commands) : _id(id), _name(name) {
    for (auto &[cmd, cb] : commands)
      _commands[cmd] = cb;
  }

  void registerCommand(uint16_t cmd, CommandCb cb) {
    _commands[cmd] = cb;
  }

  uint8_t getId() const {
    return _id;
  }
  const char *getName() const {
    return _name;
  }
  uint8_t getAddress() const;
  size_t  getCommandCount() const {
    return _commands.size();
  }

  virtual void setup() {
  }
  virtual void loop() {
  }

  // Default: dispatch to registered command handler.
  // Override for custom routing or fallback handling.
  virtual void handleFrame(const Frame &f, int64_t rxTimestamp);

protected:
  uint8_t     _id;
  const char *_name = "";
  Node       *_node = nullptr;

  std::map<uint16_t, CommandCb> _commands;

  bool send(uint8_t dest, std::vector<uint8_t> data, bool blocking = true, int retries = 3);
  bool sendCmd(uint8_t dest, uint16_t cmd, std::vector<uint8_t> payload = {}, bool blocking = true, int retries = 3);
};
} // namespace DBus2

// Convenience macro for command tables in Node constructors
#define DBUS2_HANDLE(fn)                                                                                               \
  [this](const DBus2::Frame &f) {                                                                                      \
    fn(f);                                                                                                             \
  }

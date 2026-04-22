#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace DBus2 {
struct Frame {
  uint8_t length = 0;
  uint8_t dest   = 0;

  std::vector<uint8_t> data;

  uint8_t getNodeId() const {
    return (dest >> 4) & 0x0F;
  }
  uint8_t getSubsystemId() const {
    return dest & 0x0F;
  }
  bool isBroadcast() const {
    return getNodeId() == 0;
  }

  uint16_t getCommand() const {
    if (data.size() < 2)
      return 0;
    return (data[0] << 8) | data[1];
  }

  std::span<const uint8_t> getPayload() const {
    return data.size() < 2 ? std::span<const uint8_t>{} : std::span<const uint8_t>(data).subspan(2);
  }
};
} // namespace DBus2

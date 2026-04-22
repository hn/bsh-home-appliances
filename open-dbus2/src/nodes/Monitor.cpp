#include "nodes/Monitor.h"
#include "DBus2Log.h"

static const char *TAG = "Monitor";

namespace DBus2::Nodes {

// --- Lifecycle ---------------------------------------------------------------

Monitor::Monitor() : Node(0xD0, "Bus Monitor") {
  setPromiscuous(true);
}

void Monitor::handleFrame(const Frame &f, int64_t rxTimestamp) {
  char buf[800];
  int  pos = 0;

  int64_t ms  = rxTimestamp / 1000;
  int64_t us3 = rxTimestamp % 1000;
  pos += snprintf(buf + pos, sizeof(buf) - pos, "%8lld.%03lld ", ms, us3);

  pos += snprintf(buf + pos, sizeof(buf) - pos, "Len:%02x Dst:%02x ", f.length, f.dest);

  if (f.data.size() >= 2) {
    // Command frame
    auto payload = f.getPayload();
    pos += snprintf(buf + pos, sizeof(buf) - pos, "Cmd:%04x ", f.getCommand());
    if (!payload.empty()) {
      pos += snprintf(buf + pos, sizeof(buf) - pos, "Data:");
      for (auto b : payload)
        pos += snprintf(buf + pos, sizeof(buf) - pos, " %02x", b);
    }
  } else {
    // Non-command frame
    if (!f.data.empty()) {
      pos += snprintf(buf + pos, sizeof(buf) - pos, "Data:");
      for (uint8_t b : f.data)
        pos += snprintf(buf + pos, sizeof(buf) - pos, " %02x", b);
    }
  }

  DBUS2_LOGI("%s", buf);
}

} // namespace DBus2::Nodes

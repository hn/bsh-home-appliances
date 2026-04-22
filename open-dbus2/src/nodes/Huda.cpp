/*

  Since I can never come up with names for anything, I foolishly
  called this module HUDA. It stands for Hacker's UDA,
  which is a nod to B/S/H/'s 'Universal Diagnostic Adapter.

  Essentially, it's a basic toolbox designed for
  tinkering with DBus2 participants.

*/

#define DBUS2_LOG_TAG "HUDA"
#include "nodes/Huda.h"
#include "DBus2Log.h"
#include "DBus2Manager.h"
#include <cstdlib>
#if defined(WIFI_SSID)
  #include <WiFi.h>
#endif

static const char *TAG = "HUDA";

namespace DBus2::Nodes {

#define CMD16(s) ((uint16_t)((uint16_t)(s)[0] << 8 | (uint16_t)(s)[1]))

static constexpr uint16_t CMD_READ16_REQUEST  = 0xF000;
static constexpr uint16_t CMD_READ32_REQUEST  = 0xF001;
static constexpr uint16_t CMD_READ16_RESPONSE = 0xF100;
static constexpr uint16_t CMD_READ32_RESPONSE = 0xF101;

static constexpr uint16_t CMD_WRITE16_REQUEST = 0xF200;
static constexpr uint16_t CMD_WRITE32_REQUEST = 0xF201;

static constexpr uint16_t CMD_ID_REQUEST  = 0xFF00;
static constexpr uint16_t CMD_ID_RESPONSE = 0xFE00;

static constexpr uint16_t CMD_SET_MODULE = 0xF300;

static constexpr uint16_t CMD_FLASHPAGESIZE_REQUEST  = 0xF400;
static constexpr uint16_t CMD_FLASHPAGESIZE_RESPONSE = 0xF500;

static constexpr uint16_t CMD_NODE_RESTART    = 0xFD00;
static constexpr uint16_t CMD_NODE_BOOTLOADER = 0xFDFF;

// --- Variable and action descriptor tables --------------------------------

// Mutable: value field is updated at runtime
Huda::VarEntry Huda::_vars[] = {
    // clang-format off
    {HudaVar::NODE_NUMBER, HudaSection::NODE, "number", HudaVarType::HEXVAL, "nn", "Sets node number", 0xB},
    {HudaVar::NODE_RESTART_DELAY, HudaSection::NODE, "restartdelay", HudaVarType::DECVAL, "nd", "Sets restart delay", 2},
    {HudaVar::NODE_SILENCE_DURATION, HudaSection::NODE, "silenceduration", HudaVarType::DECVAL, "ns", "Sets silence duration", 5},
    {HudaVar::NODE_BITS, HudaSection::NODE, "bits", HudaVarType::DECVAL, nullptr, "Shows MCU bit size", 0},
    {HudaVar::MODULE_NUMBER, HudaSection::MODULE, "number", HudaVarType::DECVAL, "mn", "Sets module number", 0},
    {HudaVar::MODULE_IDSTRING, HudaSection::MODULE, "idstring", HudaVarType::MEMADDR, nullptr, "Shows memory address of module id string", 0},
    {HudaVar::MODULE_PAGESIZE, HudaSection::MODULE, "pagesize", HudaVarType::HEXVAL, nullptr, "Shows flash pagesize of module", 0},
    {HudaVar::MEMORY_BEGIN, HudaSection::MEMORY, "begin", HudaVarType::MEMADDR, "mb", "Set memory begin address for next action", 0x08000000},
    {HudaVar::MEMORY_END, HudaSection::MEMORY, "end", HudaVarType::MEMADDR, "me", "Set memory end address for next action", 0x08000100},
    {HudaVar::MEMORY_POS, HudaSection::MEMORY, "pos", HudaVarType::MEMADDR, "mp", "Set memory position for next action", 0},
    {HudaVar::MEMORY_CHUNK, HudaSection::MEMORY, "chunk", HudaVarType::DECVAL, "mc", "Set memory chunk size for next read", 16},
    {HudaVar::MEMORY_SAVEFILE, HudaSection::MEMORY, "savefile", HudaVarType::BOOLVAL, "ms", "Save dump as binary file in browser", 0},
    {HudaVar::GENERAL_RAW_MODE, HudaSection::GENERAL, "rawmode", HudaVarType::BOOLVAL, "rm", "Toggle raw mode", 0},
    {HudaVar::GENERAL_BAUD, HudaSection::GENERAL, "baud", HudaVarType::DECVAL, "bb", "Bus baud rate (9600/19200/38400)", 9600},
    // clang-format on
};

// Immutable: no runtime state, lives in .rodata
const Huda::ActionEntry Huda::_actions[] = {
    {HudaSection::NODE, "info", "ni", nullptr, "Requests basic info", &Huda::actionInfo},
    {HudaSection::NODE, "restart", "nr", nullptr, "Restart node", &Huda::actionRestart},
    {HudaSection::NODE, "bootloader", "nb", nullptr, "Enter bootloader mode", &Huda::actionBootloader},
    {HudaSection::MODULE, "flashinfo", "mf", nullptr, "Requests to read flash pagesize", &Huda::actionFlashInfo},
    {HudaSection::MEMORY, "dump", "md", nullptr, "Dump memory at current position", &Huda::actionDump},
    {HudaSection::MEMORY, "write", "mw", "hexstring", "Write to memory at current pos", &Huda::actionWrite},
    {HudaSection::GENERAL, "help", "?", nullptr, "Request this help", &Huda::actionHelp},
};

// --- Helper functions ---------------------------------------------------

static size_t hex2bin(const char *src, size_t src_len, uint8_t *dest, size_t dest_max) {
  size_t processed = 0;
  size_t written   = 0;

  for (size_t i = 0; i < src_len; i++) {
    char c = src[i];

    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
      if (written >= dest_max)
        break;
      uint8_t nibble = (c % 32 + 9) % 25;
      if (processed % 2 == 0) {
        dest[written] = nibble << 4;
      } else {
        dest[written] |= nibble;
        written++;
      }
      processed++;
    }
  }
  return written;
}

// --- Variable accessors -------------------------------------------------

void Huda::setVarByKey(HudaVar var, uint32_t value) {
  for (auto &v : _vars) {
    if (v.var == var) {
      v.value = value;
      return;
    }
  }
}

uint32_t Huda::getVar(HudaVar var) const {
  for (const auto &v : _vars) {
    if (v.var == var)
      return v.value;
  }
  return 0;
}

void Huda::setVar(VarEntry &v, const char *arg) {
  switch (v.type) {
    case HudaVarType::HEXVAL:
    case HudaVarType::MEMADDR:
      v.value = strtoul(arg, nullptr, 16);
      break;
    case HudaVarType::DECVAL:
      v.value = strtoul(arg, nullptr, 10);
      break;
    case HudaVarType::BOOLVAL:
      v.value = (strtoul(arg, nullptr, 10) != 0) ? 1 : 0;
      break;
  }

  switch (v.var) {
    case HudaVar::NODE_NUMBER:
      setVarByKey(HudaVar::NODE_BITS, 0);
      sendCmd((uint8_t)(getVar(HudaVar::NODE_NUMBER) << 4), CMD_ID_REQUEST, {_sub1.getAddress()});
      break;
    case HudaVar::MODULE_NUMBER:
      sendCmd((uint8_t)(getVar(HudaVar::NODE_NUMBER) << 4), CMD_SET_MODULE, {(uint8_t)v.value});
      sendCmd((uint8_t)(getVar(HudaVar::NODE_NUMBER) << 4), CMD_ID_REQUEST, {_sub1.getAddress()});
      break;
    case HudaVar::MEMORY_BEGIN:
      setVarByKey(HudaVar::MEMORY_POS, 0);
      break;
    case HudaVar::NODE_SILENCE_DURATION:
      if (v.value < 2)
        v.value = 2;
      else if (v.value > 63)
        v.value = 63;
      break;
    case HudaVar::GENERAL_RAW_MODE:
      _manager->setRawMode(v.value);
      _rawMode = v.value;
      if (!v.value)
        _rawRxBufPos = 0;
      break;
    case HudaVar::GENERAL_BAUD:
      if ((v.value != 9600) && (v.value != 19200) && (v.value != 38400))
        v.value = 9600;
      _manager->setBaud(v.value);
      break;
    default:
      break;
  }

  printBannerSection(v.section);
}

// --- Output helpers -----------------------------------------------------

static const char *sectionName(HudaSection section) {
  switch (section) {
    case HudaSection::NODE:
      return "NODE";
    case HudaSection::MODULE:
      return "MODULE";
    case HudaSection::MEMORY:
      return "MEMORY";
    case HudaSection::GENERAL:
      return "GENERAL";
  }
  return "";
}

static const char *varTypeName(HudaVarType type) {
  switch (type) {
    case HudaVarType::HEXVAL:
      return "hex";
    case HudaVarType::DECVAL:
      return "dec";
    case HudaVarType::MEMADDR:
      return "memaddr";
    case HudaVarType::BOOLVAL:
      return "bool";
  }
  return "";
}

void Huda::printBannerSection(HudaSection section) const {
  console.printf("C [%s] %s", TAG, sectionName(section));
  for (const auto &v : _vars) {
    if (v.section != section)
      continue;
    switch (v.type) {
      case HudaVarType::HEXVAL:
        console.printf(" %s=0x%X", v.name, v.value);
        break;
      case HudaVarType::MEMADDR:
        console.printf(" %s=0x%06lx", v.name, (unsigned long)v.value);
        break;
      case HudaVarType::DECVAL:
        console.printf(" %s=%u", v.name, v.value);
        break;
      case HudaVarType::BOOLVAL:
        console.printf(" %s=%u", v.name, v.value);
        break;
    }
  }
  console.printf("\r\n");
}

void Huda::printBanner() const {
#if defined(WIFI_SSID)
  if (WiFi.status() == WL_CONNECTED) {
    console.printf("C [%s] WIFI ip=%s mac=%s\r\n", TAG, WiFi.localIP().toString().c_str(), WiFi.macAddress().c_str());
  }
#endif
  printBannerSection(HudaSection::NODE);
  printBannerSection(HudaSection::MODULE);
  printBannerSection(HudaSection::MEMORY);
  printBannerSection(HudaSection::GENERAL);
}

void Huda::tickHelp() {
  if (_helpIdx < 0)
    return;

  // Rate-limit: one line every 20 ms to avoid overwhelming the WebSocket queue
  static constexpr int64_t HELP_INTERVAL_US = 20000;
  int64_t                  now              = esp_timer_get_time();
  if ((now - _helpLastUs) < HELP_INTERVAL_US)
    return;
  _helpLastUs = now;

  static const char *fmt  = "C [%s] HELP %-7s  %-4s  %-12s  %-3s  %-9s  %s\r\n";
  static const int   nVar = (int)(sizeof(_vars) / sizeof(_vars[0]));
  static const int   nAct = (int)(sizeof(_actions) / sizeof(_actions[0]));

  if (_helpIdx == 0) {
    console.printf(fmt, TAG, "SECTION", "TYPE", "NAME", "CMD", "FORMAT", "DESCRIPTION");
  } else if (_helpIdx <= nVar) {
    const auto &v = _vars[_helpIdx - 1];
    console.printf(fmt, TAG, sectionName(v.section), "VAR", v.name, v.cmd ? v.cmd : "-", varTypeName(v.type), v.desc);
  } else if (_helpIdx <= nVar + nAct) {
    const auto &a = _actions[_helpIdx - nVar - 1];
    console.printf(fmt, TAG, sectionName(a.section), "ACT", a.name, a.cmd, a.argHint ? a.argHint : "-", a.desc);
  } else {
    _helpIdx = -1; // done
    return;
  }

  _helpIdx++;
}

// --- Action handlers ----------------------------------------------------

void Huda::actionInfo(const char *arg) {
  (void)arg;
  sendCmd((uint8_t)(getVar(HudaVar::NODE_NUMBER) << 4), CMD_ID_REQUEST, {_sub1.getAddress()});
}

void Huda::actionRestart(const char *arg) {
  (void)arg;
  sendCmd((uint8_t)(getVar(HudaVar::NODE_NUMBER) << 4), CMD_NODE_RESTART,
          {(uint8_t)getVar(HudaVar::NODE_RESTART_DELAY)});
}

void Huda::actionBootloader(const char *arg) {
  (void)arg;
  sendCmd(0x00, CMD_NODE_BOOTLOADER,
          {(uint8_t)(getVar(HudaVar::NODE_NUMBER) << 4), (uint8_t)getVar(HudaVar::NODE_RESTART_DELAY),
           (uint8_t)getVar(HudaVar::NODE_SILENCE_DURATION)});
  _manager->setRawMode(true, true);
  _rawMode = true;
  setVarByKey(HudaVar::GENERAL_RAW_MODE, 1);
  printBannerSection(HudaSection::GENERAL);

  auto carryChecksum = [](const uint8_t *data, size_t len) -> uint8_t {
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) {
      uint16_t temp = (uint16_t)sum + data[i];
      sum           = (temp > 0xFF) ? (uint8_t)(temp + 1) : (uint8_t)temp;
    }
    return sum;
  };

  uint8_t frameBuf[34];
  uint8_t framePos      = 0;
  uint8_t frameExpected = 1;
  uint8_t b;
  console.printf("RX:");
  while (true) {
    if (_manager->readRaw(&b, 1, 150) != 1)
      break;

    console.printf(" %02X", b);

    if (framePos < sizeof(frameBuf))
      frameBuf[framePos++] = b;

    if ((framePos > 1) && (framePos < frameExpected))
      continue;

    // Responses longer than 1 byte use a carry checksum over the payload only (excluding
    // the command byte), e.g. BL_OK_RESPONSE: carryChecksum(frameBuf + 1, framePos - 2).
    // Requests use a carry checksum over command + payload (e.g. BL_ID_REQUEST).
    switch (frameBuf[0]) {
      case 0x54: // BL_READY_MESSAGE
        console.printf("\r\n[BL_READY_MESSAGE]\r\n");
        _manager->writeRaw((const uint8_t[]){0x5B, 0x00, 0x00, 0x00, 0x5B}, 5);
        frameExpected = 1;
        break;
      case 0x5B: // BL_ID_REQUEST
        if (framePos == 1) {
          frameExpected = 1 + 3 + 1;
          continue;
        }
        {
          bool csumOk = (frameBuf[4] == carryChecksum(frameBuf, 4));
          console.printf("\r\n[BL_ID_REQUEST] (CHECKSUM=%s)\r\n", csumOk ? "OK" : "FAIL");
          frameExpected = csumOk ? (1 + 32 + 1) : 1;
        }
        break;
      case 0xA0: // BL_OK_RESPONSE
        if (framePos < frameExpected) {
          continue;
        }
        if (framePos < (1 + 1 + 1)) {
          console.printf("\r\n[BL_OK_RESPONSE]\r\n");
          break;
        }
        console.printf("\r\n[BL_OK_RESPONSE] (CHECKSUM=%s) \"",
                       (frameBuf[framePos - 1] == carryChecksum(frameBuf + 1, framePos - 2)) ? "OK" : "FAIL");
        for (uint8_t i = 1; i <= (framePos - 2); i++) {
          uint8_t c = frameBuf[i];
          console.write(((c >= 32) && (c <= 126)) ? c : '.');
        }
        console.printf("\"\r\n");
        frameExpected = 1;
        break;
      case 0x06: // BL_ERR_RESPONSE
        console.printf("\r\n[BL_ERR_RESPONSE]\r\n");
        frameExpected = 1;
        break;
      case 0x01: // BL_UNKNOWN_CMD_RESPONSE
        console.printf("\r\n[BL_UNKNOWN_CMD_RESPONSE]\r\n");
        frameExpected = 1;
        break;
      default:
        console.printf("\r\n[BL_UNKNOWN 0x%02X]\r\n", frameBuf[0]);
        frameExpected = 1;
        break;
    }

    framePos = 0;
    console.printf("RX:");
  }
  console.printf("\r\n");
  _manager->setRawMode(false);
  _rawMode = false;
  setVarByKey(HudaVar::GENERAL_RAW_MODE, 0);
  printBannerSection(HudaSection::GENERAL);
}

void Huda::actionFlashInfo(const char *arg) {
  (void)arg;
  sendCmd((uint8_t)(getVar(HudaVar::NODE_NUMBER) << 4), CMD_FLASHPAGESIZE_REQUEST, {_sub1.getAddress()});
}

void Huda::actionDump(const char *arg) {
  (void)arg;
  uint32_t bits = getVar(HudaVar::NODE_BITS);
  if (bits == 0) {
    console.printf("C [%s] Error: Check node bits first with :ni\r\n", TAG);
    return;
  }
  uint32_t memoryNow = getVar(HudaVar::MEMORY_BEGIN) + getVar(HudaVar::MEMORY_POS);
  uint8_t  chunk     = (uint8_t)getVar(HudaVar::MEMORY_CHUNK);
  if (bits == 16) {
    sendCmd((uint8_t)(getVar(HudaVar::NODE_NUMBER) << 4), CMD_READ16_REQUEST,
            {_sub1.getAddress(), chunk, (uint8_t)(memoryNow >> 8), (uint8_t)memoryNow});
  } else {
    sendCmd((uint8_t)(getVar(HudaVar::NODE_NUMBER) << 4), CMD_READ32_REQUEST,
            {_sub1.getAddress(), chunk, (uint8_t)(memoryNow >> 24), (uint8_t)(memoryNow >> 16),
             (uint8_t)(memoryNow >> 8), (uint8_t)memoryNow});
  }
}

void Huda::actionWrite(const char *arg) {
  uint32_t bits = getVar(HudaVar::NODE_BITS);
  if (bits == 0) {
    console.printf("C [%s] Error: Check node bits first with :ni\r\n", TAG);
    return;
  }
  uint32_t memoryNow = getVar(HudaVar::MEMORY_BEGIN) + getVar(HudaVar::MEMORY_POS);
  uint8_t  parsedBuf[64];
  size_t   bytes2write = hex2bin(arg, strlen(arg), parsedBuf, sizeof(parsedBuf));

  if (bits == 16) {
    if ((bytes2write == 0) || (bytes2write > 16)) {
      console.printf("C [%s] Error: Invalid write length (1-16 bytes)\r\n", TAG);
      return;
    }
    std::vector<uint8_t> payload;
    payload.reserve(bytes2write + 3);
    payload.push_back((uint8_t)bytes2write);
    payload.push_back((uint8_t)(memoryNow >> 8));
    payload.push_back((uint8_t)(memoryNow));
    payload.insert(payload.end(), parsedBuf, parsedBuf + bytes2write);
    sendCmd((uint8_t)(getVar(HudaVar::NODE_NUMBER) << 4), CMD_WRITE16_REQUEST, std::move(payload));
  } else {
    if ((bytes2write == 0) || (bytes2write > 64)) {
      console.printf("C [%s] Error: Invalid write length (1-64 bytes)\r\n", TAG);
      return;
    }
    std::vector<uint8_t> payload;
    payload.reserve(bytes2write + 5);
    payload.push_back((uint8_t)bytes2write);
    payload.push_back((uint8_t)(memoryNow >> 24));
    payload.push_back((uint8_t)(memoryNow >> 16));
    payload.push_back((uint8_t)(memoryNow >> 8));
    payload.push_back((uint8_t)(memoryNow));
    payload.insert(payload.end(), parsedBuf, parsedBuf + bytes2write);
    sendCmd((uint8_t)(getVar(HudaVar::NODE_NUMBER) << 4), CMD_WRITE32_REQUEST, std::move(payload));
  }
  setVarByKey(HudaVar::MEMORY_POS, getVar(HudaVar::MEMORY_POS) + bytes2write);
  printBannerSection(HudaSection::MEMORY);
}

void Huda::actionHelp(const char *arg) {
  (void)arg;
  _helpIdx = 0; // start emitting one line per loop() tick
}

// --- Command handlers ---------------------------------------------------

void Huda::handleReadResponse(const Frame &f) {
  auto payload = f.getPayload();
  if (payload.size() < 3) {
    DBUS2_LOGW("Read response payload too short (%u bytes)", payload.size());
    return;
  }
  size_t   dumpLen  = payload.size() - 2;
  uint32_t memPos   = getVar(HudaVar::MEMORY_POS);
  uint32_t memBegin = getVar(HudaVar::MEMORY_BEGIN);
  uint32_t memEnd   = getVar(HudaVar::MEMORY_END);
  uint8_t  memChunk = (uint8_t)getVar(HudaVar::MEMORY_CHUNK);

  console.printf("%08lx: ", (unsigned long)memPos);

  for (size_t i = 0; i < dumpLen; i++) {
    console.printf("%02x", payload[i + 2]);
    if ((i % 2) && ((i + 1) < dumpLen))
      console.write(' ');
  }

  console.printf("  ");

  for (size_t i = 0; i < dumpLen; i++) {
    uint8_t c = payload[i + 2];
    console.write(((c >= 32) && (c <= 126)) ? c : '.');
  }

  console.println();

  setVarByKey(HudaVar::MEMORY_POS, memPos + dumpLen);
  uint32_t memoryNow = memBegin + memPos + dumpLen;
  if (memoryNow >= memEnd)
    return;

  uint32_t todo = memEnd - memoryNow;
  uint8_t  len  = (todo < memChunk) ? (uint8_t)todo : memChunk;
  uint8_t  node = (uint8_t)(getVar(HudaVar::NODE_NUMBER) << 4);

  if (f.getCommand() & 1) { // low byte 0x01 = 32-bit, 0x00 = 16-bit
    sendCmd(node, CMD_READ32_REQUEST,
            {_sub1.getAddress(), len, (uint8_t)(memoryNow >> 24), (uint8_t)(memoryNow >> 16), (uint8_t)(memoryNow >> 8),
             (uint8_t)memoryNow},
            false);
  } else {
    sendCmd(node, CMD_READ16_REQUEST, {_sub1.getAddress(), len, (uint8_t)(memoryNow >> 8), (uint8_t)memoryNow}, false);
  }
}

void Huda::handleIdResponse(const Frame &f) {

  // 06 | 28.FE-00 | 1000F9D0 | 150C
  // 08 | A0.FE-00 | 60000001FFA8 | DEAC

  auto payload = f.getPayload();
  if (payload.size() == 4) {
    uint32_t idAddr = ((uint32_t)payload[2] << 8) | payload[3];
    setVarByKey(HudaVar::MEMORY_BEGIN, idAddr);
    setVarByKey(HudaVar::MEMORY_END, idAddr + 32);
    setVarByKey(HudaVar::MEMORY_CHUNK, 16);
    setVarByKey(HudaVar::MODULE_NUMBER, payload[1]);
    setVarByKey(HudaVar::NODE_BITS, 16);
    setVarByKey(HudaVar::MEMORY_POS, 0);
    setVarByKey(HudaVar::MODULE_IDSTRING, idAddr);
    printBannerSection(HudaSection::NODE);
    printBannerSection(HudaSection::MODULE);
    printBannerSection(HudaSection::MEMORY);
  } else if (payload.size() == 6) {
    uint32_t idAddr =
        ((uint32_t)payload[2] << 24) | ((uint32_t)payload[3] << 16) | ((uint32_t)payload[4] << 8) | payload[5];
    setVarByKey(HudaVar::MEMORY_BEGIN, idAddr);
    setVarByKey(HudaVar::MEMORY_END, idAddr + 128);
    setVarByKey(HudaVar::MEMORY_CHUNK, 32);
    setVarByKey(HudaVar::MODULE_NUMBER, payload[1]);
    setVarByKey(HudaVar::NODE_BITS, 32);
    setVarByKey(HudaVar::MEMORY_POS, 0);
    setVarByKey(HudaVar::MODULE_IDSTRING, idAddr);
    printBannerSection(HudaSection::NODE);
    printBannerSection(HudaSection::MODULE);
    printBannerSection(HudaSection::MEMORY);
  } else {
    console.printf("C [%s] Error: Invalid or unknown identify response\r\n", TAG);
  }
}

void Huda::handleFlashPageSizeResponse(const Frame &f) {
  auto payload = f.getPayload();
  if (payload.size() < 1) {
    DBUS2_LOGW("Flash page size response payload too short");
    return;
  }
  setVarByKey(HudaVar::MODULE_PAGESIZE, payload[0]);
  printBannerSection(HudaSection::MODULE);
}

// --- Lifecycle ----------------------------------------------------------

Huda::Huda() : Node(0xC, "HUDA") {
  // Subsystem 1: Diagnostic CLI
  _sub1.registerCommand(CMD_READ16_RESPONSE, DBUS2_HANDLE(handleReadResponse));
  _sub1.registerCommand(CMD_READ32_RESPONSE, DBUS2_HANDLE(handleReadResponse));
  _sub1.registerCommand(CMD_ID_RESPONSE, DBUS2_HANDLE(handleIdResponse));
  _sub1.registerCommand(CMD_FLASHPAGESIZE_RESPONSE, DBUS2_HANDLE(handleFlashPageSizeResponse));
  registerSubsystem(&_sub1);
}

void Huda::setup() {
  Node::setup();
  setVarByKey(HudaVar::GENERAL_BAUD, _manager->getBaud());
}

void Huda::loop() {
  Node::loop();
  tickHelp();

  uint8_t binBuf[USER_BUF_LEN];

  while (console.available()) {
    uint8_t c = console.read();
    console.write(c);

    if (_userBufPos >= USER_BUF_LEN - 1) {
      console.printf("C [%s] Error: Input buffer overflow\r\n", TAG);
      _userBufPos = 0;
      continue;
    }

    _userBuf[_userBufPos++] = c;

    if ((c != '\r') && (c != '\n'))
      continue;

    _userBuf[_userBufPos - 1] = 0; // overwrite trailing \r or \n to null-terminate

    if (_userBuf[0] == ':') {
      if (_rawMode && (CMD16(_userBuf + 1) != CMD16("rm"))) {
        console.printf("C [%s] Raw mode active — use ':rm0' to exit\r\n", TAG);
        _userBufPos = 0;
        continue;
      }

      bool handled = false;

      for (auto &v : _vars) {
        if (v.cmd && (CMD16(v.cmd) == CMD16(_userBuf + 1))) {
          setVar(v, &_userBuf[3]);
          handled = true;
          break;
        }
      }

      if (!handled) {
        for (const auto &a : _actions) {
          if (CMD16(a.cmd) == CMD16(_userBuf + 1)) {
            (this->*a.handler)(&_userBuf[3]);
            handled = true;
            break;
          }
        }
      }

      if (!handled)
        printBanner();

      _userBufPos = 0;
      continue;
    }

    size_t binBufLen = hex2bin(_userBuf, _userBufPos, binBuf, sizeof(binBuf));
    _userBufPos      = 0;

    if (_rawMode) {
      _manager->writeRaw(binBuf, binBufLen);
      continue;
    }

    if (binBufLen < 3) {
      console.printf("C [%s] Error: Invalid input length\r\n", TAG);
      continue;
    }

    console.println();
    send(binBuf[0], std::vector<uint8_t>(binBuf + 1, binBuf + binBufLen));
  }

  if (_rawMode) {
    uint8_t b;
    while (_manager->readRaw(&b, 1, 0) == 1) {
      _rawRxBuf[_rawRxBufPos++] = b;
      _rawRxLastUs              = esp_timer_get_time();
      if (_rawRxBufPos >= sizeof(_rawRxBuf)) {
        console.printf("RX:");
        for (uint8_t i = 0; i < _rawRxBufPos; i++)
          console.printf(" %02X", _rawRxBuf[i]);
        console.println();
        _rawRxBufPos = 0;
      }
    }
    if ((_rawRxBufPos > 0) && (esp_timer_get_time() - _rawRxLastUs > 5000)) {
      console.printf("RX:");
      for (uint8_t i = 0; i < _rawRxBufPos; i++)
        console.printf(" %02X", _rawRxBuf[i]);
      console.println();
      _rawRxBufPos = 0;
    }
  }
}

} // namespace DBus2::Nodes

#pragma once

#include "DBus2Node.h"

namespace DBus2::Nodes {

enum class HudaSection {
  NODE,
  MODULE,
  MEMORY,
  GENERAL
};
enum class HudaVarType {
  HEXVAL,
  DECVAL,
  MEMADDR,
  BOOLVAL,
};
enum class HudaVar {
  NODE_NUMBER,
  NODE_RESTART_DELAY,
  NODE_SILENCE_DURATION,
  NODE_BITS,
  MODULE_NUMBER,
  MODULE_IDSTRING,
  MODULE_PAGESIZE,
  MEMORY_BEGIN,
  MEMORY_END,
  MEMORY_POS,
  MEMORY_CHUNK,
  MEMORY_SAVEFILE,
  GENERAL_RAW_MODE,
  GENERAL_BAUD,
};

class Huda : public Node {
public:
  struct VarEntry {
    HudaVar     var;
    HudaSection section;
    const char *name;
    HudaVarType type;
    const char *cmd; // nullptr = read-only
    const char *desc;
    uint32_t    value;
  };

  struct ActionEntry {
    HudaSection section;
    const char *name;
    const char *cmd;
    const char *argHint; // nullptr = no argument
    const char *desc;
    void (Huda::*handler)(const char *arg);
  };

  Huda();
  void setup() override;
  void loop() override;

private:
  // Subsystem 1: Diagnostic CLI
  void handleReadResponse(const Frame &f);
  void handleIdResponse(const Frame &f);
  void handleFlashPageSizeResponse(const Frame &f);

  // Variable and action table accessors
  void     setVar(VarEntry &v, const char *arg);
  void     setVarByKey(HudaVar var, uint32_t value);
  uint32_t getVar(HudaVar var) const;

  // Output helpers
  void printBanner() const;
  void printBannerSection(HudaSection section) const;
  void tickHelp(); // emits one HELP line per call; driven from loop()

  // Action handlers
  void actionInfo(const char *arg);
  void actionRestart(const char *arg);
  void actionBootloader(const char *arg);
  void actionFlashInfo(const char *arg);
  void actionDump(const char *arg);
  void actionWrite(const char *arg);
  void actionHelp(const char *arg);

  static VarEntry          _vars[];
  static const ActionEntry _actions[];

  Subsystem _sub1{1, "Diagnostic CLI"};

  static constexpr uint8_t USER_BUF_LEN = 128;

  bool    _rawMode      = false;
  uint8_t _rawRxBuf[64] = {};
  uint8_t _rawRxBufPos  = 0;
  int64_t _rawRxLastUs  = 0;

  char    _userBuf[USER_BUF_LEN] = {};
  uint8_t _userBufPos            = 0;

  // -1 = idle; 0 = header; 1..N_VARS = VAR lines; N_VARS+1.. = ACT lines
  int     _helpIdx    = -1;
  int64_t _helpLastUs = 0;
};

} // namespace DBus2::Nodes

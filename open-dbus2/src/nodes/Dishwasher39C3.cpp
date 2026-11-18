/*
  Just a quick and dirty hack to emulate the EPG60110 control board at D-Bus
  0x1. Displays some hopefully entertaining things at the user control board
  BSH9000329063.

  Used for the dishwasher control panel, which was on the lectern
  during the 39C3 talk https://media.ccc.de/v/39c3-hacking-washing-machines
*/

#include "nodes/Dishwasher39C3.h"
#include "DBus2Log.h"

static const char *TAG = "DW39C3";

namespace DBus2::Nodes {

/*
         GGGGG
        F     E
         DDDDD
        C     B
         AAAAA

        7 segment LED bits: ABCDEFG, e.g. "3" => 01101101 = 0x6d
*/

static constexpr uint8_t SEG_A = (1 << 6);
static constexpr uint8_t SEG_B = (1 << 5);
static constexpr uint8_t SEG_C = (1 << 4);
static constexpr uint8_t SEG_D = (1 << 3);
static constexpr uint8_t SEG_E = (1 << 2);
static constexpr uint8_t SEG_F = (1 << 1);
static constexpr uint8_t SEG_G = (1 << 0);

static constexpr uint8_t DIGIT_3 = SEG_A | SEG_B | SEG_D | SEG_E | SEG_G;
static constexpr uint8_t DIGIT_9 = SEG_A | SEG_B | SEG_D | SEG_E | SEG_F | SEG_G;
static constexpr uint8_t DIGIT_C = SEG_A | SEG_C | SEG_F | SEG_G;

static constexpr uint8_t MSG[]    = {DIGIT_3, DIGIT_9, DIGIT_C, DIGIT_3};
static constexpr uint8_t MSG_LEN  = sizeof(MSG);
static constexpr uint8_t RING_LEN = 8;

// --- Lifecycle ----------------------------------------------------------

Dishwasher39C3::Dishwasher39C3() : Node(0x1, "Dishwasher 39C3") {
  // Subsystem 1: 39C3 Demo (no incoming commands)
  registerSubsystem(&_sub1);
}

void Dishwasher39C3::setup() {
  Node::setup();
  pinMode(PIN_BLUE, INPUT_PULLUP);
  pinMode(PIN_WHITE, INPUT_PULLUP);
}

void Dishwasher39C3::loop() {
  Node::loop();

  if (millis() - _lastTick < 100)
    return;
  _lastTick = millis();
  _tick++;

  // Init sequence: send once after 4 seconds of idle
  if ((_state == '0') && (_tick == 40)) {
    DBUS2_LOGI("Sending init sequence");
    sendCmd(0x22, 0x7FF1, {0x10});                   // pong
    sendCmd(0x27, 0x2007, {0x01});                   // prg?
    sendCmd(0x24, 0x2006, {0x08});                   // flags: door closed
    sendCmd(0x26, 0x2002, {0x00, 0x00, 0x00, 0x00}); // no errors
    sendCmd(0x25, 0x2008, {0x2A});                   // 0:42 remaining time
    _state = 'I';
  }

  if (digitalRead(PIN_BLUE) == LOW) {
    if (_state != 'B') {
      sendCmd(0x27, 0x2007, {0x07});
      _state = 'B';
      _tick  = 0;
    }

    if (_tick < 40) {
      sendCmd(0x20, 0xF200, {0x06, 0x00, 0xBE, 0x00, DIGIT_C, 0x00, DIGIT_C, 0x00, DIGIT_C}); // CCC
    } else {
      uint8_t canvas[] = {0x06, 0x00, 0xBE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
      uint8_t pos      = (_tick / 2) % RING_LEN;
      pos              = RING_LEN - 1 - pos;

      for (uint8_t i = 0; i < MSG_LEN; i++) {
        uint8_t npos = (pos + i) % RING_LEN;
        if (npos < 3) {
          canvas[4 + 2 * npos] = MSG[i];
        }
      }

      sendCmd(0x20, 0xF200,
              {canvas[0], canvas[1], canvas[2], canvas[3], canvas[4], canvas[5], canvas[6], canvas[7],
               canvas[8]}); // directly to mem
    }

    if (_tick > 120)
      _tick = 0;
  }

  else if (digitalRead(PIN_WHITE) == LOW) {
    if (_state != 'W') {
      sendCmd(0x27, 0x2007, {0x01});
      _state = 'W';
    }

    sendCmd(0x25, 0x2008, {static_cast<uint8_t>(_tick & 0xFF)}); // remaining time
  }

  else if ((_state == 'B') || (_state == 'W')) {
    _state = '0';
    _tick  = 39;
  }
}

} // namespace DBus2::Nodes

#pragma once

#include "DBus2Node.h"
#include <driver/uart.h>

namespace DBus2 {

enum class BusState {
  WAIT_FOR_LEN,
  WAIT_FOR_DEST,
  WAIT_FOR_DATA,
  WAIT_FOR_CRC,
  WAIT_FOR_ACK,
  SKIP_EXCESS_DATA
};

// Outcome of one transmission attempt
enum class TxResult {
  OK,        // ACK received (or broadcast self-ACK echoed)
  COLLISION, // echo differed from the sent byte: another node sent at the same time
  NO_ACK     // no echo or ACK in time, or a wrong or negative ACK
};

class Manager {
public:
  Manager(uart_port_t uartNr);
  bool     begin(int rxPin, int txPin, uint32_t baud = 9600);
  bool     setBaud(uint32_t baud, bool evenParity = false);
  uint32_t getBaud() const {
    return _baud;
  }
  bool registerNode(Node *node);
  bool send(uint8_t senderNodeId, uint8_t dest, std::vector<uint8_t> data, bool blocking = true, int retries = 3);
  bool sendCmd(uint8_t senderNodeId, uint8_t dest, uint16_t cmd, std::vector<uint8_t> payload = {},
               bool blocking = true, int retries = 3);
  void process();

  // Raw mode: minimal bypass for direct UART access, rarely used
  bool setRawMode(bool enabled, bool bootloader = false);
  bool isRawMode() const {
    return _rawMode;
  }
  int readRaw(uint8_t *buf, size_t len, int timeoutMs);
  int writeRaw(const uint8_t *buf, size_t len);

private:
  struct TxJob {
    uint8_t senderNodeId;
    Frame   frame;
    int64_t timestamp; // When the application queued this job, in µs (never updated)
    int     retries;

    SemaphoreHandle_t doneSem;
    bool              success;
  };

  struct RxJob {
    Frame   frame;
    int64_t timestamp; // When the frame was received from the bus (after CRC), in µs (never updated)
    bool    isTxEcho;  // Frame is loopback echo of own transmission
  };

  // Fixed application-level timeouts
  // Max time a TxJob may sit in the queue before being considered stale, in µs (500 ms)
  // Stale jobs are discarded at dequeue — their data is too old to send
  static constexpr int64_t TXJOB_QUEUE_MAX_AGE_US = 500000;
  // Absolute lifetime of a TxJob from creation to forced failure, in µs (1500 ms)
  // Guarantees send(blocking=true) never blocks longer than this
  static constexpr int64_t TXJOB_HARD_TIMEOUT_US = 1500000;
  // Max time an RxJob may sit in the queue before being discarded, in µs (500 ms)
  static constexpr int64_t RXJOB_QUEUE_MAX_AGE_US = 500000;

  // Baud-dependent timeouts (calculated in begin())
  // Configured baud rate
  uint32_t _baud = 0;
  // Duration of one bit at configured baud rate, in µs. The manufacturer's firmware derives its bus
  // timing from the bit time, so the values below scale with the baud rate the same way.
  int64_t _bitTimeUs;
  // Max gap between two bytes of a frame before the parser resets, in µs (19 bit times, ~2 ms at
  // 9600 baud). Also bounds the wait for the ACK after the last CRC byte.
  int64_t _parserResetTimeoutUs;
  // Fixed part of the bus silence required before transmitting, in µs (10 bit times = one byte)
  int64_t _txIdleBaseUs;

  // Random back-off on top of _txIdleBaseUs, in bit times: 0..7 initially, widened to 0..15,
  // 0..31, 0..63, 0..127 after each collision and reset to 0..7 after a successful transmission
  // or a valid frame addressed to us (as some versions of the manufacturer's firmware do; others keep 0..7)
  static constexpr uint8_t BACKOFF_LEVEL_MAX = 4;
  uint8_t _backoffLevel = 0;

  uart_port_t         _uart;
  QueueHandle_t       _txQueue;
  QueueHandle_t       _rxQueue;
  std::vector<Node *> _nodes;

  BusState _state = BusState::WAIT_FOR_LEN;
  Frame    _currentFrame;
  uint16_t _calcCrc;
  bool     _crcHighByteRead = false;
  bool     _isLocal         = false;
  int64_t  _lastActivity    = 0;

  TxJob  *_activeTxJob     = nullptr;
  bool    _txInFlight      = false;
  int64_t _txSilenceTarget = 0;

  volatile bool _rawMode  = false;
  uint32_t      _prevBaud = 0;

  static void task(void *pv);
  void        processRxByte(uint8_t b);
  void        processRxJobs();
  void        finishActiveTxJob(bool success);
  void        completeTxAttempt(TxResult result);
  bool        enqueueRxJob(int64_t timestamp, bool isTxEcho = false);
  uint16_t    updateCrc(uint16_t crc, uint8_t data);
};
} // namespace DBus2

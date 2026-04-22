#include "DBus2Manager.h"
#include "DBus2Log.h"
#include <esp_random.h>

static const char *TAG = "DBus2";

namespace DBus2 {

// =============================================================================
// Public API
// =============================================================================

Manager::Manager(uart_port_t uartNr) : _uart(uartNr) {
  _txQueue = xQueueCreate(20, sizeof(TxJob *));
  _rxQueue = xQueueCreate(10, sizeof(RxJob *));
}

bool Manager::setBaud(uint32_t baud, bool evenParity) {
  _baud                 = baud;
  _byteTimeUs           = 10 * 1000000 / baud;
  _parserResetTimeoutUs = _byteTimeUs * 5 / 2;
  _txBusSilenceMinUs    = _byteTimeUs * 5;
  _txBusSilenceMaxUs    = _byteTimeUs * 8;

  uart_config_t cfg = {.baud_rate  = (int)baud,
                       .data_bits  = UART_DATA_8_BITS,
                       .parity     = evenParity ? UART_PARITY_EVEN : UART_PARITY_DISABLE,
                       .stop_bits  = UART_STOP_BITS_1,
                       .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
                       .source_clk = UART_SCLK_DEFAULT};

  if (uart_param_config(_uart, &cfg) != ESP_OK) {
    DBUS2_LOGE("uart_param_config failed (baud %u)", baud);
    return false;
  }

  return true;
}

bool Manager::begin(int rxPin, int txPin, uint32_t baud) {
  if (!setBaud(baud))
    return false;

  if (uart_set_pin(_uart, txPin, rxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) {
    DBUS2_LOGE("uart_set_pin failed (RX=%d TX=%d)", rxPin, txPin);
    return false;
  }
  if (uart_driver_install(_uart, 256, 256, 0, nullptr, 0) != ESP_OK) {
    DBUS2_LOGE("uart_driver_install failed on UART%d", _uart);
    return false;
  }
  if (uart_set_rx_full_threshold(_uart, 1) != ESP_OK) {
    DBUS2_LOGE("uart_set_rx_full_threshold failed on UART%d", _uart);
    return false;
  }

  if (xTaskCreate(task, "dbus2_task", 4096, this, 10, NULL) != pdPASS) {
    DBUS2_LOGE("xTaskCreate failed for dbus2_task");
    return false;
  }

  for (auto n : _nodes)
    n->setup();

  DBUS2_LOGI("Registered nodes:");
  for (auto n : _nodes) {
    DBUS2_LOGI("  Node 0x%X \"%s\"%s", n->getNodeId(), n->getName(), n->isObserver() ? " (observer)" : "");
    for (auto &[id, s] : n->getSubsystems()) {
      DBUS2_LOGI("    Sub %d: \"%s\" (%d cmds)", id, s->getName(), s->getCommandCount());
    }
  }

  return true;
}

bool Manager::registerNode(Node *node) {
  for (auto n : _nodes) {
    if (n->getNodeId() == node->getNodeId()) {
      DBUS2_LOGW("Duplicate node ID 0x%02X, registration of \"%s\" rejected", node->getNodeId(), node->getName());
      return false;
    }
  }
  node->setManager(this);
  _nodes.push_back(node);
  return true;
}

void Manager::process() {
  processRxJobs();
  for (auto n : _nodes)
    n->loop();
}

// =============================================================================
// Raw mode (minimal bypass for direct UART access, rarely used)
//
// Suspends all protocol processing so a Node can read/write raw bytes on the
// UART. The FreeRTOS task keeps running but skips all phases. Minimal approach,
// rarely used. Used for low-level interaction. Not performance-critical.
// =============================================================================

bool Manager::setRawMode(bool enabled, bool bootloader) {
  if (enabled == _rawMode)
    return true;

  if (enabled) {
    _rawMode = true;

    // Wait for task to reach the skip point
    delay(2);

    // Fail active TX job
    if (_activeTxJob)
      finishActiveTxJob(false);

    // Drain TX queue
    TxJob *tj;
    while (xQueueReceive(_txQueue, &tj, 0) == pdPASS) {
      if (tj->doneSem)
        xSemaphoreGive(tj->doneSem);
      else
        delete tj;
    }

    // Drain RX queue
    RxJob *rj;
    while (xQueueReceive(_rxQueue, &rj, 0) == pdPASS)
      delete rj;

    if (bootloader) {
      _prevBaud = _baud;
      setBaud(9600, true);
    }
  } else {
    // Reset parser for clean protocol restart
    _state        = BusState::WAIT_FOR_LEN;
    _lastActivity = esp_timer_get_time();

    setBaud(_prevBaud != 0 ? _prevBaud : _baud, false);
    _prevBaud = 0;
  }

  // Flush UART buffers
  uart_flush_input(_uart);

  _rawMode = enabled;

  DBUS2_LOGI("Raw mode %s", enabled ? "enabled" : "disabled");
  return true;
}

int Manager::readRaw(uint8_t *buf, size_t len, int timeoutMs) {
  if (!_rawMode)
    return -1;
  return uart_read_bytes(_uart, buf, len, pdMS_TO_TICKS(timeoutMs));
}

int Manager::writeRaw(const uint8_t *buf, size_t len) {
  if (!_rawMode)
    return -1;
  return uart_write_bytes(_uart, buf, len);
}

// =============================================================================
// FreeRTOS bus task (time-critical)
//
// All functions below run in the dbus2_task context on a dedicated 4 KB stack.
// Constraints:
// - Only DBUS2_LOGV allowed (compiled out at default INFO level, zero overhead)
// - No heap allocation except TxJob/RxJob (fixed-size, fast)
// - No blocking calls except uart_read_bytes
// =============================================================================

void Manager::task(void *pv) {
  Manager *m = (Manager *)pv;
  uint8_t  b;

  while (1) {
    // Raw mode: suspend all protocol processing
    if (m->_rawMode) {
      vTaskDelay(1);
      continue;
    }

    // 1. Block up to 1 ms for a single byte from the UART RX FIFO. The timestamp
    //    captured immediately after marks the approximate end of the received byte
    //    and serves as the time reference for all silence checks and activity
    //    tracking in this iteration.
    bool    hasRxByte = uart_read_bytes(m->_uart, &b, 1, pdMS_TO_TICKS(1)) > 0;
    int64_t now       = esp_timer_get_time();

    // 2. If the bus has been silent long enough to exceed the inter-frame gap,
    //    reset the parser to WAIT_FOR_LEN. If a TX job was in flight, it missed
    //    its ACK window — fail the attempt so retries or final failure can proceed.
    if ((now - m->_lastActivity) > m->_parserResetTimeoutUs) {
      if (m->_activeTxJob && m->_txInFlight) {
        DBUS2_LOGV("Parser reset with active TX job (dest 0x%02X), failing", m->_activeTxJob->frame.dest);
        m->completeTxAttempt(false);
      }
      if (m->_state != BusState::WAIT_FOR_LEN) {
        DBUS2_LOGV("Parser reset from state %d after bus silence", (int)m->_state);
        m->_state = BusState::WAIT_FOR_LEN;
      }
    }

    // 3. Feed the received byte into the protocol parser. Besides state
    //    transitions, processRxByte() also writes the next TX byte to the UART
    //    whenever the echo of the previous one matches — driving the byte-by-byte
    //    TX interleaved with RX.
    if (hasRxByte) {
      m->_lastActivity = now;
      m->processRxByte(b);
    }

    // 4. Dequeue the next TX job if none is active. This runs unconditionally
    //    every tick so stale jobs (queued but never sent within
    //    TXJOB_QUEUE_MAX_AGE_US) are discarded promptly even during sustained
    //    bus flooding.
    if (!m->_activeTxJob) {
      if (xQueueReceive(m->_txQueue, &m->_activeTxJob, 0)) {
        if ((now - m->_activeTxJob->timestamp) > TXJOB_QUEUE_MAX_AGE_US) {
          DBUS2_LOGV("Stale queued TX job discarded (dest 0x%02X, age %lld us)", m->_activeTxJob->frame.dest,
                     now - m->_activeTxJob->timestamp);
          m->finishActiveTxJob(false);
        } else {
          m->_txInFlight = false;
        }
      }
    }

    // 5. Attempt to send the active job once the bus has been idle long enough.
    //    A random silence window [min, max] is chosen on first entry to implement
    //    CSMA — multiple nodes back off for different durations to avoid
    //    collisions. Only the first byte (length) is written here; subsequent
    //    bytes are sent one-by-one as their echoes are confirmed in
    //    processRxByte().
    if ((m->_state == BusState::WAIT_FOR_LEN) && m->_activeTxJob && !m->_txInFlight) {
      if ((now - m->_activeTxJob->timestamp) > TXJOB_QUEUE_MAX_AGE_US) {
        DBUS2_LOGV("Stale active TX job discarded (dest 0x%02X, age %lld us)", m->_activeTxJob->frame.dest,
                   now - m->_activeTxJob->timestamp);
        m->finishActiveTxJob(false);
      } else {
        if (m->_txSilenceTarget == 0) {
          m->_txSilenceTarget =
              m->_txBusSilenceMinUs + (esp_random() % (m->_txBusSilenceMaxUs - m->_txBusSilenceMinUs + 1));
        }
        if ((now - m->_lastActivity) > m->_txSilenceTarget) {
          m->_txSilenceTarget = 0;
          Frame &f            = m->_activeTxJob->frame;
          uart_write_bytes(m->_uart, (const char *)&f.length, 1);
          DBUS2_LOGV("TX frame to 0x%02X: sending byte-by-byte, length=0x%02X", f.dest, f.length);
          m->_txInFlight   = true;
          m->_lastActivity = now;
        }
      }
    }

    // 6. Enforce an absolute lifetime limit on every TX job. This fires
    //    regardless of bus state and catches scenarios where no other timeout
    //    can trigger — e.g. sustained bus flooding. Guarantees
    //    send(blocking=true) never blocks indefinitely beyond
    //    TXJOB_HARD_TIMEOUT_US.
    if (m->_activeTxJob && ((now - m->_activeTxJob->timestamp) > TXJOB_HARD_TIMEOUT_US)) {
      DBUS2_LOGV("TX job hard timeout (dest 0x%02X, age %lld us)", m->_activeTxJob->frame.dest,
                 now - m->_activeTxJob->timestamp);
      m->finishActiveTxJob(false);
    }
  }
}

void Manager::processRxByte(uint8_t b) {
  bool expectTxEcho = _activeTxJob && _txInFlight;

  switch (_state) {
    case BusState::WAIT_FOR_LEN:
      if (expectTxEcho) {
        if (b == _activeTxJob->frame.length) {
          uart_write_bytes(_uart, (const char *)&_activeTxJob->frame.dest, 1);
        } else {
          DBUS2_LOGV("TX collision at length byte: sent 0x%02X, echo 0x%02X", _activeTxJob->frame.length, b);
          completeTxAttempt(false);
          _state = BusState::SKIP_EXCESS_DATA;
          return;
        }
      }
      if (b == 0) {
        DBUS2_LOGV("Zero-length frame byte, skipping");
        _state = BusState::SKIP_EXCESS_DATA;
        return;
      }
      _currentFrame.length = b;
      _currentFrame.data.clear();
      _calcCrc         = updateCrc(0, b);
      _crcHighByteRead = false;
      _state           = BusState::WAIT_FOR_DEST;
      break;

    case BusState::WAIT_FOR_DEST:
      if (expectTxEcho) {
        if (b == _activeTxJob->frame.dest) {
          uart_write_bytes(_uart, (const char *)_activeTxJob->frame.data.data(), 1);
        } else {
          DBUS2_LOGV("TX collision at dest byte: sent 0x%02X, echo 0x%02X", _activeTxJob->frame.dest, b);
          completeTxAttempt(false);
          _state = BusState::SKIP_EXCESS_DATA;
          return;
        }
      }
      _currentFrame.dest = b;
      _calcCrc           = updateCrc(_calcCrc, b);
      _isLocal           = false;
      for (auto n : _nodes) {
        if (n->getNodeId() == _currentFrame.getNodeId()) {
          _isLocal = true;
          break;
        }
      }
      _state = BusState::WAIT_FOR_DATA;
      break;

    case BusState::WAIT_FOR_DATA: {
      _currentFrame.data.push_back(b);
      _calcCrc        = updateCrc(_calcCrc, b);
      size_t currPos  = _currentFrame.data.size();
      bool   lastByte = (currPos >= _currentFrame.length);
      if (lastByte)
        _state = BusState::WAIT_FOR_CRC;
      if (expectTxEcho) {
        if (b == _activeTxJob->frame.data[currPos - 1]) {
          if (lastByte) {
            uint8_t hi = _calcCrc >> 8;
            uart_write_bytes(_uart, (const char *)&hi, 1);
          } else {
            uart_write_bytes(_uart, (const char *)&_activeTxJob->frame.data[currPos], 1);
          }
        } else {
          DBUS2_LOGV("TX collision at data[%d]: sent 0x%02X, echo 0x%02X", (int)(currPos - 1),
                     _activeTxJob->frame.data[currPos - 1], b);
          completeTxAttempt(false);
          _state = BusState::SKIP_EXCESS_DATA;
          return;
        }
      }
      break;
    }

    case BusState::WAIT_FOR_CRC: {
      if (!_crcHighByteRead) {
        if (b != (_calcCrc >> 8)) {
          DBUS2_LOGV("CRC high byte mismatch: received 0x%02X, calculated 0x%02X", b, _calcCrc >> 8);
          _state = BusState::SKIP_EXCESS_DATA;
          return;
        }
        _crcHighByteRead = true;
        if (expectTxEcho) {
          uint8_t lo = _calcCrc & 0xFF;
          uart_write_bytes(_uart, (const char *)&lo, 1);
        }
        break;
      }

      if (b != (_calcCrc & 0xFF)) {
        DBUS2_LOGV("CRC low byte mismatch: received 0x%02X, calculated 0x%02X", b, _calcCrc & 0xFF);
        _state = BusState::SKIP_EXCESS_DATA;
        return;
      }

      bool queued = enqueueRxJob(_lastActivity, expectTxEcho);

      if (expectTxEcho) {
        // TX loopback echo — frame we just sent came back valid.
        if (_currentFrame.isBroadcast()) {
          // Broadcast: always self-ACK (protocol requirement)
          uint8_t selfAck = (_activeTxJob->senderNodeId << 4) | 0x0A;
          uart_write_bytes(_uart, (const char *)&selfAck, 1);
          DBUS2_LOGV("ACK with 0x%02X for frame to 0x%02X: broadcast self-ACK", selfAck, _currentFrame.dest);
        } else if (_isLocal) {
          // Loopback unicast: ACK if queued, silent drop if queue full
          if (queued) {
            uint8_t ack = (_currentFrame.dest & 0xF0) | 0x0A;
            uart_write_bytes(_uart, (const char *)&ack, 1);
            DBUS2_LOGV("ACK with 0x%02X for frame to 0x%02X: loopback unicast", ack, _currentFrame.dest);
          } else {
            DBUS2_LOGV("No ACK for frame to 0x%02X: loopback unicast queue full", _currentFrame.dest);
          }
        }
      } else if (_isLocal) {
        // Unicast to us (not our own TX echo): ACK if queued, silent drop if queue full
        if (queued) {
          uint8_t ack = (_currentFrame.dest & 0xF0) | 0x0A;
          uart_write_bytes(_uart, (const char *)&ack, 1);
          DBUS2_LOGV("ACK with 0x%02X for frame to 0x%02X: external unicast", ack, _currentFrame.dest);
        } else {
          DBUS2_LOGV("No ACK for frame to 0x%02X: external unicast queue full", _currentFrame.dest);
        }
      }
      // Broadcast without expectTxEcho or unicast to non-local node: no ACK from us

      // After every valid frame, consume the trailing ACK byte
      _state = BusState::WAIT_FOR_ACK;
      break;
    }

    case BusState::WAIT_FOR_ACK: {
      if (expectTxEcho) {
        // We are waiting for the ACK to our transmitted frame
        uint8_t expectedAck = _activeTxJob->frame.isBroadcast() ? (_activeTxJob->senderNodeId << 4) | 0x0A
                                                                : (_activeTxJob->frame.dest & 0xF0) | 0x0A;
        if (b == expectedAck) {
          completeTxAttempt(true);
        } else {
          DBUS2_LOGV("Unexpected ACK 0x%02X (expected 0x%02X)", b, expectedAck);
        }
      }
      _state = BusState::SKIP_EXCESS_DATA;
      break;
    }

    case BusState::SKIP_EXCESS_DATA:
      // Ignore all bytes until parser reset via bus silence timeout
      break;
  }
}

void Manager::finishActiveTxJob(bool success) {
  if (_activeTxJob->doneSem) {
    _activeTxJob->success = success;
    xSemaphoreGive(_activeTxJob->doneSem);
  } else {
    delete _activeTxJob;
  }
  _txInFlight  = false;
  _activeTxJob = nullptr;
}

void Manager::completeTxAttempt(bool success) {
  if ((_activeTxJob->retries > 0) && !success) {
    DBUS2_LOGV("TX frame to 0x%02X: failed, %d retries left", _activeTxJob->frame.dest, _activeTxJob->retries);
    _activeTxJob->retries--;
    _txInFlight = false;
    return;
  }

  if (success) {
    DBUS2_LOGV("TX frame to 0x%02X: completed successfully (ACK received)", _activeTxJob->frame.dest);
  } else {
    DBUS2_LOGV("TX frame to 0x%02X: failed, no retries left", _activeTxJob->frame.dest);
  }
  finishActiveTxJob(success);
}

bool Manager::enqueueRxJob(int64_t timestamp, bool isTxEcho) {
  RxJob *job = new RxJob{_currentFrame, timestamp, isTxEcho};
  if (xQueueSend(_rxQueue, &job, 0) == pdPASS) {
    DBUS2_LOGV("RX frame for 0x%02X enqueued", _currentFrame.dest);
    return true;
  }
  delete job;
  return false;
}

uint16_t Manager::updateCrc(uint16_t crc, uint8_t data) {
  crc ^= (uint16_t)data << 8;
  for (int i = 0; i < 8; i++) {
    if (crc & 0x8000)
      crc = (crc << 1) ^ 0x1021;
    else
      crc <<= 1;
  }
  return crc;
}

// =============================================================================
// RX dispatch + frame routing (Arduino loop context)
// =============================================================================

void Manager::processRxJobs() {
  RxJob  *job;
  int64_t now = esp_timer_get_time();
  while (xQueueReceive(_rxQueue, &job, 0) == pdPASS) {
    if ((now - job->timestamp) > RXJOB_QUEUE_MAX_AGE_US) {
      DBUS2_LOGD("Stale RX job discarded (dest 0x%02X, age %lld us)", job->frame.dest, now - job->timestamp);
      delete job;
      continue;
    }
    for (auto n : _nodes) {
      if (n->isPromiscuous() || job->frame.isBroadcast() || (n->getNodeId() == job->frame.getNodeId())) {
        n->handleFrame(job->frame, job->timestamp);
      }
    }
    delete job;
  }
}

void Node::handleFrame(const Frame &f, int64_t rxTimestamp) {
  uint8_t subId = f.getSubsystemId();
  if (_subsystems.count(subId)) {
    _subsystems[subId]->handleFrame(f, rxTimestamp);
  } else if (f.isBroadcast()) {
    DBUS2_LOGD("RX frame for 0x%02X: Node 0x%X has no Sub %d", f.dest, _id, subId);
  } else {
    DBUS2_LOGW("RX frame for 0x%02X: Node 0x%X has no Sub %d", f.dest, _id, subId);
  }
}

void Subsystem::handleFrame(const Frame &f, int64_t rxTimestamp) {
  if (f.data.size() < 2) {
    DBUS2_LOGW("RX frame for 0x%02X: too short for command dispatch in Sub %d", f.dest, f.getSubsystemId());
    return;
  }
  uint16_t cmd = f.getCommand();
  if (_commands.count(cmd)) {
    DBUS2_LOGD("RX frame for 0x%02X cmd 0x%04X: dispatched to handler in Sub %d", f.dest, cmd, f.getSubsystemId());
    _commands[cmd](f);
  } else {
    DBUS2_LOGW("RX frame for 0x%02X cmd 0x%04X: no handler in Sub %d", f.dest, cmd, f.getSubsystemId());
  }
}

// =============================================================================
// Send delegation (Manager -> Node -> Subsystem)
// =============================================================================

bool Manager::send(uint8_t senderNodeId, uint8_t dest, std::vector<uint8_t> data, bool blocking, int retries) {
  if (_rawMode)
    return false;

  if ((senderNodeId == 0) || (senderNodeId > 0xF)) {
    DBUS2_LOGE("Invalid sender node ID 0x%02X", senderNodeId);
    return false;
  }

  if (data.empty()) {
    DBUS2_LOGE("TX frame to 0x%02X: empty data rejected", dest);
    return false;
  }

  SemaphoreHandle_t sem = blocking ? xSemaphoreCreateBinary() : nullptr;

  Frame f;
  f.dest   = dest;
  f.length = data.size();
  f.data   = std::move(data);

  TxJob *j = new TxJob{senderNodeId, std::move(f), esp_timer_get_time(), retries, sem, false};

  if (xQueueSend(_txQueue, &j, 0) != pdPASS) {
    DBUS2_LOGW("TX frame to 0x%02X: queue full, dropped", dest);
    if (sem)
      vSemaphoreDelete(sem);
    delete j;
    return false;
  }

  DBUS2_LOGD("TX frame to 0x%02X: queued from Node 0x%X", dest, senderNodeId);

  if (blocking) {
    xSemaphoreTake(sem, portMAX_DELAY);
    bool result = j->success;
    vSemaphoreDelete(sem);
    delete j;
    return result;
  }

  return true;
}

bool Node::send(uint8_t dest, std::vector<uint8_t> data, bool blocking, int retries) {
  return _manager->send(_id, dest, data, blocking, retries);
}

bool Subsystem::send(uint8_t dest, std::vector<uint8_t> data, bool blocking, int retries) {
  return _node->send(dest, data, blocking, retries);
}

bool Manager::sendCmd(uint8_t senderNodeId, uint8_t dest, uint16_t cmd, std::vector<uint8_t> payload, bool blocking,
                      int retries) {
  std::vector<uint8_t> data;
  data.reserve(2 + payload.size());
  data.push_back(cmd >> 8);
  data.push_back(cmd & 0xFF);
  data.insert(data.end(), payload.begin(), payload.end());
  return send(senderNodeId, dest, std::move(data), blocking, retries);
}

bool Node::sendCmd(uint8_t dest, uint16_t cmd, std::vector<uint8_t> payload, bool blocking, int retries) {
  return _manager->sendCmd(_id, dest, cmd, std::move(payload), blocking, retries);
}

bool Subsystem::sendCmd(uint8_t dest, uint16_t cmd, std::vector<uint8_t> payload, bool blocking, int retries) {
  return _node->sendCmd(dest, cmd, std::move(payload), blocking, retries);
}

} // namespace DBus2

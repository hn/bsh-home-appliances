#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/version.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#if ESPHOME_VERSION_CODE < VERSION_CODE(2026, 11, 0)
#error "BSHDBus component requires ESPHome >= 2026.11.0"
#endif

namespace esphome::bshdbus {

using message_parser_t = std::function<float(const std::vector<uint8_t> &)>;
using binary_message_parser_t = std::function<bool(const std::vector<uint8_t> &)>;
using textmessage_parser_t = std::function<std::string(const std::vector<uint8_t> &)>;

/// Result of sending a frame: returned by send() when queueing, passed to the callback when done.
enum class SendResult : uint8_t {
  SEND_RESULT_OK,
  SEND_RESULT_QUEUE_FULL,
  SEND_RESULT_BUS_BUSY,
  SEND_RESULT_COLLISION,
  SEND_RESULT_NO_ACK,
  SEND_RESULT_REJECTED,
  SEND_RESULT_NOT_ENABLED,
};
const LogString *send_result_to_str(SendResult result);

/// Called once the outcome of a queued frame is known. Must not capture anything (plain function pointer).
using send_callback_t = void (*)(SendResult result, uint8_t dest, uint16_t command);
/// Internal, for SendAction: receives the result of a frame queued with send_from_action().
using send_notify_t = void (*)(void *context, uint8_t slot, uint8_t generation, SendResult result);

class BSHDBus;

class BSHDBusListener {
 public:
  void set_dest(uint8_t dest) { this->dest_ = dest; }
  void set_command(uint16_t command) { this->command_ = command; }

  void on_message(uint8_t dest, uint16_t command, const std::vector<uint8_t> &message);
  void dump_filter_config(const char *tag) const;

 protected:
  uint8_t dest_{0};
  uint16_t command_{0};

  virtual void handle_message(const std::vector<uint8_t> &message) = 0;
};

class BSHDBus final : public uart::UARTDevice, public Component {
 public:
  void setup() override;
  void dump_config() override;
  void loop() override;

  void register_listener(BSHDBusListener *listener) { this->listeners_.push_back(listener); }
  template<typename F> void add_on_frame_callback(F &&frame_callback) {
    this->frame_callbacks_.add(std::forward<F>(frame_callback));
  }

  /// Queue a frame for sending; LL and CRC are added. The return value only covers queueing,
  /// the callback (if any, and only if queueing succeeded) receives the final result.
  SendResult send(uint8_t dest, uint16_t command, std::initializer_list<uint8_t> data = {}, uint8_t max_retries = 0,
                  send_callback_t callback = nullptr) {
    return this->send(dest, command, data.begin(), data.size(), max_retries, callback);
  }
  /// Same, with data in RAM; the data is copied before this returns.
  SendResult send(uint8_t dest, uint16_t command, const uint8_t *data, size_t len, uint8_t max_retries = 0,
                  send_callback_t callback = nullptr);
  /// Internal, for SendAction: like send(), but data may be in flash (PROGMEM) and the result goes to notify.
  SendResult send_from_action(uint8_t dest, uint16_t command, const uint8_t *data, size_t len, uint8_t max_retries,
                              send_notify_t notify, void *context, uint8_t slot, uint8_t generation);

#ifdef USE_BSHDBUS_SEND
  void set_tx_enabled(bool tx_enabled) { this->tx_enabled_ = tx_enabled; }
  void set_max_queue_length(uint8_t max_queue_length) { this->max_queue_length_ = max_queue_length; }
  void set_node_id(uint8_t node_id) { this->node_id_ = node_id; }
#endif

 protected:
  std::vector<uint8_t> rx_buffer_;  // received bytes not parsed yet
  std::vector<uint8_t> frame_;      // the frame being delivered
  std::vector<uint8_t> message_;
  uint32_t last_rx_{0};
  uint8_t last_dest_{0};
  uint8_t rx_searched_{0};  // while searching: length of rx_buffer_ already checked for frames
  bool expect_ack_{false};
  bool rx_searching_{false};  // a byte was skipped, the start of the next frame is unknown
  std::vector<BSHDBusListener *> listeners_{};
  LazyCallbackManager<void(const std::vector<uint8_t> &, uint8_t, uint16_t, const std::vector<uint8_t> &)>
      frame_callbacks_{};

  void parse_rx_(bool flush);
  void dispatch_message_(uint8_t dest, uint16_t command, const std::vector<uint8_t> &message);

#ifdef USE_BSHDBUS_SEND
  enum class TxState : uint8_t {
    TX_STATE_IDLE,       // nothing on the bus; the head of the queue (if any) waits for a quiet bus
    TX_STATE_WAIT_ECHO,  // frame written, waiting to read it back
    TX_STATE_WAIT_ACK,   // echo matched (unicast), waiting for the ACK byte
  };
  struct TxEntry {
    send_callback_t callback;  // send()
    send_notify_t notify;      // send_from_action(), with context, slot and generation
    void *context;
    uint8_t frame_len;  // LL to CRC; the frames lie back to back in tx_buf_
    uint8_t max_retries;
    uint8_t slot;
    uint8_t generation;
  };

  SendResult enqueue_(uint8_t dest, uint16_t command, const uint8_t *data, size_t len, bool progmem, TxEntry entry);
  bool sending_allowed_(bool log);
  void process_tx_(uint32_t now, bool received);
  void start_attempt_(uint32_t now);
  void on_tx_frame_();
  void on_tx_ack_(bool valid);
  void on_tx_collision_(const LogString *reason, bool no_echo = false);
  void next_attempt_();
  void finish_tx_(SendResult result);
  TxEntry &tx_head_entry_() { return this->tx_queue_[this->tx_head_]; }

  std::unique_ptr<uint8_t[]> tx_buf_;
  FixedVector<TxEntry> tx_queue_;
  HighFrequencyLoopRequester high_freq_;
  uint32_t byte_time_us_{1042};
  uint32_t tx_head_since_{0};  // when the head of the queue became the head
  uint32_t tx_quiet_ms_{0};    // bus quiet time needed before the next attempt
  uint32_t tx_sent_at_{0};
  uint32_t tx_echo_timeout_{0};
  uint16_t tx_buf_used_{0};
  uint8_t max_queue_length_{4};
  uint8_t node_id_{0};  // 0: none
  uint8_t tx_head_{0};
  uint8_t tx_count_{0};
  uint8_t tx_collisions_{0};
  uint8_t tx_retries_{0};
  TxState tx_state_{TxState::TX_STATE_IDLE};
  SendResult tx_last_error_{SendResult::SEND_RESULT_BUS_BUSY};
  bool tx_enabled_{false};
  bool tx_no_echo_{false};  // the last collision was a missing echo
#endif
};

}  // namespace esphome::bshdbus

/*

   B/S/H/ D-Bus command frame parser

   Because the read logic does not run in an ISR, correct timing as
   presumed for the protocol cannot be guaranteed. Hardware-side
   buffering (especially on the ESP32) makes detecting the inter-frame
   gap even more difficult. This component tries to make the best of it,
   which generally works well.

   (C) 2024-2026 Hajo Noerenberg

   http://www.noerenberg.de/
   https://github.com/hn/bsh-home-appliances

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 3.0 as
   published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program. If not, see <http://www.gnu.org/licenses/gpl-3.0.txt>.

*/

#include "bshdbus.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

#include <algorithm>
#include <cinttypes>
#include <cstring>

namespace esphome::bshdbus {

ESPHOME_LOG_TAG(TAG, "bshdbus");

/* There seems to be no official min/max frame length; especially the max
   appears device-specific and covers the largest frame of all components.
   We assume the largest possible value for safety. */
static constexpr size_t BSHDBUS_MIN_CMDFRAME_LENGTH = 6;
static constexpr size_t BSHDBUS_MAX_CMDFRAME_LENGTH = 255;

/* The manufacturer's firmware discards a frame after a gap of about 19 bit times (~2 ms at 9600 baud)
   between two bytes. Reading in loop(), we only see when loop() ran, not when a byte arrived, so the gap has to cover
   a loop() cycle. A UART task or event with byte timestamps could use the real value. */
static constexpr uint16_t BSHDBUS_RX_TIMEOUT = 20;

/* Sufficient for most cases, some larger frames may be truncated */
static constexpr size_t BSHDBUS_MAX_LOG_BYTES = 128;

/* LL + DS + CCCC + CRC around the data */
static constexpr size_t BSHDBUS_FRAME_OVERHEAD = 1 + 1 + 2 + 2;
static constexpr size_t BSHDBUS_MAX_DATA_LENGTH = BSHDBUS_MAX_CMDFRAME_LENGTH - BSHDBUS_FRAME_OVERHEAD;

PROGMEM_STRING_TABLE(SendResultStrings, "OK", "queue full", "bus busy", "collision", "no ACK", "rejected",
                     "not enabled (no tx_pin, or uart rx_full_threshold is not 1 on ESP32)", "unknown");

const LogString *send_result_to_str(SendResult result) {
  return SendResultStrings::get_log_str(static_cast<uint8_t>(result), SendResultStrings::LAST_INDEX);
}

#ifdef USE_BSHDBUS_SEND
/* Shared by all queued frames; a frame of maximum length always fits */
static constexpr size_t BSHDBUS_TX_BUFFER_SIZE = 256;
/* Deadlines from open-dbus2, counted from when a frame becomes the head of the queue */
static constexpr uint32_t BSHDBUS_TX_BUS_BUSY_TIMEOUT = 500;
static constexpr uint32_t BSHDBUS_TX_HARD_TIMEOUT = 1500;
/* Collisions are retried internally; the receivers drop a frame with a broken CRC. The manufacturer's
   firmware retries collisions more often, depending on the version; with the longer quiet time below,
   collisions are rare enough that 3 suffice. */
static constexpr uint8_t BSHDBUS_TX_MAX_COLLISION_RETRIES = 3;
/* The bus must be quiet for a random 5 to 8 byte times before sending. The manufacturer's firmware usually
   waits only 10 bit times plus a random 0 to 7 bit times (1.0 to 1.8 ms at 9600 baud), and some versions
   widen the random part after each collision. We see the last received byte only to the resolution of
   loop(), so a quiet time that short cannot be measured; the longer one leaves the bus to the
   appliances first, which suits a component that only listens most of the time. */
static constexpr uint32_t BSHDBUS_TX_QUIET_MIN_BYTES = 5;
static constexpr uint32_t BSHDBUS_TX_QUIET_RANDOM_BYTES = 4;
#endif

void BSHDBus::setup() {
  this->rx_buffer_.reserve(BSHDBUS_MAX_CMDFRAME_LENGTH);
  this->frame_.reserve(BSHDBUS_MAX_CMDFRAME_LENGTH);
  this->message_.reserve(BSHDBUS_MAX_CMDFRAME_LENGTH);
#ifdef USE_BSHDBUS_SEND
  if (this->tx_enabled_) {
    this->tx_buf_ = std::make_unique<uint8_t[]>(BSHDBUS_TX_BUFFER_SIZE);
    this->tx_queue_.init(this->max_queue_length_);
    for (uint8_t i = 0; i < this->max_queue_length_; i++)
      this->tx_queue_.push_back(TxEntry{});
    const uint32_t bits = 1 + this->parent_->get_data_bits() +
                          (this->parent_->get_parity() == uart::UART_CONFIG_PARITY_NONE ? 0 : 1) +
                          this->parent_->get_stop_bits();
    const uint32_t baud_rate = std::max<uint32_t>(this->parent_->get_baud_rate(), 1);
    this->byte_time_us_ = (bits * 1000000UL + baud_rate - 1) / baud_rate;
  }
#endif
}

void BSHDBus::dump_config() {
  ESP_LOGCONFIG(TAG, "BSH D-Bus:");
#ifdef USE_BSHDBUS_SEND
  if (this->tx_enabled_) {
    ESP_LOGCONFIG(TAG,
                  "  Sending: %s\n"
                  "  Max queue length: %u",
                  this->sending_allowed_(false) ? LOG_STR_LITERAL("enabled")
                                                : LOG_STR_LITERAL("disabled (uart rx_full_threshold must be 1)"),
                  this->max_queue_length_);
    if (this->node_id_ != 0) {
      ESP_LOGCONFIG(TAG, "  Node ID: 0x%X", this->node_id_);
    }
    return;
  }
#endif
  ESP_LOGCONFIG(TAG, "  Sending: disabled (no tx_pin)");
}

void BSHDBus::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  const uint32_t delta_rx = now - this->last_rx_;
  bool received = false;
  bool timed_out = delta_rx > BSHDBUS_RX_TIMEOUT;
  /* loop() only sees when it ran itself, not when a byte arrived. Bytes already waiting continue what is
     pending (a frame, its ACK, our own echo), even if another component held up loop() for longer than the
     timeout. A real pause shows in a loop() that finds nothing to read. */
  if (timed_out && (this->available() > 0))
    timed_out = false;

  if (timed_out) {
    if (!this->rx_buffer_.empty()) {
      /* The rest of a frame that has begun will not arrive any more. It may have begun at a wrong byte,
         so the bytes are searched for frames once more before they are dropped. */
      ESP_LOGV(TAG, "Bus quiet for %" PRIu32 " ms with %zu unparsed bytes", delta_rx, this->rx_buffer_.size());
      this->parse_rx_(true);
    }
    /* The manufacturer's firmware acknowledges within about 8.5 bit times after the CRC, and senders give up
       after about 19 bit times (~2 ms at 9600 baud). Without byte timestamps, the ACK is awaited until the RX
       timeout instead. */
    if (this->expect_ack_) {
      ESP_LOGD(TAG, "Timed out ACK for dest 0x%02x of preceding frame after %" PRIu32 " ms", this->last_dest_,
               delta_rx);
      this->expect_ack_ = false;
#ifdef USE_BSHDBUS_SEND
      this->on_tx_ack_(false);
#endif
    }
  }

  this->read_available_chunks([this, now, &received](const uint8_t *buf, size_t to_read) {
    this->last_rx_ = now;
    received = true;
    /* The buffer never holds more than one frame of maximum length. A full buffer always starts with a
       complete candidate, so parsing frees at least one byte. */
    while (to_read > 0) {
      const size_t copy_bytes = std::min(BSHDBUS_MAX_CMDFRAME_LENGTH - this->rx_buffer_.size(), to_read);
      this->rx_buffer_.insert(this->rx_buffer_.end(), buf, buf + copy_bytes);
      buf += copy_bytes;
      to_read -= copy_bytes;
      this->parse_rx_(false);
    }
  });

#ifdef USE_BSHDBUS_SEND
  if (this->tx_enabled_)
    this->process_tx_(now, received);
#endif
}

/* Takes all complete frames out of rx_buffer_. A byte that does not start a valid frame (wrong length,
   wrong CRC) is skipped and the search goes on at the next byte, so a wrong length byte costs only itself
   and not the frames that follow within the length it claims. With flush, no more bytes are expected:
   a frame that is not complete is skipped the same way, and the buffer is empty afterwards.

   In sync (after a frame or a pause), there is one candidate, the next byte, and a frame that is not
   complete is waited for. After a skipped byte, it is unknown where the next frame starts: every byte
   is a candidate then, and the first one that is complete and valid wins. */
void BSHDBus::parse_rx_(bool flush) {
  char hex_buf[format_hex_size(BSHDBUS_MAX_LOG_BYTES)];
  const uint8_t *data = this->rx_buffer_.data();
  const size_t size = this->rx_buffer_.size();
  size_t pos = 0;
  size_t skip_start = 0;
  const LogString *skip_reason = nullptr;

  /* Consecutive skipped bytes are logged as one run, with the reason for its first byte */
  auto skip_byte = [&](const LogString *reason) {
    if (!this->rx_searching_) {
      this->rx_searching_ = true;
      this->rx_searched_ = 0;
    }
    if (skip_reason == nullptr) {
      skip_reason = reason;
      skip_start = pos;
#ifdef USE_BSHDBUS_SEND
      this->on_tx_collision_(reason);
#endif
    }
    pos++;
  };
  auto log_skipped = [&]() {
    if (skip_reason == nullptr)
      return;
    ESP_LOGD(TAG, "Skipped %zu bytes of invalid data (%s): 0x%s", pos - skip_start, LOG_STR_ARG(skip_reason),
             format_hex_to(hex_buf, sizeof(hex_buf), data + skip_start, pos - skip_start));
    skip_reason = nullptr;
  };

  while (pos < size) {
    if (this->expect_ack_) {
      const uint8_t reply = data[pos];
      const bool broadcast = (this->last_dest_ & 0xF0) == 0;
      const bool from_dest = (reply & 0xF0) == (this->last_dest_ & 0xF0);
      /* Broadcasts are acknowledged with 0x0A or the node id of the sender, depending on the firmware */
      if (((reply & 0x0F) == 0x0A) && (broadcast || from_dest)) {
        ESP_LOGV(TAG, "Received valid ACK 0x%02x for dest 0x%02x of preceding frame", reply, this->last_dest_);
        this->expect_ack_ = false;
#ifdef USE_BSHDBUS_SEND
        this->on_tx_ack_(true);
#endif
        pos++;
        continue;
      }
      /* Receivers may also answer 0x?7 (CRC error) or 0x?3 (frame too long or busy, depending on the
         firmware). Without byte timing, such a byte looks like the length byte of a frame that follows
         without ACK. It is taken as that frame if a valid one starts here, and as a negative reply
         otherwise; any such byte is a length within the allowed range. */
      if (!broadcast && from_dest && (((reply & 0x0F) == 0x03) || ((reply & 0x0F) == 0x07))) {
        const size_t framelen = 1 + 1 + 2 + static_cast<size_t>(reply);
        const bool complete = framelen <= size - pos;
        if (!complete && !flush)
          /* Wait for more bytes */
          break;
        if (!complete || crc16be(data + pos, framelen, 0x0, 0x1021, false, false)) {
          ESP_LOGD(TAG, "Received negative reply 0x%02x for dest 0x%02x of preceding frame", reply, this->last_dest_);
          this->expect_ack_ = false;
#ifdef USE_BSHDBUS_SEND
          this->on_tx_ack_(false);
#endif
          pos++;
          continue;
        }
      }
      ESP_LOGD(TAG, "Missed ACK for dest 0x%02x of preceding frame", this->last_dest_);
      this->expect_ack_ = false;
#ifdef USE_BSHDBUS_SEND
      this->on_tx_ack_(false);
#endif
    }

    /* framelen = bytes for LL + DS + CRC, and message len byte MM added */
    const size_t framelen = 1 + 1 + 2 + static_cast<size_t>(data[pos]);
    if ((framelen < BSHDBUS_MIN_CMDFRAME_LENGTH) || (framelen > BSHDBUS_MAX_CMDFRAME_LENGTH)) {
      skip_byte(LOG_STR("invalid length"));
      continue;
    }
    if (framelen > size - pos) {
      if (flush) {
        skip_byte(LOG_STR("incomplete frame"));
        continue;
      }
      /* While searching, look for a frame behind this candidate. Each candidate is checked once, when
         the length it claims has arrived: rx_searched_ is the buffer length checked so far. */
      size_t found = size;
      if (this->rx_searching_) {
        for (size_t next = pos + 1; next < size; next++) {
          const size_t nextlen = 1 + 1 + 2 + static_cast<size_t>(data[next]);
          const size_t end = next + nextlen;
          if ((end <= this->rx_searched_) || (end > size) || (nextlen < BSHDBUS_MIN_CMDFRAME_LENGTH) ||
              (nextlen > BSHDBUS_MAX_CMDFRAME_LENGTH))
            continue;
          if (!crc16be(data + next, nextlen, 0x0, 0x1021, false, false)) {
            found = next;
            break;
          }
        }
        this->rx_searched_ = static_cast<uint8_t>(size);
      }
      if (found == size)
        /* Wait for more bytes */
        break;
      while (pos < found)
        skip_byte(LOG_STR("frame found behind"));
      continue;
    }
    if (crc16be(data + pos, framelen, 0x0, 0x1021, false, false)) {
      skip_byte(LOG_STR("invalid CRC"));
      continue;
    }

    log_skipped();
    this->rx_searching_ = false;
    /* More bytes may follow the frame in the buffer, so on_frame gets a copy of the frame alone */
    this->frame_.assign(data + pos, data + pos + framelen);
    pos += framelen;

    const uint8_t dest = this->frame_[1];
    this->last_dest_ = dest;
    this->expect_ack_ = true;
    const uint16_t command = encode_uint16(this->frame_[2], this->frame_[3]);
    /* Skip bytes for LL + DS + CCCC at beginning, CRC at end */
    this->message_.assign(this->frame_.begin() + 1 + 1 + 2, this->frame_.end() - 2);

    ESP_LOGV(TAG, "Valid frame dest 0x%02x cmd 0x%04x: 0x%s", dest, command, format_hex_to(hex_buf, this->message_));
#ifdef USE_BSHDBUS_SEND
    this->on_tx_frame_();
#endif
    this->frame_callbacks_.call(this->frame_, dest, command, this->message_);
    this->dispatch_message_(dest, command, this->message_);
  }

  log_skipped();
  this->rx_buffer_.erase(this->rx_buffer_.begin(), this->rx_buffer_.begin() + pos);
  if (this->rx_buffer_.empty()) {
    /* Nothing pending: a frame that starts now is in sync again, be it after a frame or after a pause */
    this->rx_searching_ = false;
  } else {
    this->rx_searched_ = (this->rx_searched_ > pos) ? static_cast<uint8_t>(this->rx_searched_ - pos) : 0;
  }
}

SendResult BSHDBus::send(uint8_t dest, uint16_t command, const uint8_t *data, size_t len, uint8_t max_retries,
                         send_callback_t callback) {
#ifdef USE_BSHDBUS_SEND
  return this->enqueue_(dest, command, data, len, false, TxEntry{callback, nullptr, nullptr, 0, max_retries, 0, 0});
#else
  ESP_LOGW(TAG, "Not sending frame dest 0x%02x cmd 0x%04x: no tx_pin configured", dest, command);
  return SendResult::SEND_RESULT_NOT_ENABLED;
#endif
}

SendResult BSHDBus::send_from_action(uint8_t dest, uint16_t command, const uint8_t *data, size_t len,
                                     uint8_t max_retries, send_notify_t notify, void *context, uint8_t slot,
                                     uint8_t generation) {
#ifdef USE_BSHDBUS_SEND
  return this->enqueue_(dest, command, data, len, true,
                        TxEntry{nullptr, notify, context, 0, max_retries, slot, generation});
#else
  ESP_LOGW(TAG, "Not sending frame dest 0x%02x cmd 0x%04x: no tx_pin configured", dest, command);
  return SendResult::SEND_RESULT_NOT_ENABLED;
#endif
}

#ifdef USE_BSHDBUS_SEND
SendResult BSHDBus::enqueue_(uint8_t dest, uint16_t command, const uint8_t *data, size_t len, bool progmem,
                             TxEntry entry) {
  if (!this->sending_allowed_(true))
    return SendResult::SEND_RESULT_NOT_ENABLED;
  if ((len > BSHDBUS_MAX_DATA_LENGTH) || ((data == nullptr) && (len != 0))) {
    ESP_LOGW(TAG, "Rejected frame dest 0x%02x cmd 0x%04x: %zu bytes of data, at most %zu allowed", dest, command, len,
             BSHDBUS_MAX_DATA_LENGTH);
    return SendResult::SEND_RESULT_REJECTED;
  }
  if (((dest & 0xF0) == 0) && (this->node_id_ == 0)) {
    ESP_LOGW(TAG, "Rejected broadcast to dest 0x%02x cmd 0x%04x: sending broadcasts requires node_id", dest, command);
    return SendResult::SEND_RESULT_REJECTED;
  }
  const size_t frame_len = len + BSHDBUS_FRAME_OVERHEAD;
  if ((this->tx_count_ >= this->max_queue_length_) || (this->tx_buf_used_ + frame_len > BSHDBUS_TX_BUFFER_SIZE)) {
    ESP_LOGW(TAG, "Rejected frame dest 0x%02x cmd 0x%04x: queue full", dest, command);
    return SendResult::SEND_RESULT_QUEUE_FULL;
  }

  uint8_t *frame = this->tx_buf_.get() + this->tx_buf_used_;
  frame[0] = static_cast<uint8_t>(len + 2);
  frame[1] = dest;
  put_be16(frame + 2, command);
  if (len != 0) {
    /* Constant data of an action stays in flash; progmem_memcpy() reads RAM as well */
    if (progmem) {
      progmem_memcpy(frame + 4, data, len);
    } else {
      std::memcpy(frame + 4, data, len);
    }
  }
  put_be16(frame + 4 + len, crc16be(frame, static_cast<uint16_t>(len + 4)));

  const uint8_t slot = (this->tx_head_ + this->tx_count_) % this->max_queue_length_;
  entry.frame_len = static_cast<uint8_t>(frame_len);
  this->tx_queue_[slot] = entry;
  this->tx_buf_used_ += frame_len;
  if (this->tx_count_++ == 0) {
    this->tx_head_since_ = App.get_loop_component_start_time();
    this->next_attempt_();
    this->high_freq_.start();
  }
  ESP_LOGV(TAG, "Queued frame dest 0x%02x cmd 0x%04x, %zu bytes of data", dest, command, len);
  return SendResult::SEND_RESULT_OK;
}

bool BSHDBus::sending_allowed_(bool log) {
  if (!this->tx_enabled_) {
    if (log) {
      ESP_LOGW(TAG, "Not sending: no tx_pin configured for this bus");
    }
    return false;
  }
#ifdef USE_ESP32
  /* Otherwise the driver hands over received bytes in chunks, too late to see that the bus is busy */
  const size_t threshold = this->parent_->get_rx_full_threshold();
  if (threshold != 1) {
    if (log) {
      ESP_LOGW(TAG, "Not sending: uart rx_full_threshold is %zu, sending on ESP32 requires 'rx_full_threshold: 1'",
               threshold);
    }
    return false;
  }
#endif
  if (this->tx_buf_ == nullptr) {
    if (log) {
      ESP_LOGW(TAG, "Not sending: bus is not set up yet");
    }
    return false;
  }
  return true;
}

void BSHDBus::process_tx_(uint32_t now, bool received) {
  if (this->tx_count_ == 0)
    return;
  if (this->tx_state_ == TxState::TX_STATE_WAIT_ECHO) {
    if (now - this->tx_sent_at_ > this->tx_echo_timeout_) {
      /* Only a missing echo hints at the TX wiring, not one that stopped halfway */
      this->on_tx_collision_(LOG_STR("no echo"), this->rx_buffer_.empty());
    }
    return;
  }
  if (this->tx_state_ != TxState::TX_STATE_IDLE)
    return;

  /* tx_last_error_ stays BUS_BUSY until an attempt was written and failed */
  const uint32_t age = now - this->tx_head_since_;
  if ((age >= BSHDBUS_TX_HARD_TIMEOUT) ||
      ((this->tx_last_error_ == SendResult::SEND_RESULT_BUS_BUSY) && (age >= BSHDBUS_TX_BUS_BUSY_TIMEOUT))) {
    this->finish_tx_(this->tx_last_error_);
    return;
  }

  if (received || !this->rx_buffer_.empty() || this->expect_ack_ || (now - this->last_rx_ < this->tx_quiet_ms_))
    return;
  this->start_attempt_(now);
}

void BSHDBus::start_attempt_(uint32_t now) {
  const uint8_t *frame = this->tx_buf_.get();
  const uint8_t frame_len = this->tx_head_entry_().frame_len;
  const bool broadcast = (frame[1] & 0xF0) == 0;

  ESP_LOGV(TAG, "Sending frame dest 0x%02x cmd 0x%04x (%u bytes)", frame[1], encode_uint16(frame[2], frame[3]),
           frame_len);
  this->write_array(frame, frame_len);
  /* Workaround: a broadcast is acknowledged by the sender itself, right after the CRC, but only if
     the echo shows the frame went out without collision. Reading from loop(), we see the echo too
     late for that, so the ACK always goes out (after a collision, it is garbage that all receivers
     drop). Proper ACK handling needs reception in a UART task or event, which acting as a full
     bus node (acknowledging and answering frames) would require anyway. */
  if (broadcast)
    this->write_byte(static_cast<uint8_t>((this->node_id_ << 4) | 0x0A));

  const uint32_t bytes = frame_len + (broadcast ? 1 : 0);
  this->tx_sent_at_ = now;
  this->tx_echo_timeout_ = (bytes * this->byte_time_us_ + 999) / 1000 + BSHDBUS_RX_TIMEOUT;
  this->tx_state_ = TxState::TX_STATE_WAIT_ECHO;
}

void BSHDBus::on_tx_frame_() {
  if (this->tx_state_ != TxState::TX_STATE_WAIT_ECHO)
    return;
  const uint8_t *frame = this->tx_buf_.get();
  const uint8_t frame_len = this->tx_head_entry_().frame_len;
  if ((this->frame_.size() != frame_len) || (std::memcmp(this->frame_.data(), frame, frame_len) != 0)) {
    this->on_tx_collision_(LOG_STR("other frame"));
    return;
  }
  if ((frame[1] & 0xF0) == 0) {
    ESP_LOGV(TAG, "Echo of broadcast matches");
    this->finish_tx_(SendResult::SEND_RESULT_OK);
    return;
  }
  ESP_LOGV(TAG, "Echo matches, waiting for ACK");
  this->tx_state_ = TxState::TX_STATE_WAIT_ACK;
}

void BSHDBus::on_tx_ack_(bool valid) {
  if (this->tx_state_ != TxState::TX_STATE_WAIT_ACK)
    return;
  if (valid) {
    this->finish_tx_(SendResult::SEND_RESULT_OK);
    return;
  }
  this->tx_last_error_ = SendResult::SEND_RESULT_NO_ACK;
  if (this->tx_retries_ >= this->tx_head_entry_().max_retries) {
    this->finish_tx_(SendResult::SEND_RESULT_NO_ACK);
    return;
  }
  this->tx_retries_++;
  ESP_LOGD(TAG, "No ACK for frame dest 0x%02x, retry %u of %u", this->tx_buf_[1], this->tx_retries_,
           this->tx_head_entry_().max_retries);
  this->next_attempt_();
}

void BSHDBus::on_tx_collision_(const LogString *reason, bool no_echo) {
  if (this->tx_state_ != TxState::TX_STATE_WAIT_ECHO)
    return;
  this->tx_last_error_ = SendResult::SEND_RESULT_COLLISION;
  this->tx_no_echo_ = no_echo;
  if (this->tx_collisions_ >= BSHDBUS_TX_MAX_COLLISION_RETRIES) {
    this->finish_tx_(SendResult::SEND_RESULT_COLLISION);
    return;
  }
  this->tx_collisions_++;
  ESP_LOGD(TAG, "Collision (%s) for frame dest 0x%02x, retry %u of %u", LOG_STR_ARG(reason), this->tx_buf_[1],
           this->tx_collisions_, BSHDBUS_TX_MAX_COLLISION_RETRIES);
  this->next_attempt_();
}

void BSHDBus::next_attempt_() {
  this->tx_state_ = TxState::TX_STATE_IDLE;
  const uint32_t quiet_bytes = BSHDBUS_TX_QUIET_MIN_BYTES + random_uint32() % BSHDBUS_TX_QUIET_RANDOM_BYTES;
  this->tx_quiet_ms_ = (quiet_bytes * this->byte_time_us_ + 999) / 1000;
}

void BSHDBus::finish_tx_(SendResult result) {
  const TxEntry entry = this->tx_head_entry_();
  const uint8_t frame_len = entry.frame_len;
  const uint8_t dest = this->tx_buf_[1];
  const uint16_t command = encode_uint16(this->tx_buf_[2], this->tx_buf_[3]);

  if (result == SendResult::SEND_RESULT_OK) {
    ESP_LOGV(TAG, "Sent frame dest 0x%02x cmd 0x%04x", dest, command);
  } else {
    ESP_LOGW(TAG, "Sending frame dest 0x%02x cmd 0x%04x failed: %s%s", dest, command,
             LOG_STR_ARG(send_result_to_str(result)),
             this->tx_no_echo_ ? LOG_STR_LITERAL(" (no echo, check TX wiring)") : LOG_STR_LITERAL(""));
  }

  /* Take the head off the queue before notifying, so the callback may send again right away */
  this->tx_buf_used_ -= frame_len;
  std::memmove(this->tx_buf_.get(), this->tx_buf_.get() + frame_len, this->tx_buf_used_);
  this->tx_head_ = (this->tx_head_ + 1) % this->max_queue_length_;
  this->tx_count_--;
  this->tx_collisions_ = 0;
  this->tx_retries_ = 0;
  this->tx_no_echo_ = false;
  this->tx_last_error_ = SendResult::SEND_RESULT_BUS_BUSY;
  this->next_attempt_();
  if (this->tx_count_ == 0) {
    this->high_freq_.stop();
  } else {
    this->tx_head_since_ = App.get_loop_component_start_time();
  }

  if (entry.callback != nullptr) {
    entry.callback(result, dest, command);
  } else if (entry.notify != nullptr) {
    entry.notify(entry.context, entry.slot, entry.generation, result);
  }
}
#endif

void BSHDBus::dispatch_message_(uint8_t dest, uint16_t command, const std::vector<uint8_t> &message) {
  for (auto *listener : this->listeners_)
    listener->on_message(dest, command, message);
}

void BSHDBusListener::on_message(uint8_t dest, uint16_t command, const std::vector<uint8_t> &message) {
  if ((this->command_ != command) || (this->dest_ != dest))
    return;
  this->handle_message(message);
}

void BSHDBusListener::dump_filter_config(const char *tag) const {
  ESP_LOGCONFIG(tag,
                "  Dest node: 0x%02x\n"
                "  Command: 0x%04x",
                this->dest_, this->command_);
}

}  // namespace esphome::bshdbus

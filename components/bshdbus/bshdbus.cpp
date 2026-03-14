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

namespace esphome {
namespace bshdbus {

static const char *const TAG = "bshdbus";

/* There seems to be no official min/max frame length; especially the max
   appears device-specific and covers the largest frame of all components.
   We assume the largest possible value for safety. */
static constexpr size_t BSHDBUS_MIN_CMDFRAME_LENGTH = 6;
static constexpr size_t BSHDBUS_MAX_CMDFRAME_LENGTH = 255;

/* This would be much stricter if we were in an ISR. */
static constexpr uint16_t BSHDBUS_RX_TIMEOUT = 20;

/* Sufficient for most cases, some larger frames may be truncated */
static constexpr size_t BSHDBUS_MAX_LOG_BYTES = 128;

void BSHDBus::dump_config() {
  ESP_LOGCONFIG(TAG, "BSH D-Bus:");
  check_uart_settings(9600);
}

void BSHDBus::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  const uint32_t delta_rx = now - this->last_rx_;
  char hex_buf[format_hex_size(BSHDBUS_MAX_LOG_BYTES)];

  if (delta_rx > BSHDBUS_RX_TIMEOUT) {
    if (!this->rx_buffer_.empty()) {
      ESP_LOGD(TAG, "Discarding %zu bytes of unparsed data after timeout of %" PRIu32 " ms: 0x%s",
               this->rx_buffer_.size(), delta_rx, format_hex_to(hex_buf, this->rx_buffer_));
      this->rx_buffer_.clear();
    }
    if (this->expect_ack_for_) {
      ESP_LOGD(TAG, "Timed out ACK for dest 0x%02x of preceeding frame after %" PRIu32 " ms", this->expect_ack_for_,
               delta_rx);
      this->expect_ack_for_ = 0;
    }
  }

  size_t avail = this->available();
  uint8_t buf[64];
  while (avail > 0) {
    size_t to_read = std::min(avail, sizeof(buf));
    if (!this->read_array(buf, to_read))
      break;
    avail -= to_read;
    this->last_rx_ = now;

    for (size_t i = 0; i < to_read;) {
      if (this->expect_ack_for_) {
        if ((buf[i] == ((this->expect_ack_for_ & 0xF0) | 0x0A)) ||
            ((buf[i] == 0x1A) && (this->expect_ack_for_ == 0x0F))) {
          ESP_LOGV(TAG, "Received valid ACK 0x%02x for preceding frame", buf[i]);
          this->expect_ack_for_ = 0;
          i++;
          continue;
        } else {
          ESP_LOGD(TAG, "Missed ACK for dest 0x%02x of preceding frame", this->expect_ack_for_);
          this->expect_ack_for_ = 0;
        }
      }

      /* framelen = bytes for LL + DS + CRC, and message len byte MM added */
      size_t framelen = 1 + 1 + 2;
      if (this->rx_buffer_.empty()) {
        framelen += static_cast<size_t>(buf[i]);
        if ((framelen < BSHDBUS_MIN_CMDFRAME_LENGTH) || (framelen > BSHDBUS_MAX_CMDFRAME_LENGTH)) {
          ESP_LOGD(TAG, "Invalid command frame length %zu, discarding byte", framelen);
          i++;
          continue;
        }
      } else {
        framelen += static_cast<size_t>(this->rx_buffer_[0]);
      }

      const size_t need_bytes = (framelen > this->rx_buffer_.size()) ? framelen - this->rx_buffer_.size() : 0;
      const size_t have_bytes = to_read - i;
      const size_t copy_bytes = std::min(need_bytes, have_bytes);

      this->rx_buffer_.insert(this->rx_buffer_.end(), buf + i, buf + i + copy_bytes);
      i += copy_bytes;

      if (need_bytes > copy_bytes)
        /* Wait for more bytes */
        continue;

      if (crc16be(this->rx_buffer_.data(), this->rx_buffer_.size(), 0x0, 0x1021, false, false)) {
        ESP_LOGD(TAG, "Invalid CRC, discarding %zu bytes of data: 0x%s", this->rx_buffer_.size(),
                 format_hex_to(hex_buf, this->rx_buffer_));
        this->rx_buffer_.clear();
        continue;
      }

      const uint8_t dest = this->rx_buffer_[1];
      this->expect_ack_for_ = dest;
      const uint16_t command = encode_uint16(this->rx_buffer_[2], this->rx_buffer_[3]);
      /* Skip bytes for LL + DS + CCCC at beginning, CRC at end */
      std::vector<uint8_t> message(this->rx_buffer_.begin() + 1 + 1 + 2, this->rx_buffer_.end() - 2);

      ESP_LOGV(TAG, "Valid frame dest 0x%02x cmd 0x%04x: 0x%s", dest, command, format_hex_to(hex_buf, message));
      this->frame_callbacks_.call(this->rx_buffer_, dest, command, message);
      for (auto &listener : this->listeners_)
        listener->on_message(dest, command, message);

      this->rx_buffer_.clear();
    }
  }
}

void BSHDBus::add_on_frame_callback(
    std::function<void(std::vector<uint8_t>, uint8_t, uint16_t, std::vector<uint8_t>)> &&frame_callback) {
  this->frame_callbacks_.add(std::move(frame_callback));
}

void BSHDBusListener::on_message(uint8_t dest, uint16_t command, std::vector<uint8_t> &message) {
  if ((this->command_ != 0xffff) && (this->command_ != command))
    return;
  if ((this->dest_ != 0xff) && (this->dest_ != dest))
    return;
  this->handle_message(message);
}

}  // namespace bshdbus
}  // namespace esphome

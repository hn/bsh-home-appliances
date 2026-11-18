#pragma once

#include "bshdbus.h"
#include "esphome/core/automation.h"
#include "esphome/core/base_automation.h"
#include "esphome/core/helpers.h"

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

namespace esphome::bshdbus {

template<typename... Ts> class SendAction final : public Action<Ts...>, public Parented<BSHDBus> {
  TEMPLATABLE_VALUE(uint8_t, dest)
  TEMPLATABLE_VALUE(uint16_t, command)
  TEMPLATABLE_BYTES(data)

 public:
  void set_max_retries(uint8_t max_retries) { this->max_retries_ = max_retries; }
  void set_wait_for_sent(bool wait_for_sent) { this->flags_.wait_for_sent = wait_for_sent; }
  void set_continue_on_error(bool continue_on_error) { this->flags_.continue_on_error = continue_on_error; }

  /// One slot per frame that can wait in the queue of the hub, so at most that many runs wait at once.
  void init_pending(uint8_t count) {
    this->pending_.init(count);
    for (uint8_t i = 0; i < count; i++)
      this->pending_.emplace_back();
  }

  /// Call after set_wait_for_sent(): only a waiting run needs to continue after the handlers.
  void add_on_sent(const std::initializer_list<Action<Ts...> *> &actions) {
    this->sent_.add_actions(actions);
    if (this->flags_.wait_for_sent)
      this->sent_.add_action(new LambdaAction<Ts...>([this](Ts... x) { this->after_handlers_(true, x...); }));
  }
  void add_on_error(const std::initializer_list<Action<Ts..., SendResult> *> &actions) {
    this->error_.add_actions(actions);
    if (this->flags_.wait_for_sent)
      this->error_.add_action(new LambdaAction<Ts..., SendResult>(
          [this](Ts... x, SendResult /*error*/) { this->after_handlers_(false, x...); }));
  }

  void play_complex(const Ts &...x) override {
    this->num_running_++;
    SendResult result = SendResult::SEND_RESULT_QUEUE_FULL;
    if (!this->wants_result_()) {
      result = this->enqueue_(nullptr, 0, x...);
    } else if (const uint8_t slot = this->free_slot_(); slot < this->pending_.size()) {
      this->pending_[slot].emplace(x...);
      result = this->enqueue_(&SendAction::notify_, slot, x...);
      if (result != SendResult::SEND_RESULT_OK)
        this->pending_[slot].reset();
    }
    // Errors at queueing (queue full, rejected, not enabled) are reported right away
    if (result != SendResult::SEND_RESULT_OK)
      this->handle_result_(result, x...);
    if (!this->flags_.wait_for_sent)
      this->play_next_(x...);
  }

 protected:
  using args_t = std::tuple<stored_trigger_arg_t<Ts>...>;

  void play(const Ts &...x) override { /* see play_complex */ }

  /// Runs that wait for a result end here; their frames stay queued and are sent, but the results are dropped.
  void stop() override {
    this->generation_++;
    for (auto &pending : this->pending_)
      pending.reset();
    this->sent_.stop();
    this->error_.stop();
  }

  bool wants_result_() const { return this->flags_.wait_for_sent || !this->sent_.empty() || !this->error_.empty(); }

  uint8_t free_slot_() const {
    uint8_t slot = 0;
    while ((slot < this->pending_.size()) && this->pending_[slot].has_value())
      slot++;
    return slot;
  }

  SendResult enqueue_(send_notify_t notify, uint8_t slot, const Ts &...x) {
    const uint8_t dest = this->dest_.value(x...);
    const uint16_t command = this->command_.value(x...);
    // Constant data is read straight from flash, without a copy in RAM
    if (this->data_.is_static()) {
      return this->parent_->send_from_action(dest, command, this->data_.data(), this->data_.size(), this->max_retries_,
                                             notify, this, slot, this->generation_);
    }
    const std::vector<uint8_t> data = this->data_.value(x...);
    return this->parent_->send_from_action(dest, command, data.data(), data.size(), this->max_retries_, notify, this,
                                           slot, this->generation_);
  }

  static void notify_(void *context, uint8_t slot, uint8_t generation, SendResult result) {
    auto *self = static_cast<SendAction *>(context);
    // After a stop() the slot is free or belongs to a newer run
    if ((generation != self->generation_) || (slot >= self->pending_.size()) || !self->pending_[slot].has_value())
      return;
    // Free the slot first, so the handlers may run this action again
    args_t args = std::move(*self->pending_[slot]);
    self->pending_[slot].reset();
    std::apply([self, result](const auto &...x) { self->handle_result_(result, x...); }, args);
  }

  void handle_result_(SendResult result, const Ts &...x) {
    if (result == SendResult::SEND_RESULT_OK) {
      if (!this->sent_.empty()) {
        this->sent_.play(x...);
        return;
      }
    } else if (!this->error_.empty()) {
      this->error_.play(x..., result);
      return;
    }
    this->after_handlers_(result == SendResult::SEND_RESULT_OK, x...);
  }

  /// Continue or end the run once the handlers are done; a run that did not wait has already moved on.
  void after_handlers_(bool sent, const Ts &...x) {
    if (!this->flags_.wait_for_sent)
      return;
    if (sent || this->flags_.continue_on_error) {
      this->play_next_(x...);
    } else if (this->num_running_ > 0) {
      // End only this run; stop_complex() would also drop the results other runs wait for
      this->num_running_--;
    }
  }

  FixedVector<std::optional<args_t>> pending_{};
  ActionList<Ts...> sent_;
  ActionList<Ts..., SendResult> error_;
  uint8_t max_retries_{0};
  uint8_t generation_{0};
  struct {
    uint8_t wait_for_sent : 1;
    uint8_t continue_on_error : 1;
    uint8_t reserved : 6;
  } flags_{1, 1, 0};
};

}  // namespace esphome::bshdbus

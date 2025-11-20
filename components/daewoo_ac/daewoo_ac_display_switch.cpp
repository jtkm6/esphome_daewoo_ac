#include "daewoo_ac_display_switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace daewoo_ac {

static const char *const TAG = "daewoo_ac.display_switch";

void DaewooACDisplaySwitch::setup() {
  if (this->parent_ == nullptr) {
    ESP_LOGE(TAG, "Parent not set for display switch during setup");
    return;
  }

  bool initial_state = this->parent_->is_display_on();
  this->last_reported_state_ = initial_state;

  ESP_LOGCONFIG(TAG, "Setting up Display Toggle (initial state: %s)", initial_state ? "ON" : "OFF");
  this->publish_state(initial_state);
}

void DaewooACDisplaySwitch::loop() {
  if (this->parent_ == nullptr) {
    return;
  }

  bool parent_state = this->parent_->is_display_on();
  if (parent_state != this->last_reported_state_) {
    this->last_reported_state_ = parent_state;
    ESP_LOGD(TAG, "Display state synced from AC: %s", parent_state ? "ON" : "OFF");
    this->publish_state(parent_state);
  }
}

void DaewooACDisplaySwitch::write_state(bool state) {
  if (this->parent_ == nullptr) {
    ESP_LOGE(TAG, "Parent not set for display switch");
    return;
  }

  if (state == this->last_reported_state_) {
    ESP_LOGD(TAG, "Display already %s", state ? "ON" : "OFF");
    return;
  }

  this->parent_->enqueue_ui_change("display_on", state ? "true" : "false");
  this->parent_->set_display_on(state);
  this->last_reported_state_ = state;
  this->publish_state(state);
}

}  // namespace daewoo_ac
}  // namespace esphome



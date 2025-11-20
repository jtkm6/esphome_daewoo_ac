#include "daewoo_ac_uv_light_switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace daewoo_ac {

static const char *const TAG = "daewoo_ac.uv_light_switch";

void DaewooACUVLightSwitch::setup() {
  if (this->parent_ == nullptr) {
    ESP_LOGE(TAG, "Parent not set for UV light switch during setup");
    return;
  }

  ESP_LOGCONFIG(TAG, "Setting up UV Light Toggle");
  this->publish_state(this->parent_->is_uv_light_on());
}

void DaewooACUVLightSwitch::write_state(bool state) {
  if (this->parent_ == nullptr) {
    ESP_LOGE(TAG, "Parent not set for UV light switch");
    return;
  }

  this->parent_->enqueue_ui_change("uv_light_on", state ? "true" : "false");
  this->parent_->set_uv_light_on(state);
  this->publish_state(state);
}

}  // namespace daewoo_ac
}  // namespace esphome




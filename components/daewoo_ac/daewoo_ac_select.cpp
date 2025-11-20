#include "daewoo_ac_select.h"
#include "esphome/core/log.h"

namespace esphome {
namespace daewoo_ac {

static const char *const TAG = "daewoo_ac.select";

void DaewooACVaneSelect::setup() {
  // Get initial state from parent, or use first option if not set
  std::string initial_state;
  const auto &options = this->traits.get_options();

  if (this->parent_ != nullptr) {
    this->parent_->set_vertical_vane_labels(options);

    initial_state = this->parent_->get_vertical_vane_display_value();
    ESP_LOGCONFIG(TAG, "Setting up Vertical Vane Select");
    
    // If the initial state is not in the options list, use the first option
    if (!this->has_option(initial_state) && !options.empty()) {
      initial_state = options[0];
      ESP_LOGW(TAG, "Initial state not in options, using first option: %s", initial_state.c_str());
      
      // Update parent with the valid initial state
      this->parent_->set_vertical_vane_position(initial_state);
    }
    
    ESP_LOGCONFIG(TAG, "Initial state: %s", initial_state.c_str());
    this->publish_state(initial_state);
  } else {
    ESP_LOGE(TAG, "Parent not set for vane select during setup");
  }
}

void DaewooACVaneSelect::control(const std::string &value) {
  if (this->parent_ == nullptr) {
    ESP_LOGE(TAG, "Parent not set for vane select");
    return;
  }

  this->parent_->enqueue_ui_change("vertical_vane", value);
  this->parent_->set_vertical_vane_position(value);
  
  this->publish_state(value);
}

}  // namespace daewoo_ac
}  // namespace esphome


#include "daewoo_ac.h"
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include "esphome/core/log.h"

namespace esphome {
namespace daewoo_ac {

static const char *const TAG = "daewoo_ac.climate";

void DaewooAC::enqueue_ui_change(const std::string &property, const std::string &value) {
  this->ui_change_queue_.push_back(UiChangeEntry{property, value});
  if (this->ui_change_queue_.size() > UI_CHANGE_QUEUE_MAX_SIZE) {
    this->ui_change_queue_.erase(this->ui_change_queue_.begin());
  }
  ESP_LOGD(TAG, "Queued UI change: %s = %s", property.c_str(), value.c_str());
}

void DaewooAC::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Daewoo AC...");
  
  // Initialize with demo data
  this->current_temperature = this->current_temperature_;
  this->target_temperature = this->target_temperature_;
  this->fan_mode = this->current_fan_mode_;
  this->mode = this->current_mode_;

  // Initialize swing mode based on initial vane positions
  this->update_swing_mode();
  
  if (this->display_switch_ != nullptr) {
    this->display_switch_->publish_state(this->display_on_);
  }

  if (this->uv_light_switch_ != nullptr) {
    this->uv_light_switch_->publish_state(this->uv_light_on_);
  }
  
  // Publish initial state
  this->publish_state();
  ESP_LOGD(TAG, "  Display: %s", LOG_STR_ARG(this->display_on_ ? "ON" : "OFF"));
  ESP_LOGD(TAG, "  UV Light: %s", LOG_STR_ARG(this->uv_light_on_ ? "ON" : "OFF"));
  ESP_LOGD(TAG, "  Vertical vane: %s", this->vertical_vane_display_value_.c_str());
  ESP_LOGD(TAG, "  Horizontal swing: %s", this->horizontal_swing_on_ ? "ON" : "OFF");
}

void DaewooAC::loop() {
  // Always perform non-blocking UART polling to avoid long blocking operations
  if (this->uart_ != nullptr) {
    while (this->uart_->available()) {
      uint8_t byte;
      this->uart_->read_byte(&byte);
      this->uart_rx_buffer_.push_back(byte);
    }

    // If we have at least one full frame, parse it and clear the buffer.
    // This keeps loop() fast and non-blocking.
    if (this->uart_rx_buffer_.size() >= MESSAGE_LENGTH) {
      std::vector<uint8_t> response(this->uart_rx_buffer_.begin(),
                                    this->uart_rx_buffer_.begin() + MESSAGE_LENGTH);
      this->parse_uart_response_(response);
      this->uart_rx_buffer_.clear();
    }
  }

  uint32_t interval = this->update_interval_ms_ > 0 ? this->update_interval_ms_ : UPDATE_INTERVAL_DEFAULT_MILLIS;
  uint32_t now = millis();
  if (now - this->last_update_ > interval) {
    this->last_update_ = now;

    // Send UART message if UART is configured (non-blocking)
    if (this->uart_ != nullptr) {
      std::vector<uint8_t> cmd_vec{0xAA, 0x02, 0x01, 0xAD};
      if (this->ui_change_queue_.size() > 0) {
        std::array<uint8_t, MESSAGE_LENGTH> frame = this->build_command_frame_from_state_();
        cmd_vec.assign(frame.begin(), frame.end());
      }

      this->uart_->write_array(cmd_vec.data(), cmd_vec.size());

      // Print the command sent in hex
      std::string hex_string;
      for (uint8_t byte : cmd_vec) {
        char hex[4];
        sprintf(hex, "%02X ", byte);
        hex_string += hex;
      }
      ESP_LOGI(TAG, "Sent UART frame:\t%s", hex_string.c_str());
    }
  }
}

void DaewooAC::parse_uart_response_(const std::vector<uint8_t> &buffer) {
  if (buffer.size() != MESSAGE_LENGTH) {
    ESP_LOGW(TAG, "Invalid UART frame: expected %u bytes, got %u bytes", MESSAGE_LENGTH, buffer.size());
    return;
  }

  if (buffer[0] != 0xAA) {
    char hex[4];
    sprintf(hex, "%02X ", buffer[0]);
    ESP_LOGW(TAG, "Invalid UART frame: expected 0xAA, got %s", hex);
    return;
  }

  if (buffer[1] + 2U != MESSAGE_LENGTH) {
    ESP_LOGW(TAG, "Invalid UART frame: expected %u bytes, got %u bytes", MESSAGE_LENGTH - 2U, buffer[1]);
    return;
  }

  // Validate checksum: sum of all bytes except the last, modulo 256, must equal the last byte
  uint8_t calculated_checksum = 0;
  for (size_t i = 0; i < MESSAGE_LENGTH - 1; ++i) {
    calculated_checksum = static_cast<uint8_t>(calculated_checksum + buffer[i]);
  }
  uint8_t received_checksum = buffer[MESSAGE_LENGTH - 1];

  if (calculated_checksum != received_checksum) {
    ESP_LOGW(TAG, "Invalid UART frame checksum: calculated 0x%02X, received 0x%02X", calculated_checksum,
             received_checksum);
    return;
  }

  std::string hex_string;
  for (uint8_t byte : buffer) {
    char hex[4];
    sprintf(hex, "%02X ", byte);
    hex_string += hex;
  }
  ESP_LOGI(TAG, "Received UART frame:\t%s", hex_string.c_str());

  std::memcpy(&this->daewoo_state_, buffer.data() + 2U, sizeof(DaewooState));

  this->sync_daewoo_state();
}

// Helper functions for parsing numeric values from strings without exceptions.
namespace {
int parse_int(const std::string &value, int fallback) {
  char *end = nullptr;
  long v = std::strtol(value.c_str(), &end, 10);
  if (end == value.c_str()) {
    return fallback;
  }
  if (v < std::numeric_limits<int>::min() || v > std::numeric_limits<int>::max()) {
    return fallback;
  }
  return static_cast<int>(v);
}

float parse_float(const std::string &value, float fallback) {
  char *end = nullptr;
  float v = std::strtof(value.c_str(), &end);
  if (end == value.c_str()) {
    return fallback;
  }
  return v;
}
}  // namespace

void DaewooAC::sync_daewoo_state() {
  climate::ClimateMode resolved_mode = this->current_mode_;
  climate::ClimateFanMode resolved_fan_mode = this->current_fan_mode_;
  bool mode_valid = false;
  bool fan_valid = false;
  bool should_publish = false;

  switch (this->daewoo_state_.power_state) {
    case 0x00:  // OFF
      resolved_mode = climate::CLIMATE_MODE_OFF;
      mode_valid = true;
      break;
    case 0x01:  // ON
      switch (this->daewoo_state_.mode) {
        case 0x00:
          resolved_mode = climate::CLIMATE_MODE_AUTO;
          mode_valid = true;
          break;
        case 0x01:
          resolved_mode = climate::CLIMATE_MODE_COOL;
          mode_valid = true;
          break;
        case 0x02:
          resolved_mode = climate::CLIMATE_MODE_DRY;
          mode_valid = true;
          break;
        case 0x03:
          resolved_mode = climate::CLIMATE_MODE_HEAT;
          mode_valid = true;
          break;
        case 0x04:
          resolved_mode = climate::CLIMATE_MODE_FAN_ONLY;
          mode_valid = true;
          break;
        default:
          ESP_LOGW(TAG, "Unknown Daewoo mode value: 0x%02X", this->daewoo_state_.mode);
          break;
      }
      break;
    default:
      ESP_LOGW(TAG, "Unknown power state value: 0x%02X", this->daewoo_state_.power_state);
      break;
  }

  if (mode_valid) {
    bool mode_changed = resolved_mode != this->current_mode_;
    this->current_mode_ = resolved_mode;
    this->mode = resolved_mode;

    if (mode_changed) {
      should_publish = true;
      ESP_LOGD(TAG, "Climate mode updated to %d (power=0x%02X, mode=0x%02X)", static_cast<int>(resolved_mode),
      this->daewoo_state_.power_state, this->daewoo_state_.mode);
    }
  }

  bool quiet_forced = (this->daewoo_state_.flags1 & QUIET_FLAG_MASK) != 0;

  if (quiet_forced) {
    resolved_fan_mode = climate::CLIMATE_FAN_QUIET;
    fan_valid = true;
    ESP_LOGD(TAG, "Quiet flag detected (flags=0x%02X); forcing fan mode to QUIET", this->daewoo_state_.flags1);
  } else {
    switch (this->daewoo_state_.fan_mode) {
      case 0x00:
        resolved_fan_mode = climate::CLIMATE_FAN_AUTO;
        fan_valid = true;
        break;
      case 0x01:
        resolved_fan_mode = climate::CLIMATE_FAN_LOW;
        fan_valid = true;
        break;
      case 0x02:
        resolved_fan_mode = climate::CLIMATE_FAN_MEDIUM;
        fan_valid = true;
        break;
      case 0x03:
        resolved_fan_mode = climate::CLIMATE_FAN_HIGH;
        fan_valid = true;
        break;
      default:
        ESP_LOGW(TAG, "Unknown fan mode value: 0x%02X", this->daewoo_state_.fan_mode);
        break;
    }
  }

  if (fan_valid) {
    bool fan_changed = resolved_fan_mode != this->current_fan_mode_;
    this->current_fan_mode_ = resolved_fan_mode;
    this->fan_mode = resolved_fan_mode;

    if (fan_changed) {
      should_publish = true;
      ESP_LOGD(TAG, "Fan mode updated to %d (fan=0x%02X)", static_cast<int>(resolved_fan_mode), this->daewoo_state_.fan_mode);
    }
  }

  // Map Daewoo vertical vane byte to internal vertical vane position and update select
  VerticalVanePosition resolved_vertical_vane = this->vertical_vane_position_;
  bool vertical_vane_valid = false;

  switch (this->daewoo_state_.vertical_vane) {
    case 0x00:  // SWING
      resolved_vertical_vane = VerticalVanePosition::SWING;
      vertical_vane_valid = true;
      break;
    case 0x01:  // DOWN
      resolved_vertical_vane = VerticalVanePosition::DOWN;
      vertical_vane_valid = true;
      break;
    case 0x02:  // MEDIUM_DOWN
      resolved_vertical_vane = VerticalVanePosition::MEDIUM_DOWN;
      vertical_vane_valid = true;
      break;
    case 0x03:  // MEDIUM
      resolved_vertical_vane = VerticalVanePosition::MEDIUM;
      vertical_vane_valid = true;
      break;
    case 0x04:  // UP_MEDIUM
      resolved_vertical_vane = VerticalVanePosition::UP_MEDIUM;
      vertical_vane_valid = true;
      break;
    case 0x05:  // UP
      resolved_vertical_vane = VerticalVanePosition::UP;
      vertical_vane_valid = true;
      break;
    case 0x06:  // STATIC
      resolved_vertical_vane = VerticalVanePosition::STATIC;
      vertical_vane_valid = true;
      break;
    default:
      ESP_LOGW(TAG, "Unknown vertical vane value: 0x%02X", this->daewoo_state_.vertical_vane);
      break;
  }

  if (vertical_vane_valid && resolved_vertical_vane != this->vertical_vane_position_) {
    this->apply_vertical_vane_position(resolved_vertical_vane);
    should_publish = true;
    ESP_LOGD(TAG, "Vertical vane updated to %s (vertical_vane=0x%02X)",
             this->vertical_vane_display_value_.c_str(), this->daewoo_state_.vertical_vane);
  }

  // Bit 0x02 of flags0 controls horizontal swing (ON when set, OFF when cleared).
  bool horizontal_swing_enabled = (this->daewoo_state_.flags0 & HORIZONTAL_SWING_FLAG_MASK) != 0;

  if (horizontal_swing_enabled != this->horizontal_swing_on_) {
    this->set_horizontal_swing_on(horizontal_swing_enabled);
    should_publish = true;
    ESP_LOGD(TAG, "Horizontal swing flag updated to %s (flags0=0x%02X)",
             horizontal_swing_enabled ? "ON" : "OFF", this->daewoo_state_.flags0);
  }


  bool display_enabled = (this->daewoo_state_.flags1 & DISPLAY_FLAG_MASK) != 0;

  if (display_enabled != this->display_on_) {
    this->set_display_on(display_enabled);
    should_publish = true;
    ESP_LOGD(TAG, "Display flag updated to %s (flags=0x%02X)", display_enabled ? "ON" : "OFF", this->daewoo_state_.flags1);
  }


  bool uv_light_enabled = (this->daewoo_state_.flags1 & UV_LIGHT_FLAG_MASK) != 0;

  if (uv_light_enabled != this->uv_light_on_) {
    this->set_uv_light_on(uv_light_enabled);
    should_publish = true;
    ESP_LOGD(TAG, "UV light flag updated to %s (flags=0x%02X)", uv_light_enabled ? "ON" : "OFF", this->daewoo_state_.flags1);
  }


  uint8_t raw_target_temperature = this->daewoo_state_.target_temperature;

  if (raw_target_temperature >= MIN_TARGET_TEMPERATURE && raw_target_temperature <= MAX_TARGET_TEMPERATURE) {
    float resolved_target_temperature = static_cast<float>(raw_target_temperature);
    bool target_temperature_changed = resolved_target_temperature != this->target_temperature_;

    this->target_temperature_ = resolved_target_temperature;
    this->target_temperature = resolved_target_temperature;
    if (target_temperature_changed) {
      should_publish = true;
      ESP_LOGD(TAG, "Target temperature updated to %.1f (raw=0x%02X)", resolved_target_temperature,
               this->daewoo_state_.target_temperature);
    }
  } else {
    ESP_LOGW(TAG, "Invalid target temperature value: %u (expected %u-%u)", raw_target_temperature,
             MIN_TARGET_TEMPERATURE, MAX_TARGET_TEMPERATURE);
  }


  uint8_t raw_current_temperature = this->daewoo_state_.current_temperature;

  if (raw_current_temperature >= MIN_CURRENT_TEMPERATURE && raw_current_temperature <= MAX_CURRENT_TEMPERATURE) {
    float resolved_current_temperature = static_cast<float>(raw_current_temperature);
    bool current_temperature_changed = resolved_current_temperature != this->current_temperature_;

    this->current_temperature_ = resolved_current_temperature;
    this->current_temperature = resolved_current_temperature;

    if (current_temperature_changed) {
      should_publish = true;
      ESP_LOGD(TAG, "Current temperature updated to %.1f (raw=0x%02X)", resolved_current_temperature,
               this->daewoo_state_.current_temperature);
    }
  } else {
    ESP_LOGW(TAG, "Invalid current temperature value: %u (expected %u-%u)", raw_current_temperature,
             MIN_CURRENT_TEMPERATURE, MAX_CURRENT_TEMPERATURE);
  }

  if (should_publish) {
    this->publish_state();
    ESP_LOGD(TAG, "  Display: %s", LOG_STR_ARG(this->display_on_ ? "ON" : "OFF"));
    ESP_LOGD(TAG, "  UV Light: %s", LOG_STR_ARG(this->uv_light_on_ ? "ON" : "OFF"));
    ESP_LOGD(TAG, "  Vertical sweep: %s", this->vertical_vane_display_value_.c_str());
    ESP_LOGD(TAG, "  Horizontal swing: %s", this->horizontal_swing_on_ ? "ON" : "OFF");
  }
}

void DaewooAC::apply_ui_change_to_state_(DaewooState &state, const UiChangeEntry &change) const {
  const std::string &prop = change.property;

  // Mode / power state mapping
  if (prop == "mode") {
    int mode_int = parse_int(change.value, static_cast<int>(this->current_mode_));
    auto requested_mode = static_cast<climate::ClimateMode>(mode_int);

    if (requested_mode == climate::CLIMATE_MODE_OFF) {
      state.power_state = 0x00;
    } else {
      state.power_state = 0x01;
      switch (requested_mode) {
        case climate::CLIMATE_MODE_AUTO:
          state.mode = 0x00;
          break;
        case climate::CLIMATE_MODE_COOL:
          state.mode = 0x01;
          break;
        case climate::CLIMATE_MODE_DRY:
          state.mode = 0x02;
          break;
        case climate::CLIMATE_MODE_HEAT:
          state.mode = 0x03;
          break;
        case climate::CLIMATE_MODE_FAN_ONLY:
          state.mode = 0x04;
          break;
        default:
          // Unsupported/unknown mode; keep previous byte.
          break;
      }
    }
    return;
  }

  // Target temperature mapping
  if (prop == "target_temperature") {
    float temp = parse_float(change.value, this->target_temperature_);
    int temp_int = static_cast<int>(std::round(temp));
    if (temp_int < MIN_TARGET_TEMPERATURE)
      temp_int = MIN_TARGET_TEMPERATURE;
    if (temp_int > MAX_TARGET_TEMPERATURE)
      temp_int = MAX_TARGET_TEMPERATURE;
    state.target_temperature = static_cast<uint8_t>(temp_int);
    return;
  }

  // Fan mode mapping (fan_mode byte + quiet flag)
  if (prop == "fan_mode") {
    int fan_int = parse_int(change.value, static_cast<int>(this->current_fan_mode_));
    auto requested_fan = static_cast<climate::ClimateFanMode>(fan_int);

    switch (requested_fan) {
      case climate::CLIMATE_FAN_AUTO:
        state.fan_mode = 0x00;
        state.flags1 &= static_cast<uint8_t>(~QUIET_FLAG_MASK);
        break;
      case climate::CLIMATE_FAN_LOW:
        state.fan_mode = 0x01;
        state.flags1 &= static_cast<uint8_t>(~QUIET_FLAG_MASK);
        break;
      case climate::CLIMATE_FAN_MEDIUM:
        state.fan_mode = 0x02;
        state.flags1 &= static_cast<uint8_t>(~QUIET_FLAG_MASK);
        break;
      case climate::CLIMATE_FAN_HIGH:
        state.fan_mode = 0x03;
        state.flags1 &= static_cast<uint8_t>(~QUIET_FLAG_MASK);
        break;
      case climate::CLIMATE_FAN_QUIET:
        // Reuse underlying fan speed from AUTO but mark quiet flag.
        state.fan_mode = 0x00;
        state.flags1 |= QUIET_FLAG_MASK;
        break;
      default:
        // Unknown fan mode; leave as-is.
        break;
    }
    return;
  }

  // Display flag (flags1 bit 0x10)
  if (prop == "display_on") {

    bool on = (change.value == "true" || change.value == "1" || change.value == "on");
    if (on) {
      state.flags1 |= DISPLAY_FLAG_MASK;
    } else {
      state.flags1 &= static_cast<uint8_t>(~DISPLAY_FLAG_MASK);
    }
    return;
  }

  // UV light flag (flags1 bit 0x02, matching sync_daewoo_state())
  if (prop == "uv_light_on") {
    bool on = (change.value == "true" || change.value == "1" || change.value == "on");
    if (on) {
      state.flags1 |= UV_LIGHT_FLAG_MASK;
    } else {
      state.flags1 &= static_cast<uint8_t>(~UV_LIGHT_FLAG_MASK);
    }
    return;
  }

  // Horizontal swing flag (flags0 bit 0x02)
  if (prop == "horizontal_swing_on" || prop == "swing_mode") {
    bool swing_on = this->horizontal_swing_on_;
    if (prop == "horizontal_swing_on") {
      swing_on = (change.value == "true" || change.value == "1" || change.value == "on");
    }

    if (swing_on) {
      state.flags0 |= HORIZONTAL_SWING_FLAG_MASK;
    } else {
      state.flags0 &= static_cast<uint8_t>(~HORIZONTAL_SWING_FLAG_MASK);
    }

    // For swing-related changes, also update the vertical vane byte from the
    // current vertical vane position so that the frame matches UI intent.
    switch (this->vertical_vane_position_) {
      case VerticalVanePosition::SWING:
        state.vertical_vane = 0x00;
        break;
      case VerticalVanePosition::DOWN:
        state.vertical_vane = 0x01;
        break;
      case VerticalVanePosition::MEDIUM_DOWN:
        state.vertical_vane = 0x02;
        break;
      case VerticalVanePosition::MEDIUM:
        state.vertical_vane = 0x03;
        break;
      case VerticalVanePosition::UP_MEDIUM:
        state.vertical_vane = 0x04;
        break;
      case VerticalVanePosition::UP:
        state.vertical_vane = 0x05;
        break;
      case VerticalVanePosition::STATIC:
      default:
        state.vertical_vane = 0x06;
        break;
    }
    return;
  }

  // Vertical vane select change; map from current internal vane position
  if (prop == "vertical_vane") {
    switch (this->vertical_vane_position_) {
      case VerticalVanePosition::SWING:
        state.vertical_vane = 0x00;
        break;
      case VerticalVanePosition::DOWN:
        state.vertical_vane = 0x01;
        break;
      case VerticalVanePosition::MEDIUM_DOWN:
        state.vertical_vane = 0x02;
        break;
      case VerticalVanePosition::MEDIUM:
        state.vertical_vane = 0x03;
        break;
      case VerticalVanePosition::UP_MEDIUM:
        state.vertical_vane = 0x04;
        break;
      case VerticalVanePosition::UP:
        state.vertical_vane = 0x05;
        break;
      case VerticalVanePosition::STATIC:
      default:
        state.vertical_vane = 0x06;
        break;
    }
    return;
  }

  // Unknown property: ignore for now.
}

std::array<uint8_t, MESSAGE_LENGTH> DaewooAC::build_command_frame_from_state_() {
  // Start from the last known Daewoo state as received from the AC.
  DaewooState working = this->daewoo_state_;

  // Apply all queued UI changes in order; later entries win.
  for (const auto &change : this->ui_change_queue_) {
    this->apply_ui_change_to_state_(working, change);
  }

  // Build the 22-byte UART frame: 0xAA, 0x14, <20-byte payload>, checksum.
  std::array<uint8_t, MESSAGE_LENGTH> frame{};
  frame[0] = 0xAA;
  frame[1] = static_cast<uint8_t>(MESSAGE_LENGTH - 2);  // payload length (0x14)

  // Clear checksum field before computing it.
  working.checksum = 0x00;
  std::memcpy(frame.data() + 2U, &working, sizeof(DaewooState));

  // Write writer mode (command identifier for write operation)
  frame[2] = 0x02;

  // Compute checksum as the sum of all bytes except the last, modulo 256.
  uint8_t checksum = 0;
  for (size_t i = 0; i < MESSAGE_LENGTH - 1; ++i) {
    checksum = static_cast<uint8_t>(checksum + frame[i]);
  }

  // Write checksum at the end of the frame and into the DaewooState payload.
  frame[MESSAGE_LENGTH - 1] = checksum;
  frame[2U + sizeof(DaewooState) - 1U] = checksum;  // last byte of DaewooState in payload
  
  // All pending UI changes have now been encoded into this command frame,
  // so we can clear the queue.
  this->ui_change_queue_.clear();

  return frame;
}

void DaewooAC::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value()) {
    this->mode = *call.get_mode();
    this->current_mode_ = *call.get_mode();
    this->enqueue_ui_change("mode", std::to_string(static_cast<int>(*call.get_mode())));
    ESP_LOGD(TAG, "Mode changed to: %d", *call.get_mode());
  }
  
  if (call.get_target_temperature().has_value()) {
    this->target_temperature = *call.get_target_temperature();
    this->target_temperature_ = *call.get_target_temperature();
     this->enqueue_ui_change("target_temperature", std::to_string(*call.get_target_temperature()));
    ESP_LOGD(TAG, "Target temperature changed to: %.1f", *call.get_target_temperature());
  }
  
  if (call.get_fan_mode().has_value()) {
    this->fan_mode = *call.get_fan_mode();
    this->current_fan_mode_ = *call.get_fan_mode();
    this->enqueue_ui_change("fan_mode", std::to_string(static_cast<int>(*call.get_fan_mode())));
    ESP_LOGD(TAG, "Fan mode changed to: %d", *call.get_fan_mode());
  }
  
  if (call.get_swing_mode().has_value()) {
    auto requested_swing_mode = *call.get_swing_mode();
    this->enqueue_ui_change("swing_mode", std::to_string(static_cast<int>(requested_swing_mode)));
    ESP_LOGD(TAG, "Swing mode change request: %d", static_cast<int>(requested_swing_mode));

    bool vane_positions_updated = false;

    auto set_vertical_if_needed = [&](VerticalVanePosition motion) {
      if (this->vertical_vane_position_ != motion) {
        this->set_vertical_vane_position(motion);
        vane_positions_updated = true;
      }
    };

    auto set_horizontal_if_needed = [&](bool swing_on) {
      if (this->horizontal_swing_on_ != swing_on) {
        this->set_horizontal_swing_on(swing_on);
        vane_positions_updated = true;
      }
    };

    switch (requested_swing_mode) {
      case climate::CLIMATE_SWING_BOTH:
        set_vertical_if_needed(VerticalVanePosition::SWING);
        set_horizontal_if_needed(true);
        break;

      case climate::CLIMATE_SWING_VERTICAL:
        set_vertical_if_needed(VerticalVanePosition::SWING);
        if (this->horizontal_swing_on_) {
          set_horizontal_if_needed(false);
        }
        break;

      case climate::CLIMATE_SWING_HORIZONTAL:
        set_horizontal_if_needed(true);
        if (this->vertical_vane_position_ == VerticalVanePosition::SWING) {
          set_vertical_if_needed(VerticalVanePosition::STATIC);
        }
        break;

      case climate::CLIMATE_SWING_OFF:
        if (this->vertical_vane_position_ == VerticalVanePosition::SWING) {
          set_vertical_if_needed(VerticalVanePosition::STATIC);
        }
        if (this->horizontal_swing_on_) {
          set_horizontal_if_needed(false);
        }
        break;

      default:
        ESP_LOGW(TAG, "Unsupported swing mode request: %d", static_cast<int>(requested_swing_mode));
        break;
    }

    if (!vane_positions_updated) {
      this->update_swing_mode();
    }
  }
  
  // Publish the updated state
  this->publish_state();
  ESP_LOGD(TAG, "  Display: %s", LOG_STR_ARG(this->display_on_ ? "ON" : "OFF"));
  ESP_LOGD(TAG, "  UV Light: %s", LOG_STR_ARG(this->uv_light_on_ ? "ON" : "OFF"));
  ESP_LOGD(TAG, "  Vertical sweep: %s", this->vertical_vane_display_value_.c_str());
  ESP_LOGD(TAG, "  Horizontal swing: %s", this->horizontal_swing_on_ ? "ON" : "OFF");
}

climate::ClimateTraits DaewooAC::traits() {
  auto traits = climate::ClimateTraits();
  
  // Supported modes
  traits.set_supported_modes({
    climate::CLIMATE_MODE_OFF,
    climate::CLIMATE_MODE_COOL,
    climate::CLIMATE_MODE_HEAT,
    climate::CLIMATE_MODE_DRY,
    climate::CLIMATE_MODE_FAN_ONLY,
    climate::CLIMATE_MODE_AUTO,
  });
  
  // Supported fan modes
  traits.set_supported_fan_modes({
    climate::CLIMATE_FAN_AUTO,
    climate::CLIMATE_FAN_LOW,
    climate::CLIMATE_FAN_MEDIUM,
    climate::CLIMATE_FAN_HIGH,
    climate::CLIMATE_FAN_QUIET,
  });
  
  // Supported swing modes
  traits.set_supported_swing_modes({
    climate::CLIMATE_SWING_OFF,
    climate::CLIMATE_SWING_BOTH,
    climate::CLIMATE_SWING_VERTICAL,
    climate::CLIMATE_SWING_HORIZONTAL,
  });
  
  // Temperature settings
  traits.add_feature_flags(climate::ClimateFeature::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
  traits.set_visual_min_temperature(static_cast<float>(MIN_CURRENT_TEMPERATURE));
  traits.set_visual_max_temperature(static_cast<float>(MAX_CURRENT_TEMPERATURE));
  traits.set_visual_temperature_step(1.0);
  
  return traits;
}

void DaewooAC::set_vertical_vane_position(const std::string &label) {
  for (size_t i = 0; i < this->vertical_vane_labels_.size(); ++i) {
    if (this->vertical_vane_labels_[i] == label) {
      this->apply_vertical_vane_position(static_cast<VerticalVanePosition>(i));
      return;
    }
  }
  ESP_LOGW(TAG, "Unknown vertical vane label '%s'; keeping current state", label.c_str());
}

void DaewooAC::set_vertical_vane_position(VerticalVanePosition position) {
  this->apply_vertical_vane_position(position);
}

void DaewooAC::set_vertical_vane_labels(const FixedVector<const char *> &options) {
  if (options.empty()) {
    ESP_LOGW(TAG, "Vertical vane select configured without options; keeping defaults");
    return;
  }

  size_t limit = std::min(options.size(), this->vertical_vane_labels_.size());
  for (size_t i = 0; i < limit; ++i) {
    this->vertical_vane_labels_[i] = options[i];
  }

  if (options.size() < this->vertical_vane_labels_.size()) {
    ESP_LOGW(TAG, "Vertical vane select provided %zu options but %zu are expected; missing entries keep defaults",
             options.size(), this->vertical_vane_labels_.size());
  } else if (options.size() > this->vertical_vane_labels_.size()) {
    ESP_LOGW(TAG, "Vertical vane select provided %zu options; only the first %zu are used",
             options.size(), this->vertical_vane_labels_.size());
  }

  this->vertical_vane_display_value_ =
      this->vertical_vane_labels_[static_cast<size_t>(this->vertical_vane_position_)];
}

void DaewooAC::apply_vertical_vane_position(VerticalVanePosition position) {
  this->vertical_vane_position_ = position;
  this->vertical_vane_display_value_ =
      this->vertical_vane_labels_[static_cast<size_t>(this->vertical_vane_position_)];

  ESP_LOGD(TAG, "Vertical vane position changed to: %s", this->vertical_vane_display_value_.c_str());

  if (this->vertical_vane_select_ != nullptr) {
    this->vertical_vane_select_->publish_state(this->vertical_vane_display_value_);
  }

  this->update_swing_mode();
}

void DaewooAC::update_swing_mode() {
  bool vertical_is_swing = (this->vertical_vane_position_ == VerticalVanePosition::SWING);
  bool horizontal_is_swing = this->horizontal_swing_on_;
  
  climate::ClimateSwingMode new_swing_mode;
  if (vertical_is_swing && horizontal_is_swing) {
    new_swing_mode = climate::CLIMATE_SWING_BOTH;
  } else if (vertical_is_swing && !horizontal_is_swing) {
    new_swing_mode = climate::CLIMATE_SWING_VERTICAL;
  } else if (!vertical_is_swing && horizontal_is_swing) {
    new_swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
  } else {
    new_swing_mode = climate::CLIMATE_SWING_OFF;
  }
  
  this->swing_mode = new_swing_mode;
  
  ESP_LOGD(TAG, "Swing mode updated to: %d (vertical: %s, horizontal: %s)", new_swing_mode,
           this->vertical_vane_display_value_.c_str(), this->horizontal_swing_on_ ? "Swing" : "Static");
}

void DaewooAC::set_display_on(bool on) {
  if (this->display_on_ == on) {
    ESP_LOGD(TAG, "Display already %s", on ? "ON" : "OFF");
  } else {
    this->display_on_ = on;
    ESP_LOGD(TAG, "Display state changed to: %s", on ? "ON" : "OFF");
  }

  if (this->display_switch_ != nullptr) {
    this->display_switch_->publish_state(this->display_on_);
  }
}

void DaewooAC::set_uv_light_on(bool on) {
  if (this->uv_light_on_ == on) {
    ESP_LOGD(TAG, "UV Light already %s", on ? "ON" : "OFF");
  } else {
    this->uv_light_on_ = on;
    ESP_LOGD(TAG, "UV Light state changed to: %s", on ? "ON" : "OFF");
  }

  if (this->uv_light_switch_ != nullptr) {
    this->uv_light_switch_->publish_state(this->uv_light_on_);
  }
}

void DaewooAC::set_horizontal_swing_on(bool on) {
  if (this->horizontal_swing_on_ == on) {
    ESP_LOGD(TAG, "Horizontal swing already %s", on ? "ON" : "OFF");
  } else {
    this->horizontal_swing_on_ = on;
    ESP_LOGD(TAG, "Horizontal swing state changed to: %s", on ? "ON" : "OFF");
  }

  this->update_swing_mode();
}

}  // namespace daewoo_ac
}  // namespace esphome


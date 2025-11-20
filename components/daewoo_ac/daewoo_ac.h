#pragma once

#include <array>
#include <vector>
#include <string>

#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/select/select.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace daewoo_ac {

static constexpr uint8_t QUIET_FLAG_MASK = 0x01;
static constexpr uint8_t DISPLAY_FLAG_MASK = 0x10;
static constexpr uint8_t UV_LIGHT_FLAG_MASK = 0x02;
static constexpr uint8_t HORIZONTAL_SWING_FLAG_MASK = 0x02;

static constexpr uint8_t MIN_TARGET_TEMPERATURE = 16;
static constexpr uint8_t MAX_TARGET_TEMPERATURE = 32;

static constexpr uint8_t MIN_CURRENT_TEMPERATURE = 10;
static constexpr uint8_t MAX_CURRENT_TEMPERATURE = 40;
static constexpr size_t VERTICAL_VANE_OPTION_COUNT = 7;

static constexpr uint32_t UPDATE_INTERVAL_DEFAULT_MILLIS = 2000;

// Length (in bytes) of Daewoo AC UART messages we care about
static constexpr size_t MESSAGE_LENGTH = 22;

enum class VerticalVanePosition : uint8_t {
  SWING = 0,
  UP = 1,
  UP_MEDIUM = 2,
  MEDIUM = 3,
  MEDIUM_DOWN = 4,
  DOWN = 5,
  STATIC = 6,
};

class DaewooAC : public climate::Climate, public Component {
 public:
  void setup() override;
  void loop() override;
  void control(const climate::ClimateCall &call) override;
  climate::ClimateTraits traits() override;

  // Queue a UI-originated change (property name and expected value).
  // The queue only stores a limited history of recent changes.
  void enqueue_ui_change(const std::string &property, const std::string &value);

  void set_update_interval(uint32_t update_interval_ms) { this->update_interval_ms_ = update_interval_ms; }
  void set_uart(uart::UARTComponent *uart) { this->uart_ = uart; }
  void set_vertical_vane_select(select::Select *vertical_vane) { this->vertical_vane_select_ = vertical_vane; }
  void set_display_switch(switch_::Switch *display_switch) { this->display_switch_ = display_switch; }
  void set_uv_light_switch(switch_::Switch *uv_light_switch) { this->uv_light_switch_ = uv_light_switch; }

  const std::string &get_vertical_vane_display_value() const { return this->vertical_vane_display_value_; }
  VerticalVanePosition get_vertical_vane_position_state() const { return this->vertical_vane_position_; }
  bool is_display_on() const { return this->display_on_; }
  bool is_uv_light_on() const { return this->uv_light_on_; }
  void set_vertical_vane_position(const std::string &label);
  void set_vertical_vane_position(VerticalVanePosition position);
  void set_vertical_vane_labels(const FixedVector<const char *> &options);
  void set_display_on(bool on);
  void set_uv_light_on(bool on);
  bool is_horizontal_swing_on() const { return this->horizontal_swing_on_; }
  void set_horizontal_swing_on(bool on);
  
  void update_swing_mode();

 private:
 struct UiChangeEntry {
  std::string property;
  std::string value;
};

struct DaewooState {
  uint8_t operation;           // 0x01 for read operation, 0x02 for write operation
  uint8_t power_state;         // done
  uint8_t reserved0;
  uint8_t vertical_vane;       // done
  uint8_t flags0;
  uint8_t mode;                // done
  uint8_t flags1;              // bits 1, 2, 5 - done
  uint8_t fan_mode;            // done
  uint8_t target_temperature;  // done
  uint8_t current_temperature; // done
  uint8_t reserved1[9];
  uint8_t checksum;            // done
};

  // Build a Daewoo UART command frame representing the desired state.
  // The frame is based on the last known Daewoo state (`daewoo_state_`)
  // with all pending UI changes from `ui_change_queue_` applied on top.
  // The returned array always has length `MESSAGE_LENGTH` (22 bytes),
  // starts with 0xAA 0x14 and ends with a valid checksum byte.
  std::array<uint8_t, MESSAGE_LENGTH> build_command_frame_from_state_();

  // Apply a single UI change entry to a mutable DaewooState instance.
  void apply_ui_change_to_state_(DaewooState &state, const UiChangeEntry &change) const;

  // Last known raw Daewoo state as received over UART.
  DaewooState daewoo_state_{};

  // Timestamp of the last periodic update
  uint32_t last_update_{0};

  // Simple receive buffer for assembling UART responses without blocking
  std::vector<uint8_t> uart_rx_buffer_;

  // Bounded FIFO queue of recent UI-driven changes.
  static constexpr size_t UI_CHANGE_QUEUE_MAX_SIZE = 50;
  std::vector<UiChangeEntry> ui_change_queue_;

  // Parse raw UART bytes into framed messages and dispatch valid ones.
  void parse_uart_response_(const std::vector<uint8_t> &buffer);

  void sync_daewoo_state();

 protected:
  // Mock state variables
  float current_temperature_{22.0};
  float target_temperature_{24.0};
  climate::ClimateMode current_mode_{climate::CLIMATE_MODE_OFF};
  climate::ClimateFanMode current_fan_mode_{climate::CLIMATE_FAN_AUTO};
  uint32_t update_interval_ms_{UPDATE_INTERVAL_DEFAULT_MILLIS};
  uart::UARTComponent *uart_{nullptr};
  switch_::Switch *display_switch_{nullptr};
  switch_::Switch *uv_light_switch_{nullptr};
  
  // Vane position selectors
  select::Select *vertical_vane_select_{nullptr};
  std::array<std::string, VERTICAL_VANE_OPTION_COUNT> vertical_vane_labels_{
      {"Swing", "Up", "Up & Medium", "Medium", "Medium & Down", "Down", "Static"}};
  VerticalVanePosition vertical_vane_position_{VerticalVanePosition::STATIC};
  std::string vertical_vane_display_value_{"Static"};
  bool display_on_{true};
  bool uv_light_on_{false};
  bool horizontal_swing_on_{false};

  void apply_vertical_vane_position(VerticalVanePosition position);
};

}  // namespace daewoo_ac
}  // namespace esphome


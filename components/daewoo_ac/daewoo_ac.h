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

// Fields identified by probing (see PROTOCOL.md). Not used by the component yet;
// they are preserved as-is because write frames start from the last received state.
// The AC only accepts writes to ext_flags while it is running (rejected when off).
static constexpr uint8_t EXT_FIELD_MASK = 0x1C;   // ext_flags bits 2-4: 3-bit value 0-7, purpose unknown
static constexpr uint8_t EXT_FIELD_SHIFT = 2;
static constexpr uint8_t EXT_FLAG_MASK = 0x40;    // ext_flags bit 6: purpose unknown
static constexpr uint8_t FLAGS0_UNKNOWN_MASK = 0x01;  // stored by the AC, no visible effect
static constexpr uint8_t FLAGS1_UNKNOWN_MASK = 0x04;  // stored by the AC, no visible effect
static constexpr uint8_t TIMER_MODE_MASK = 0x03;  // timer_mode: only the low 2 bits are stored
static constexpr uint8_t TIMER_MODE_NONE = 0x00;
static constexpr uint8_t TIMER_MODE_ON = 0x01;    // assumed by analogy, not captured from the remote
static constexpr uint8_t TIMER_MODE_OFF = 0x02;   // off-timer, confirmed with the IR remote

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

// Payload of a 22-byte frame (after the 0xAA 0x14 header). Byte offsets in
// comments are payload indexes; see PROTOCOL.md for the full byte map.
struct DaewooState {
  uint8_t operation;           // [0]  0x01 status/read, 0x02 write (the AC answers writes with an op 0x02 ack)
  uint8_t power_state;         // [1]  0x00 off, 0x01 on
  uint8_t ext_flags;           // [2]  EXT_FIELD_MASK (3-bit value) + EXT_FLAG_MASK; only writable while running
  uint8_t vertical_vane;       // [3]  0x00 swing, 0x01 down ... 0x05 up, 0x06 static
  uint8_t flags0;              // [4]  0x02 horizontal swing; 0x01 unknown (stored); 0x04/0x10 observed always set
  uint8_t mode;                // [5]  0 auto, 1 cool, 2 dry, 3 heat, 4 fan only
  uint8_t flags1;              // [6]  0x01 quiet, 0x02 UV light, 0x10 display; 0x04 unknown (stored)
  uint8_t fan_mode;            // [7]  0 auto, 1 low, 2 medium, 3 high
  uint8_t target_temperature;  // [8]  degrees C (16-32)
  uint8_t current_temperature; // [9]  degrees C, read only
  uint8_t timer_mode;          // [10] TIMER_MODE_* (2 bits); display shows the clock icon when non-zero
  uint8_t timer_hours;         // [11] 1-23, 0 = 24 h; ignored unless timer_mode is set
  uint8_t timer_counter;       // [12] changes about once a minute while a timer runs; meaning unknown, read only
  uint8_t reserved[6];         // [13-18] ignored by the AC on write, always 0x00
  uint8_t checksum;            // [19] sum of all frame bytes before it, modulo 256
};
static_assert(sizeof(DaewooState) == MESSAGE_LENGTH - 2, "DaewooState must match the 20-byte frame payload");

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


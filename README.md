# Daewoo AC (ESPHome component)

A minimal ESPHome climate platform component for demonstration purposes. This component provides a mock climate interface with demo data.

## Installation

### As an External Component

Add this to your ESPHome YAML configuration:

```yaml
external_components:
  - source:
      type: local
      path: components
    components: [daewoo_ac]

climate:
  - platform: daewoo_ac
    id: daewoo_ac_unit
    name: "Daewoo AC"

select:
  - platform: daewoo_ac
    daewoo_ac_id: daewoo_ac_unit
    vertical_vane:
      name: "Vertical Vane Position"

switch:
  - platform: daewoo_ac
    daewoo_ac_id: daewoo_ac_unit
    display:
      name: "Daewoo AC Display"
    uv_light:
      name: "Daewoo AC UV Light"
    horizontal_swing:
      name: "Daewoo AC Horizontal Swing"
```

### From GitHub

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/yourusername/esphome_daewoo_ac
    components: [daewoo_ac]

climate:
  - platform: daewoo_ac
    id: daewoo_ac_unit
    name: "Daewoo AC"

select:
  - platform: daewoo_ac
    daewoo_ac_id: daewoo_ac_unit
    vertical_vane:
      name: "Vertical Vane Position"

switch:
  - platform: daewoo_ac
    daewoo_ac_id: daewoo_ac_unit
    display:
      name: "Daewoo AC Display"
    uv_light:
      name: "Daewoo AC UV Light"
    horizontal_swing:
      name: "Daewoo AC Horizontal Swing"
```

## Features

This minimal implementation includes:

- **Climate Modes**: Off, Cool, Heat, Dry, Fan Only, Auto
- **Fan Modes**: Auto, Low, Medium, High
- **Vane Position Controls**: 
  - Vertical Vane Position (Swing, Static, Up, Medium, Down, Up & Medium, Medium & Down)
  - Horizontal Vane Position (Swing, Static, Up, Medium, Down, Up & Medium, Medium & Down)
- **Display Toggle**: Expose the AC's display on/off state as a switch entity
- **UV Light Toggle**: Simulate the Daewoo AC's UV light sanitizer switch
- **Temperature Range**: 17°C - 30°C (configurable steps of 1°C)
- **Mock Behavior**: Simulates temperature changes every 5 seconds based on the selected mode
- **UART Communication**: Sends commands to the AC unit and logs responses

## Configuration Options

### Climate Component

#### Required Parameters

- `uart_id`: The ID of the UART component to use for communication with the AC unit

#### Optional Parameters

- `name`: The name of the climate entity (default: "Daewoo AC")
- `update_interval`: How often to update the temperature simulation (default: `5s`)

### Vane Position Selectors

#### Required Parameters

- `daewoo_ac_id`: The ID of the Daewoo AC climate component

#### Optional Parameters

- `vertical_vane`: Configuration for vertical vane position selector
  - `name`: The name of the entity (required)
  - `options`: Custom list of exactly 7 option names (optional, defaults to default options)

**Default Options**: `["Swing", "Static", "Up", "Medium", "Down", "Up & Medium", "Medium & Down"]`

You can customize the option names to match your AC unit's terminology or use different languages.  
**Note:** When specifying custom options you must provide exactly 7 entries to match the AC's vane positions.

### Display Toggle Switch

#### Required Parameters

- `daewoo_ac_id`: The ID of the Daewoo AC climate component
- `display`: Standard ESPHome switch configuration block (requires `name`)

This exposes a switch entity that mirrors the mock "Display On" state in the demo component.  
Use it to simulate toggling the AC's physical display visibility.

### UV Light Toggle Switch

#### Required Parameters

- `daewoo_ac_id`: The ID of the Daewoo AC climate component
- `uv_light`: Standard ESPHome switch configuration block (requires `name`)

This optional switch entity mirrors the mock "UV Light On" state that is part of the demo component.  
Use it to simulate enabling or disabling the AC's UV sanitizing lights.

### Complete Example

```yaml
uart:
  id: uart_bus
  tx_pin: GPIO17
  rx_pin: GPIO16
  baud_rate: 9600

climate:
  - platform: daewoo_ac
    id: daewoo_ac_unit
    name: "Daewoo AC"
    uart_id: uart_bus
    update_interval: 3s

# Configure vane position selectors
select:
  - platform: daewoo_ac
    daewoo_ac_id: daewoo_ac_unit
    vertical_vane:
      name: "Vertical Vane Position"

switch:
  - platform: daewoo_ac
    daewoo_ac_id: daewoo_ac_unit
    display:
      name: "Daewoo AC Display"
    uv_light:
      name: "Daewoo AC UV Light"
    horizontal_swing:
      name: "Daewoo AC Horizontal Swing"
```

This will create:
- A climate entity in Home Assistant named "Daewoo AC" with controls for temperature, mode, and fan speed
- Two select entities for controlling vertical and horizontal vane positions with default options:
  - Swing
  - Static
  - Up
  - Medium
  - Down
  - Up & Medium
  - Medium & Down

- Two switch entities to toggle the Daewoo AC's display and UV light states

### Customizing Option Names

You can customize the option names for each vane selector:

```yaml
select:
  - platform: daewoo_ac
    daewoo_ac_id: daewoo_ac_unit
    vertical_vane:
      name: "Vertical Vane Position"
      options:
        - "Auto"
        - "Fixed"
        - "Top"
        - "Center"
        - "Bottom"
        - "Top & Center"
        - "Center & Bottom"
```

This allows you to:
- Use different terminology that matches your AC unit
- Translate option names to your preferred language
- Adapt to different AC models with different capabilities

See `example.yaml` for a complete configuration example.

## Project Structure

```
components/
  daewoo_ac/
    __init__.py              # Component initialization and namespace definition
    climate.py               # Climate platform registration and configuration
    select.py                # Select platform for vane position controls
    switch.py                # Switch platform for the display & UV toggles
    daewoo_ac.h              # Main C++ header file
    daewoo_ac.cpp            # Main C++ implementation with mock logic
    daewoo_ac_select.h       # Vane position select C++ header
    daewoo_ac_select.cpp     # Vane position select C++ implementation
    daewoo_ac_display_switch.h  # Display switch C++ header
    daewoo_ac_display_switch.cpp # Display switch C++ implementation
    daewoo_ac_uv_light_switch.h  # UV light switch C++ header
    daewoo_ac_uv_light_switch.cpp # UV light switch C++ implementation
```

## Development

This is a demonstration component with mocked functionality. In a real implementation, you would:

1. Replace mock temperature reading with actual sensor data
2. Implement real AC control via IR, UART, or other communication protocol
3. Add additional features like swing modes, preset modes, etc.
4. Implement proper error handling and state synchronization

## License

See LICENSE file for details.

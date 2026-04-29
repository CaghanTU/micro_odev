# Software Implementation

## 1. Firmware Architecture

The firmware was implemented for the ESP32 DevKit V1 using the PlatformIO environment and the Arduino framework. The software follows a modular embedded architecture in which hardware configuration, sensor acquisition, actuator control, user interface, logging, alarm handling, and web monitoring are separated into independent modules.

The firmware uses non-blocking `millis()`-based timing instead of blocking delays. This allows the ESP32 to scan the keypad, update the LCD, serve the web dashboard, read the sensor, and run the control logic in the same main loop without stopping the system.

All hardware pins, timing intervals, setpoint limits, control tolerances, fan speeds, Wi-Fi credentials, LCD address, safety thresholds, and simulation settings are centralized in `config.h`.

## 2. Module Responsibilities

The implementation is divided into the following modules:

| Module | Responsibility |
|---|---|
| `config.h` | Stores all hardware constants, timing values, setpoint limits, safety thresholds, Wi-Fi settings, PWM values, and hardware notes. |
| `system_types.h` | Defines the `SystemState` enum, alarm reason enum, and shared `SystemData` structure. |
| `sensors.cpp` | Reads the DHT22 sensor, performs non-blocking retry handling, tracks sensor failures, and supports optional simulation mode. |
| `actuators.cpp` | Controls heater, Peltier, humidifier, fan, and buzzer outputs using ESP32 LEDC PWM or configurable digital fallback. |
| `controller.cpp` | Implements the main safety and environmental control logic. |
| `alarm.cpp` | Sets, maintains, and acknowledges alarm states. |
| `display.cpp` | Updates the 20x4 I2C LCD display with current readings, setpoints, state, and last message. |
| `keypad_input.cpp` | Handles keypad input for temperature and humidity setpoints and alarm acknowledgement. |
| `logger.cpp` | Stores the last 10 events in a RAM ring buffer and appends logs to ESP32 internal flash using LittleFS. |
| `web_dashboard.cpp` | Starts the ESP32 Access Point and serves a read-only web dashboard. |
| `main.cpp` | Initializes modules and coordinates periodic updates in the main loop. |

## 3. Main Control Loop

The `setup()` function initializes serial output, default setpoints, actuators, alarm state, sensor module, LCD, keypad, web dashboard, and controller state. On boot, the firmware starts in `INIT`, then transitions to `IDLE`. All actuators and the buzzer are initially disabled.

The `loop()` function coordinates the system by calling:

1. `keypadInputUpdate()`
2. `sensorsUpdate()`
3. `controllerUpdate()`
4. `displayUpdate()`
5. `webDashboardHandle()`

The loop does not use `delay()`. Each module decides internally whether its update interval has elapsed.

## 4. Sensor Acquisition and Retry Logic

The system supports two active DHT22 sensors, matching the report component list of three DHT22 modules where one module is treated as a spare. The default active sensor pins are GPIO23 and GPIO15. Both active sensors must return valid data; their readings are averaged to obtain the cabinet temperature and humidity. If only one sensor is available during prototype testing, the active sensor count can be reduced in `config.h`.

Normal sensor sampling occurs every 2 seconds. If the first read fails, the firmware schedules one short non-blocking retry after 250 ms. A failed sample attempt is counted only if all active sensors fail during both the normal read and the retry.

The last valid temperature and humidity readings are not overwritten with invalid or `NaN` values. After any successful reading, the sensor failure counter is reset to zero. If three consecutive sample attempts fail, the system enters the `ALARM` state with the message `Sensor Error`.

For hardware-free verification, a compile-time simulation mode is available but disabled by default. In simulation mode, fake temperature and humidity values are generated so the control logic, LCD state, web dashboard, alarms, and logs can be compiled and exercised without a DHT22 sensor.

## 5. Temperature and Humidity Control Logic

The first firmware version uses safe threshold-based control. PID constants are defined in `config.h` for future tuning, but the current implementation does not depend on aggressive PID behavior.

Temperature control uses a tolerance of ±1°C:

| Condition | Action |
|---|---|
| Temperature < setpoint - 1°C | Heater enabled, Peltier disabled, fan low. |
| Temperature > setpoint + 1°C | Peltier enabled, heater disabled, fan high. |
| Temperature within tolerance | Heater and Peltier disabled. |

Humidity control uses a tolerance of ±5% RH:

| Condition | Action |
|---|---|
| Humidity < setpoint - 5% RH | Humidifier enabled, fan low. |
| Humidity > setpoint + 5% RH | Humidifier disabled, fan high. |
| Humidity within tolerance | Humidifier disabled. |

The displayed `SystemState` represents the dominant control state. If temperature and humidity both require action, temperature control has priority for the dominant state, but safe actuator combinations are still allowed. For example, heater + humidifier + low fan may run together. Heater and Peltier are never allowed to run at the same time.

## 6. Safety and Alarm Handling

Safety logic has priority over normal environmental control. The controller checks existing alarms, sensor failure, and overtemperature before applying normal heating, cooling, humidifying, or drying logic.

If the measured temperature exceeds 45°C, the firmware enters `ALARM` with an overtemperature message. Heater, Peltier, and humidifier outputs are disabled immediately. During an overtemperature alarm, the fan runs at high speed.

If a sensor error alarm is active, the firmware does not continue normal control using stale readings. In `ALARM`, heater, Peltier, and humidifier remain disabled and the buzzer is enabled. For a sensor error alarm, the fan uses the configured safe alarm speed.

The `C` key acknowledges alarms only when the unsafe condition is cleared. Overtemperature cannot be acknowledged while the temperature remains above the safety threshold. Sensor Error cannot be acknowledged until a later valid DHT22 reading has succeeded and the consecutive failure counter has returned to zero.

## 7. LCD and Keypad Interface

The local interface consists of a 20x4 I2C LCD and a 4x4 matrix keypad. The firmware calls `Wire.begin(SDA, SCL)` before LCD initialization. The LCD address is configurable and defaults to `0x27`.

The keypad row pins are GPIO13, GPIO12, GPIO14, and GPIO27, and the column pins are GPIO26, GPIO25, GPIO33, and GPIO32. This matches the pin table in the hardware report. Since GPIO12 is an ESP32 boot-strapping pin, the keypad circuit should not force this pin into an invalid boot level during reset.

LCD layout:

| Line | Content |
|---|---|
| 1 | Current temperature and temperature setpoint. |
| 2 | Current humidity and humidity setpoint. |
| 3 | Current system state and alarm reason when active. |
| 4 | Last log or alarm message. |

Keypad functions:

| Key | Function |
|---|---|
| `A` | Enter temperature setpoint input mode. |
| `B` | Enter humidity setpoint input mode. |
| `0-9` | Enter integer setpoint digits. |
| `#` | Confirm input. |
| `*` | Cancel input. |
| `C` | Acknowledge alarm if safe. |
| `D` | Reserved for future use. |

Setpoints are integer values in this firmware version. Temperature setpoints outside 0-40°C and humidity setpoints outside 20-100% RH are rejected, the previous value is kept, and an invalid input event is logged.

## 8. Web Dashboard

The ESP32 operates in Access Point mode using:

| Setting | Value |
|---|---|
| SSID | `CalibCabinet_AP` |
| Password | `calibration123` |
| Address | `http://192.168.4.1` |

The web dashboard is read-only. It displays current temperature, current humidity, temperature setpoint, humidity setpoint, system state, last message, and the last 10 log entries. The page automatically refreshes every 5 seconds. Log and message strings are HTML-escaped before rendering to avoid malformed dashboard output.

## 9. Logging Strategy

The firmware keeps the latest 10 log entries in a RAM ring buffer for fast LCD/web access. In addition, log entries are appended to ESP32 internal flash using LittleFS at `/calibration_log.csv`, which better matches the report requirement for storing calibration data in internal memory.

The logger records:

- valid sensor readings,
- setpoint changes,
- invalid setpoint attempts,
- alarm events,
- alarm acknowledgements.

Timestamps are based on seconds since boot and formatted as `HH:MM:SS`. This approach avoids dependence on an external real-time clock.

## 10. Hardware-Free Verification Results

Hardware-free verification was performed without connecting the ESP32 or physical components. The following checks passed:

| Verification Item | Result |
|---|---|
| Normal firmware build: `pio run -e esp32dev` | Passed |
| Simulation-mode build: `pio run -e esp32dev-sim` | Passed |
| Static review of safety priority | Passed |
| Static review of actuator mutual exclusion | Passed |
| Static review of non-blocking loop timing | Passed |
| Static review of LCD, keypad, web, and logging coverage | Passed |

The physical ESP32 board, DHT22 sensors, LCD, keypad, MOSFET drivers, fan, humidifier, heater, Peltier module, and buzzer have not yet been tested with this firmware. Physical hardware testing will be performed when the assembled system is available.

## 11. Hardware Test Checklist for Later

When the hardware is available, the following tests should be performed:

- Upload the firmware to the ESP32 using PlatformIO.
- Confirm serial monitor boot messages.
- Verify that the LCD initializes at address `0x27`; try `0x3F` if no display appears.
- Confirm that DHT22 readings appear every 2 seconds.
- Confirm that both active DHT22 sensors are detected and that readings are averaged.
- Disconnect or miswire the DHT22 temporarily to verify Sensor Error behavior after three failed sample attempts.
- Enter valid and invalid temperature setpoints using the keypad.
- Enter valid and invalid humidity setpoints using the keypad.
- Confirm that the heater output activates only below the temperature setpoint tolerance.
- Confirm that the Peltier output activates only above the temperature setpoint tolerance.
- Confirm that heater and Peltier outputs are never active together.
- Confirm humidifier activation below the humidity tolerance.
- Confirm high fan speed during drying and cooling.
- Confirm low fan speed during stable circulation and humidifying.
- Simulate or carefully test overtemperature behavior and verify heater/Peltier cutoff at 45°C.
- Confirm buzzer activation during alarms and acknowledgement with key `C` only after the fault is cleared.
- Connect a phone or laptop to `CalibCabinet_AP` and verify the read-only dashboard at `http://192.168.4.1`.
- Confirm that the dashboard refreshes every 5 seconds and shows the last 10 log entries.

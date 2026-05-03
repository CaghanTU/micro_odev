#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Hardware pin map
// ---------------------------------------------------------------------------
// Technical design report uses one DHT11 sensor connected to D4/GPIO4.
constexpr uint8_t DHT_SENSOR_PIN_COUNT = 1;
constexpr uint8_t DHT_SENSOR_COUNT = 1;
constexpr uint8_t DHT_SENSOR_PINS[DHT_SENSOR_PIN_COUNT] = {4};
constexpr bool REQUIRE_ALL_DHT_SENSORS_VALID = true;

constexpr uint8_t I2C_SDA_PIN = 21;
constexpr uint8_t I2C_SCL_PIN = 22;

constexpr uint8_t KEYPAD_ROWS = 4;
constexpr uint8_t KEYPAD_COLS = 3;
// Technical design report 4x3 keypad mapping:
// rows D26,D15,D32,D33 and columns D13,D12,D14.
// GPIO12 is a boot-strapping pin, so the keypad must not pull it into an
// invalid level during ESP32 reset.
constexpr uint8_t KEYPAD_ROW_PINS[KEYPAD_ROWS] = {26, 15, 32, 33};
constexpr uint8_t KEYPAD_COL_PINS[KEYPAD_COLS] = {13, 12, 14};

constexpr uint8_t HEATER_PWM_PIN = 27;
constexpr uint8_t PELTIER_PWM_PIN = 5;
constexpr uint8_t HUMIDIFIER_PIN = 18;
constexpr uint8_t CIRCULATION_FAN_PWM_PIN = 23;
constexpr uint8_t COOLING_FAN_PWM_PIN = 19;

// Active 5V buzzer should be driven through a transistor/MOSFET or suitable
// driver circuit, not directly from ESP32 GPIO if current exceeds GPIO limits.
constexpr uint8_t BUZZER_PIN = 25;
constexpr uint8_t BUZZER_ACTIVE_LEVEL = HIGH;
constexpr uint8_t BUZZER_INACTIVE_LEVEL = LOW;

// ---------------------------------------------------------------------------
// Hardware notes
// ---------------------------------------------------------------------------
// Relay module is reserved for future hard power cutoff or auxiliary switching.
// It is intentionally not used in v1 firmware unless relay pins are assigned.
#define RELAY_MODULE_ENABLED 0

// LCD may be powered at 5V while ESP32 uses 3.3V logic. This has no firmware
// effect. If the LCD I2C backpack is unreliable at 3.3V logic, use the
// bidirectional logic level converter between ESP32 SDA/SCL and LCD SDA/SCL.
constexpr uint8_t LCD_I2C_ADDRESS = 0x27; // Try 0x3F if the LCD does not respond.
constexpr uint8_t LCD_COLUMNS = 20;
constexpr uint8_t LCD_ROWS = 4;

// ---------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------
constexpr unsigned long SENSOR_READ_INTERVAL_MS = 2000UL;
constexpr unsigned long SENSOR_RETRY_DELAY_MS = 250UL;
constexpr unsigned long CONTROL_LOOP_INTERVAL_MS = 500UL;
constexpr unsigned long LCD_UPDATE_INTERVAL_MS = 1000UL;
constexpr unsigned long KEYPAD_SCAN_INTERVAL_MS = 50UL;
constexpr unsigned long WEB_REFRESH_INTERVAL_SECONDS = 5UL;
constexpr unsigned long MILLIS_ROLLOVER_HALF_RANGE = 0x80000000UL;

// ---------------------------------------------------------------------------
// Sensor configuration
// ---------------------------------------------------------------------------
// Adafruit DHT library uses DHT11 as sensor type value 11.
constexpr uint8_t DHT_SENSOR_TYPE = 11;
constexpr uint8_t SENSOR_FAILURE_LIMIT = 3;

// Hardware-free verification mode. Keep disabled for real hardware. When set
// to 1, sensors.cpp generates fake DHT11 readings so controller, LCD, web,
// alarms, and logs can be exercised without a physical sensor.
#ifndef ENABLE_SIMULATION_MODE
#define ENABLE_SIMULATION_MODE 0
#endif

constexpr float SIMULATION_START_TEMPERATURE_C = 22.0F;
constexpr float SIMULATION_START_HUMIDITY_RH = 50.0F;
constexpr float SIMULATION_AMBIENT_TEMPERATURE_C = 24.0F;
constexpr float SIMULATION_AMBIENT_HUMIDITY_RH = 45.0F;
constexpr float SIMULATION_HEATING_STEP_C = 0.8F;
constexpr float SIMULATION_COOLING_STEP_C = 0.7F;
constexpr float SIMULATION_PASSIVE_TEMP_STEP_C = 0.1F;
constexpr float SIMULATION_HUMIDIFYING_STEP_RH = 3.0F;
constexpr float SIMULATION_DRYING_STEP_RH = 2.5F;
constexpr float SIMULATION_PASSIVE_HUMIDITY_STEP_RH = 0.3F;

// Optional simulation fault injection. Leave at 0 for normal fake readings.
constexpr uint16_t SIMULATION_FORCE_SENSOR_FAILURE_AFTER_SAMPLES = 0;
constexpr uint16_t SIMULATION_FORCE_OVERTEMP_AFTER_SAMPLES = 0;

// ---------------------------------------------------------------------------
// Setpoints and safety
// ---------------------------------------------------------------------------
constexpr float DEFAULT_TEMPERATURE_SETPOINT_C = 25.0F;
constexpr float DEFAULT_HUMIDITY_SETPOINT_RH = 60.0F;

constexpr float MIN_TEMPERATURE_SETPOINT_C = 0.0F;
constexpr float MAX_TEMPERATURE_SETPOINT_C = 50.0F;
constexpr float MIN_HUMIDITY_SETPOINT_RH = 20.0F;
constexpr float MAX_HUMIDITY_SETPOINT_RH = 100.0F;

constexpr float TEMPERATURE_TOLERANCE_C = 1.0F;
constexpr float HUMIDITY_TOLERANCE_RH = 5.0F;

constexpr float OVER_TEMPERATURE_THRESHOLD_C = 50.0F;
constexpr float OVER_TEMPERATURE_CLEAR_C = 50.0F;

// PID constants are provided for future tuning. The v1 firmware uses safe
// threshold control by default instead of depending on aggressive PID tuning.
constexpr double TEMPERATURE_PID_KP = 2.0;
constexpr double TEMPERATURE_PID_KI = 0.1;
constexpr double TEMPERATURE_PID_KD = 0.5;
constexpr double HUMIDITY_PID_KP = 2.0;
constexpr double HUMIDITY_PID_KI = 0.05;
constexpr double HUMIDITY_PID_KD = 0.25;

// ---------------------------------------------------------------------------
// PWM / actuator configuration
// ---------------------------------------------------------------------------
// Set USE_LEDC_PWM to 0 if the local ESP32 Arduino LEDC API differs and a
// simple digital on/off fallback is preferred for classroom demonstration.
#ifndef USE_LEDC_PWM
#define USE_LEDC_PWM 1
#endif

constexpr uint32_t PWM_FREQUENCY_HZ = 5000;
constexpr uint8_t PWM_RESOLUTION_BITS = 8;
constexpr uint8_t PWM_MAX_DUTY = 255;

constexpr uint8_t HEATER_PWM_CHANNEL = 0;
constexpr uint8_t PELTIER_PWM_CHANNEL = 1;
constexpr uint8_t HUMIDIFIER_PWM_CHANNEL = 2;
constexpr uint8_t CIRCULATION_FAN_PWM_CHANNEL = 3;
constexpr uint8_t COOLING_FAN_PWM_CHANNEL = 4;

constexpr uint8_t HEATER_ON_POWER = 220;
constexpr uint8_t PELTIER_ON_POWER = 220;
constexpr uint8_t HUMIDIFIER_ON_POWER = 255;

constexpr uint8_t FAN_OFF = 0;
constexpr uint8_t FAN_LOW_CIRCULATION = 85;
constexpr uint8_t FAN_HIGH_DRYING = 220;
constexpr uint8_t FAN_HIGH_COOLING = 220;
constexpr uint8_t FAN_ALARM_SENSOR = FAN_OFF;
constexpr uint8_t COOLING_FAN_OFF = FAN_OFF;
constexpr uint8_t COOLING_FAN_ON = FAN_HIGH_COOLING;

// ---------------------------------------------------------------------------
// Wi-Fi dashboard
// ---------------------------------------------------------------------------
constexpr const char *WIFI_AP_SSID = "CalibCabinet_AP";
constexpr const char *WIFI_AP_PASSWORD = "calibration123";

// ---------------------------------------------------------------------------
// Logging and messages
// ---------------------------------------------------------------------------
constexpr uint8_t LOG_CAPACITY = 10;
constexpr bool PERSIST_LOG_TO_INTERNAL_FLASH = true;
constexpr const char *PERSISTENT_LOG_PATH = "/calibration_log.csv";
constexpr uint8_t MAX_SETPOINT_INPUT_DIGITS = 3;

#pragma once

#include <Arduino.h>
#include "config.h"

enum SystemState {
  INIT,
  IDLE,
  SENSING,
  EVALUATING,
  HEATING,
  COOLING,
  HUMIDIFYING,
  DRYING,
  STABLE,
  ALARM
};

enum AlarmReason {
  ALARM_NONE,
  ALARM_SENSOR_ERROR,
  ALARM_OVERTEMP
};

inline const char *stateToString(SystemState state) {
  switch (state) {
    case INIT: return "INIT";
    case IDLE: return "IDLE";
    case SENSING: return "SENSING";
    case EVALUATING: return "EVALUATING";
    case HEATING: return "HEATING";
    case COOLING: return "COOLING";
    case HUMIDIFYING: return "HUMIDIFYING";
    case DRYING: return "DRYING";
    case STABLE: return "STABLE";
    case ALARM: return "ALARM";
    default: return "UNKNOWN";
  }
}

inline const char *alarmReasonToString(AlarmReason reason) {
  switch (reason) {
    case ALARM_NONE: return "None";
    case ALARM_SENSOR_ERROR: return "Sensor Error";
    case ALARM_OVERTEMP: return "Overtemp";
    default: return "Unknown";
  }
}

struct SystemData {
  float currentTemperatureC = NAN;
  float currentHumidityRh = NAN;
  float temperatureSetpointC = DEFAULT_TEMPERATURE_SETPOINT_C;
  float humiditySetpointRh = DEFAULT_HUMIDITY_SETPOINT_RH;

  SystemState state = INIT;
  AlarmReason alarmReason = ALARM_NONE;

  bool alarmActive = false;
  bool sensorFaultActive = false;
  bool overTemperatureActive = false;
  bool hasValidSensorReading = false;

  String lastMessage = "Booting";
  unsigned long lastValidSensorTimestamp = 0;
  uint8_t consecutiveSensorFailures = 0;

  bool heaterActive = false;
  bool peltierActive = false;
  bool humidifierActive = false;
  bool buzzerActive = false;
  uint8_t heaterPower = 0;
  uint8_t peltierPower = 0;
  uint8_t fanSpeed = FAN_OFF;
  uint8_t coolingFanSpeed = COOLING_FAN_OFF;
};

#include "controller.h"

#include "actuators.h"
#include "alarm.h"

namespace {
uint8_t maxFanSpeed(uint8_t current, uint8_t candidate) {
  return candidate > current ? candidate : current;
}

void applyAlarmOutputs(SystemData &data) {
  data.state = ALARM;
  disableUnsafeActuators(data);
  setBuzzer(data, true);

  if (data.overTemperatureActive) {
    setFanSpeed(data, FAN_HIGH_COOLING);
    setCoolingFanSpeed(data, COOLING_FAN_ON);
  } else {
    setFanSpeed(data, FAN_ALARM_SENSOR);
    setCoolingFanSpeed(data, COOLING_FAN_OFF);
  }
}
}

void controllerInit(SystemData &data) {
  data.state = IDLE;
  data.overTemperatureActive = false;
}

void controllerUpdate(SystemData &data) {
  static unsigned long lastControlMs = 0;
  const unsigned long now = millis();

  if (now - lastControlMs < CONTROL_LOOP_INTERVAL_MS) {
    return;
  }
  lastControlMs = now;

  data.overTemperatureActive =
      data.hasValidSensorReading &&
      data.currentTemperatureC > OVER_TEMPERATURE_THRESHOLD_C;

  if (data.alarmActive) {
    applyAlarmOutputs(data);
    return;
  }

  if (data.sensorFaultActive ||
      data.consecutiveSensorFailures >= SENSOR_FAILURE_LIMIT) {
    data.sensorFaultActive = true;
    alarmSet(data, ALARM_SENSOR_ERROR, "Sensor Error");
    applyAlarmOutputs(data);
    return;
  }

  if (data.overTemperatureActive) {
    alarmSet(data, ALARM_OVERTEMP, "Overtemp");
    applyAlarmOutputs(data);
    return;
  }

  if (!data.hasValidSensorReading) {
    data.state = IDLE;
    disableUnsafeActuators(data);
    setFanSpeed(data, FAN_OFF);
    setCoolingFanSpeed(data, COOLING_FAN_OFF);
    setBuzzer(data, false);
    return;
  }

  data.state = EVALUATING;

  const bool needsHeating =
      data.currentTemperatureC < data.temperatureSetpointC - TEMPERATURE_TOLERANCE_C;
  const bool needsCooling =
      data.currentTemperatureC > data.temperatureSetpointC + TEMPERATURE_TOLERANCE_C;
  const bool needsHumidifying =
      data.currentHumidityRh < data.humiditySetpointRh - HUMIDITY_TOLERANCE_RH;
  const bool needsDrying =
      data.currentHumidityRh > data.humiditySetpointRh + HUMIDITY_TOLERANCE_RH;

  uint8_t heaterPower = 0;
  uint8_t peltierPower = 0;
  bool humidifierEnabled = false;
  uint8_t fanSpeed = FAN_OFF;
  uint8_t coolingFanSpeed = COOLING_FAN_OFF;
  SystemState dominantState = STABLE;

  if (needsHeating) {
    heaterPower = HEATER_ON_POWER;
    peltierPower = 0;
    fanSpeed = maxFanSpeed(fanSpeed, FAN_LOW_CIRCULATION);
    dominantState = HEATING;
  } else if (needsCooling) {
    peltierPower = PELTIER_ON_POWER;
    heaterPower = 0;
    fanSpeed = maxFanSpeed(fanSpeed, FAN_HIGH_COOLING);
    coolingFanSpeed = COOLING_FAN_ON;
    dominantState = COOLING;
  }

  if (needsHumidifying) {
    humidifierEnabled = true;
    fanSpeed = maxFanSpeed(fanSpeed, FAN_LOW_CIRCULATION);
    if (dominantState == STABLE) {
      dominantState = HUMIDIFYING;
    }
  } else if (needsDrying) {
    humidifierEnabled = false;
    fanSpeed = maxFanSpeed(fanSpeed, FAN_HIGH_DRYING);
    if (dominantState == STABLE) {
      dominantState = DRYING;
    }
  }

  if (dominantState == STABLE) {
    fanSpeed = FAN_LOW_CIRCULATION;
  }

  setHeaterPower(data, heaterPower);
  setPeltierPower(data, peltierPower);
  setHumidifier(data, humidifierEnabled);
  setFanSpeed(data, fanSpeed);
  setCoolingFanSpeed(data, coolingFanSpeed);
  setBuzzer(data, false);
  data.state = dominantState;
}

#include "controller.h"

#include "actuators.h"
#include "alarm.h"

namespace {
struct PidController {
  double integral = 0.0;
  double lastError = 0.0;
  bool hasLastError = false;
};

PidController temperaturePid;
PidController humidityPid;

uint8_t maxFanSpeed(uint8_t current, uint8_t candidate) {
  return candidate > current ? candidate : current;
}

double clampDouble(double value, double minimum, double maximum) {
  if (value < minimum) {
    return minimum;
  }
  if (value > maximum) {
    return maximum;
  }
  return value;
}

void resetPid(PidController &pid) {
  pid.integral = 0.0;
  pid.lastError = 0.0;
  pid.hasLastError = false;
}

double computePid(PidController &pid,
                  double setpoint,
                  double measured,
                  double kp,
                  double ki,
                  double kd,
                  double dtSeconds,
                  double integralLimit,
                  double outputLimit) {
  const double error = setpoint - measured;
  pid.integral += error * dtSeconds;
  pid.integral = clampDouble(pid.integral, -integralLimit, integralLimit);

  double derivative = 0.0;
  if (pid.hasLastError && dtSeconds > 0.0) {
    derivative = (error - pid.lastError) / dtSeconds;
  }

  pid.lastError = error;
  pid.hasLastError = true;

  const double output = (kp * error) + (ki * pid.integral) + (kd * derivative);
  return clampDouble(output, -outputLimit, outputLimit);
}

uint8_t pwmFromPidOutput(double output) {
  if (output <= 0.0) {
    return 0;
  }

  const double clamped = clampDouble(output, MIN_ACTIVE_PWM_POWER, PWM_MAX_DUTY);
  return static_cast<uint8_t>(clamped);
}

void applyAlarmOutputs(SystemData &data) {
  data.state = ALARM;
  disableUnsafeActuators(data);
  setBuzzer(data, true);
  resetPid(temperaturePid);
  resetPid(humidityPid);

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
  resetPid(temperaturePid);
  resetPid(humidityPid);
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
    resetPid(temperaturePid);
    resetPid(humidityPid);
    return;
  }

  if (!data.controlEnabled) {
    data.state = IDLE;
    disableUnsafeActuators(data);
    setFanSpeed(data, FAN_OFF);
    setCoolingFanSpeed(data, COOLING_FAN_OFF);
    setBuzzer(data, false);
    resetPid(temperaturePid);
    resetPid(humidityPid);
    return;
  }

  data.state = EVALUATING;

#if ENABLE_PID_CONTROL
  const double dtSeconds = CONTROL_LOOP_INTERVAL_MS / 1000.0;
  const double temperatureOutput =
      computePid(temperaturePid,
                 data.temperatureSetpointC,
                 data.currentTemperatureC,
                 TEMPERATURE_PID_KP,
                 TEMPERATURE_PID_KI,
                 TEMPERATURE_PID_KD,
                 dtSeconds,
                 TEMPERATURE_PID_INTEGRAL_LIMIT,
                 TEMPERATURE_PID_OUTPUT_LIMIT);
  const double humidityOutput =
      computePid(humidityPid,
                 data.humiditySetpointRh,
                 data.currentHumidityRh,
                 HUMIDITY_PID_KP,
                 HUMIDITY_PID_KI,
                 HUMIDITY_PID_KD,
                 dtSeconds,
                 HUMIDITY_PID_INTEGRAL_LIMIT,
                 HUMIDITY_PID_OUTPUT_LIMIT);

  const bool needsHeating =
      temperatureOutput > 0.0 &&
      data.currentTemperatureC < data.temperatureSetpointC - TEMPERATURE_TOLERANCE_C;
  const bool needsCooling =
      temperatureOutput < 0.0 &&
      data.currentTemperatureC > data.temperatureSetpointC + TEMPERATURE_TOLERANCE_C;
  const bool needsHumidifying =
      humidityOutput > 0.0 &&
      data.currentHumidityRh < data.humiditySetpointRh - HUMIDITY_TOLERANCE_RH;
  const bool needsDrying =
      humidityOutput < 0.0 &&
      data.currentHumidityRh > data.humiditySetpointRh + HUMIDITY_TOLERANCE_RH;
#else
  const bool needsHeating =
      data.currentTemperatureC < data.temperatureSetpointC - TEMPERATURE_TOLERANCE_C;
  const bool needsCooling =
      data.currentTemperatureC > data.temperatureSetpointC + TEMPERATURE_TOLERANCE_C;
  const bool needsHumidifying =
      data.currentHumidityRh < data.humiditySetpointRh - HUMIDITY_TOLERANCE_RH;
  const bool needsDrying =
      data.currentHumidityRh > data.humiditySetpointRh + HUMIDITY_TOLERANCE_RH;
#endif

  uint8_t heaterPower = 0;
  uint8_t peltierPower = 0;
  uint8_t humidifierPower = 0;
  uint8_t fanSpeed = FAN_OFF;
  uint8_t coolingFanSpeed = COOLING_FAN_OFF;
  SystemState dominantState = STABLE;

  if (needsHeating) {
#if ENABLE_PID_CONTROL
    heaterPower = pwmFromPidOutput(temperatureOutput);
#else
    heaterPower = HEATER_ON_POWER;
#endif
    peltierPower = 0;
    fanSpeed = maxFanSpeed(fanSpeed, FAN_LOW_CIRCULATION);
    dominantState = HEATING;
  } else if (needsCooling) {
#if ENABLE_PID_CONTROL
    peltierPower = pwmFromPidOutput(-temperatureOutput);
#else
    peltierPower = PELTIER_ON_POWER;
#endif
    heaterPower = 0;
    fanSpeed = maxFanSpeed(fanSpeed, FAN_HIGH_COOLING);
    coolingFanSpeed = COOLING_FAN_ON;
    dominantState = COOLING;
  }

  if (needsHumidifying) {
#if ENABLE_PID_CONTROL
    humidifierPower = pwmFromPidOutput(humidityOutput);
#else
    humidifierPower = HUMIDIFIER_ON_POWER;
#endif
    fanSpeed = maxFanSpeed(fanSpeed, FAN_LOW_CIRCULATION);
    if (dominantState == STABLE) {
      dominantState = HUMIDIFYING;
    }
  } else if (needsDrying) {
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
  setHumidifierPower(data, humidifierPower);
  setFanSpeed(data, fanSpeed);
  setCoolingFanSpeed(data, coolingFanSpeed);
  setBuzzer(data, false);
  data.state = dominantState;
}

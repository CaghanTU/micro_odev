#include "alarm.h"
#include "actuators.h"
#include "logger.h"

void alarmInit(SystemData &data) {
  data.alarmActive = false;
  data.alarmReason = ALARM_NONE;
  data.sensorFaultActive = false;
  data.overTemperatureActive = false;
}

void alarmSet(SystemData &data, AlarmReason reason, const String &message) {
  const bool isNewAlarm = !data.alarmActive || data.alarmReason != reason;

  data.alarmActive = true;
  data.alarmReason = reason;
  data.state = ALARM;

  disableUnsafeActuators(data);
  setBuzzer(data, true);

  if (isNewAlarm) {
    loggerAdd(data, "Alarm: " + message);
  } else {
    data.lastMessage = message;
  }
}

bool alarmCanClear(const SystemData &data) {
  if (!data.alarmActive) {
    return true;
  }

  if (data.overTemperatureActive ||
      (data.hasValidSensorReading && data.currentTemperatureC > OVER_TEMPERATURE_CLEAR_C)) {
    return false;
  }

  if (data.sensorFaultActive) {
    return false;
  }

  if (data.alarmReason == ALARM_SENSOR_ERROR) {
    return data.hasValidSensorReading && data.consecutiveSensorFailures == 0;
  }

  return true;
}

bool alarmAcknowledge(SystemData &data) {
  if (!data.alarmActive) {
    data.lastMessage = "No active alarm";
    return true;
  }

  if (!alarmCanClear(data)) {
    loggerAdd(data, "Alarm still active");
    setBuzzer(data, true);
    return false;
  }

  data.alarmActive = false;
  data.alarmReason = ALARM_NONE;
  data.state = IDLE;
  setBuzzer(data, false);
  loggerAdd(data, "Alarm acknowledged");
  return true;
}

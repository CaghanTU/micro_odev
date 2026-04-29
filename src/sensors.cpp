#include "sensors.h"

#include <DHT.h>
#include <math.h>

#include "logger.h"

namespace {
DHT dht(DHT_PIN, DHT_SENSOR_TYPE);

bool retryPending = false;
bool firstSample = true;
unsigned long lastSampleStartMs = 0;
unsigned long retryDueMs = 0;
uint16_t simulationSampleCount = 0;

bool millisDue(unsigned long now, unsigned long dueTime) {
  return now - dueTime < MILLIS_ROLLOVER_HALF_RANGE;
}

bool readDht(SystemData &data, bool forceRead) {
  const float humidity = dht.readHumidity(forceRead);
  // readHumidity() performs the physical DHT transaction. Temperature is then
  // taken from the same cached sample, avoiding a second forced DHT read.
  const float temperature = dht.readTemperature(false, false);

  if (isnan(humidity) || isnan(temperature)) {
    return false;
  }

  data.currentTemperatureC = temperature;
  data.currentHumidityRh = humidity;
  data.hasValidSensorReading = true;
  data.lastValidSensorTimestamp = millis();
  data.consecutiveSensorFailures = 0;
  data.sensorFaultActive = false;

  char buffer[48];
  snprintf(buffer, sizeof(buffer), "Sensor OK T:%.1f H:%.1f", temperature, humidity);
  loggerAdd(data, String(buffer));
  return true;
}

void storeValidReading(SystemData &data, float temperature, float humidity, const char *prefix) {
  data.currentTemperatureC = temperature;
  data.currentHumidityRh = humidity;
  data.hasValidSensorReading = true;
  data.lastValidSensorTimestamp = millis();
  data.consecutiveSensorFailures = 0;
  data.sensorFaultActive = false;

  char buffer[56];
  snprintf(buffer, sizeof(buffer), "%s T:%.1f H:%.1f", prefix, temperature, humidity);
  loggerAdd(data, String(buffer));
}

float moveToward(float current, float target, float step) {
  if (current < target) {
    current += step;
    return current > target ? target : current;
  }
  if (current > target) {
    current -= step;
    return current < target ? target : current;
  }
  return current;
}

void generateSimulationReading(SystemData &data) {
  float temperature = data.hasValidSensorReading
                          ? data.currentTemperatureC
                          : SIMULATION_START_TEMPERATURE_C;
  float humidity = data.hasValidSensorReading
                       ? data.currentHumidityRh
                       : SIMULATION_START_HUMIDITY_RH;

  if (data.heaterActive) {
    temperature += SIMULATION_HEATING_STEP_C;
  } else if (data.peltierActive) {
    temperature -= SIMULATION_COOLING_STEP_C;
  } else {
    temperature = moveToward(temperature, SIMULATION_AMBIENT_TEMPERATURE_C,
                             SIMULATION_PASSIVE_TEMP_STEP_C);
  }

  if (data.humidifierActive) {
    humidity += SIMULATION_HUMIDIFYING_STEP_RH;
  } else if (data.fanSpeed >= FAN_HIGH_DRYING) {
    humidity -= SIMULATION_DRYING_STEP_RH;
  } else {
    humidity = moveToward(humidity, SIMULATION_AMBIENT_HUMIDITY_RH,
                          SIMULATION_PASSIVE_HUMIDITY_STEP_RH);
  }

  humidity = constrain(humidity, 0.0F, 100.0F);

  if (SIMULATION_FORCE_OVERTEMP_AFTER_SAMPLES > 0 &&
      simulationSampleCount >= SIMULATION_FORCE_OVERTEMP_AFTER_SAMPLES) {
    temperature = OVER_TEMPERATURE_THRESHOLD_C + 2.0F;
  }

  storeValidReading(data, temperature, humidity, "Sim Sensor OK");
}

void recordFailedSample(SystemData &data) {
  if (data.consecutiveSensorFailures < 255) {
    data.consecutiveSensorFailures++;
  }

  if (data.consecutiveSensorFailures >= SENSOR_FAILURE_LIMIT) {
    data.sensorFaultActive = true;
  }

  char buffer[40];
  snprintf(buffer, sizeof(buffer), "Sensor failed %u/%u",
           data.consecutiveSensorFailures, SENSOR_FAILURE_LIMIT);
  loggerAdd(data, String(buffer));
}
}

void sensorsInit(SystemData &data) {
  dht.begin();
  retryPending = false;
  firstSample = true;
  lastSampleStartMs = 0;
  retryDueMs = 0;
  simulationSampleCount = 0;
  data.consecutiveSensorFailures = 0;
  data.sensorFaultActive = false;
  data.hasValidSensorReading = false;
}

bool sensorsUpdate(SystemData &data) {
  const unsigned long now = millis();

#if ENABLE_SIMULATION_MODE
  if (!firstSample && now - lastSampleStartMs < SENSOR_READ_INTERVAL_MS) {
    return false;
  }

  firstSample = false;
  lastSampleStartMs = now;
  simulationSampleCount++;

  if (data.state != ALARM) {
    data.state = SENSING;
  }

  if (SIMULATION_FORCE_SENSOR_FAILURE_AFTER_SAMPLES > 0 &&
      simulationSampleCount >= SIMULATION_FORCE_SENSOR_FAILURE_AFTER_SAMPLES) {
    recordFailedSample(data);
    return false;
  }

  generateSimulationReading(data);
  return true;
#else
  if (retryPending && millisDue(now, retryDueMs)) {
    retryPending = false;
    if (readDht(data, true)) {
      return true;
    }

    recordFailedSample(data);
    return false;
  }

  if (!firstSample && now - lastSampleStartMs < SENSOR_READ_INTERVAL_MS) {
    return false;
  }

  firstSample = false;
  lastSampleStartMs = now;

  if (data.state != ALARM) {
    data.state = SENSING;
  }

  if (readDht(data, false)) {
    retryPending = false;
    return true;
  }

  retryPending = true;
  retryDueMs = now + SENSOR_RETRY_DELAY_MS;
  return false;
#endif
}

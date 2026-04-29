#include "actuators.h"

#if USE_LEDC_PWM
#if __has_include(<esp_arduino_version.h>)
#include <esp_arduino_version.h>
#endif
#ifndef ESP_ARDUINO_VERSION_MAJOR
#define ESP_ARDUINO_VERSION_MAJOR 2
#endif
#endif

namespace {
uint8_t clampDuty(uint8_t duty) {
  return duty > PWM_MAX_DUTY ? PWM_MAX_DUTY : duty;
}

void configureOutputPin(uint8_t pin, uint8_t channel) {
  pinMode(pin, OUTPUT);
#if USE_LEDC_PWM
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  (void)channel;
  ledcAttach(pin, PWM_FREQUENCY_HZ, PWM_RESOLUTION_BITS);
#else
  ledcSetup(channel, PWM_FREQUENCY_HZ, PWM_RESOLUTION_BITS);
  ledcAttachPin(pin, channel);
#endif
#else
  (void)channel;
#endif
}

void writePwmOutput(uint8_t pin, uint8_t channel, uint8_t duty) {
  duty = clampDuty(duty);
#if USE_LEDC_PWM
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  (void)channel;
  ledcWrite(pin, duty);
#else
  (void)pin;
  ledcWrite(channel, duty);
#endif
#else
  (void)channel;
  digitalWrite(pin, duty > 0 ? HIGH : LOW);
#endif
}
}

void actuatorsInit(SystemData &data) {
  configureOutputPin(HEATER_PWM_PIN, HEATER_PWM_CHANNEL);
  configureOutputPin(PELTIER_PWM_PIN, PELTIER_PWM_CHANNEL);
  configureOutputPin(HUMIDIFIER_PIN, HUMIDIFIER_PWM_CHANNEL);
  configureOutputPin(FAN_PWM_PIN, FAN_PWM_CHANNEL);
  pinMode(BUZZER_PIN, OUTPUT);

  disableAllActuators(data);
}

void setHeaterPower(SystemData &data, uint8_t power) {
  power = clampDuty(power);
  if (power > 0 && data.peltierPower > 0) {
    writePwmOutput(PELTIER_PWM_PIN, PELTIER_PWM_CHANNEL, 0);
    data.peltierPower = 0;
    data.peltierActive = false;
  }

  writePwmOutput(HEATER_PWM_PIN, HEATER_PWM_CHANNEL, power);
  data.heaterPower = power;
  data.heaterActive = power > 0;
}

void setPeltierPower(SystemData &data, uint8_t power) {
  power = clampDuty(power);
  if (power > 0 && data.heaterPower > 0) {
    writePwmOutput(HEATER_PWM_PIN, HEATER_PWM_CHANNEL, 0);
    data.heaterPower = 0;
    data.heaterActive = false;
  }

  writePwmOutput(PELTIER_PWM_PIN, PELTIER_PWM_CHANNEL, power);
  data.peltierPower = power;
  data.peltierActive = power > 0;
}

void setHumidifier(SystemData &data, bool enabled) {
  writePwmOutput(HUMIDIFIER_PIN, HUMIDIFIER_PWM_CHANNEL,
                 enabled ? HUMIDIFIER_ON_POWER : 0);
  data.humidifierActive = enabled;
}

void setFanSpeed(SystemData &data, uint8_t speed) {
  speed = clampDuty(speed);
  writePwmOutput(FAN_PWM_PIN, FAN_PWM_CHANNEL, speed);
  data.fanSpeed = speed;
}

void setBuzzer(SystemData &data, bool enabled) {
  digitalWrite(BUZZER_PIN, enabled ? BUZZER_ACTIVE_LEVEL : BUZZER_INACTIVE_LEVEL);
  data.buzzerActive = enabled;
}

void disableUnsafeActuators(SystemData &data) {
  setHeaterPower(data, 0);
  setPeltierPower(data, 0);
  setHumidifier(data, false);
}

void disableAllActuators(SystemData &data) {
  setHeaterPower(data, 0);
  setPeltierPower(data, 0);
  setHumidifier(data, false);
  setFanSpeed(data, FAN_OFF);
  setBuzzer(data, false);
}

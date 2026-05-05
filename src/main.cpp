#include <Arduino.h>

#include "actuators.h"
#include "alarm.h"
#include "controller.h"
#include "display.h"
#include "keypad_input.h"
#include "logger.h"
#include "sensors.h"
#include "web_dashboard.h"

SystemData systemData;

void setup() {
  Serial.begin(115200);

  systemData.state = INIT;
  systemData.temperatureSetpointC = DEFAULT_TEMPERATURE_SETPOINT_C;
  systemData.humiditySetpointRh = DEFAULT_HUMIDITY_SETPOINT_RH;
  systemData.controlEnabled = START_CONTROL_ON_BOOT;
  systemData.lastMessage = "Booting";

  loggerInit();
  actuatorsInit(systemData);
  alarmInit(systemData);
  sensorsInit(systemData);
  displayInit(systemData);
  keypadInputInit(systemData);
  webDashboardInit(systemData);
  controllerInit(systemData);

  loggerAdd(systemData, "System ready");
}

void loop() {
  keypadInputUpdate(systemData);
  sensorsUpdate(systemData);
  controllerUpdate(systemData);
  displayUpdate(systemData);
  webDashboardHandle();
}

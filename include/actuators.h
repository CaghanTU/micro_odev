#pragma once

#include <Arduino.h>
#include "system_types.h"

void actuatorsInit(SystemData &data);
void setHeaterPower(SystemData &data, uint8_t power);
void setPeltierPower(SystemData &data, uint8_t power);
void setHumidifierPower(SystemData &data, uint8_t power);
void setHumidifier(SystemData &data, bool enabled);
void setFanSpeed(SystemData &data, uint8_t speed);
void setCoolingFanSpeed(SystemData &data, uint8_t speed);
void setBuzzer(SystemData &data, bool enabled);
void disableUnsafeActuators(SystemData &data);
void disableAllActuators(SystemData &data);

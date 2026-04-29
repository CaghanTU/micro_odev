#pragma once

#include <Arduino.h>
#include "system_types.h"

void alarmInit(SystemData &data);
void alarmSet(SystemData &data, AlarmReason reason, const String &message);
bool alarmCanClear(const SystemData &data);
bool alarmAcknowledge(SystemData &data);

#pragma once

#include <Arduino.h>
#include "system_types.h"

struct LogEntry {
  uint32_t secondsSinceBoot = 0;
  String message;
};

void loggerInit();
void loggerAdd(SystemData &data, const String &message);
uint8_t loggerCount();
LogEntry loggerGet(uint8_t chronologicalIndex);
String loggerFormatTimestamp(uint32_t secondsSinceBoot);

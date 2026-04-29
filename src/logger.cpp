#include "logger.h"

#include <LittleFS.h>

namespace {
LogEntry logEntries[LOG_CAPACITY];
uint8_t logHead = 0;
uint8_t logSize = 0;
bool persistentLogReady = false;

void appendPersistentLog(uint32_t secondsSinceBoot, const String &message) {
  if (!persistentLogReady) {
    return;
  }

  File file = LittleFS.open(PERSISTENT_LOG_PATH, FILE_APPEND);
  if (!file) {
    return;
  }

  String sanitized = message;
  sanitized.replace("\"", "\"\"");

  file.print(loggerFormatTimestamp(secondsSinceBoot));
  file.print(",\"");
  file.print(sanitized);
  file.println("\"");
  file.close();
}
}

void loggerInit() {
  logHead = 0;
  logSize = 0;
  persistentLogReady = false;

  if (PERSIST_LOG_TO_INTERNAL_FLASH) {
    persistentLogReady = LittleFS.begin(true);
    if (persistentLogReady && !LittleFS.exists(PERSISTENT_LOG_PATH)) {
      File file = LittleFS.open(PERSISTENT_LOG_PATH, FILE_WRITE);
      if (file) {
        file.println("time,message");
        file.close();
      }
    }
  }
}

String loggerFormatTimestamp(uint32_t secondsSinceBoot) {
  const uint32_t hours = secondsSinceBoot / 3600UL;
  const uint32_t minutes = (secondsSinceBoot % 3600UL) / 60UL;
  const uint32_t seconds = secondsSinceBoot % 60UL;

  char buffer[12];
  snprintf(buffer, sizeof(buffer), "%02lu:%02lu:%02lu",
           static_cast<unsigned long>(hours % 100UL),
           static_cast<unsigned long>(minutes),
           static_cast<unsigned long>(seconds));
  return String(buffer);
}

void loggerAdd(SystemData &data, const String &message) {
  const uint32_t nowSeconds = millis() / 1000UL;
  logEntries[logHead].secondsSinceBoot = nowSeconds;
  logEntries[logHead].message = message;
  logHead = (logHead + 1) % LOG_CAPACITY;
  if (logSize < LOG_CAPACITY) {
    logSize++;
  }

  data.lastMessage = message;

  Serial.print(loggerFormatTimestamp(nowSeconds));
  Serial.print(' ');
  Serial.println(message);

  appendPersistentLog(nowSeconds, message);
}

uint8_t loggerCount() {
  return logSize;
}

LogEntry loggerGet(uint8_t chronologicalIndex) {
  if (chronologicalIndex >= logSize) {
    return LogEntry();
  }

  const uint8_t oldestIndex = (logSize < LOG_CAPACITY) ? 0 : logHead;
  const uint8_t entryIndex = (oldestIndex + chronologicalIndex) % LOG_CAPACITY;
  return logEntries[entryIndex];
}

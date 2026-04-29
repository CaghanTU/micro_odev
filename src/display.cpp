#include "display.h"

#include <LiquidCrystal_I2C.h>
#include <Wire.h>

namespace {
LiquidCrystal_I2C lcd(LCD_I2C_ADDRESS, LCD_COLUMNS, LCD_ROWS);
String lastRenderedLines[LCD_ROWS];

String fitLine(const String &input) {
  String line = input;
  if (line.length() > LCD_COLUMNS) {
    line = line.substring(0, LCD_COLUMNS);
  }
  while (line.length() < LCD_COLUMNS) {
    line += ' ';
  }
  return line;
}

void writeLine(uint8_t row, const String &text) {
  if (row >= LCD_ROWS) {
    return;
  }

  const String line = fitLine(text);
  if (line == lastRenderedLines[row]) {
    return;
  }

  lcd.setCursor(0, row);
  lcd.print(line);
  lastRenderedLines[row] = line;
}

String formatTemperatureLine(const SystemData &data) {
  char buffer[24];
  if (!data.hasValidSensorReading) {
    snprintf(buffer, sizeof(buffer), "T:--.-C SP:%2.0fC", data.temperatureSetpointC);
  } else {
    snprintf(buffer, sizeof(buffer), "T:%5.1fC SP:%2.0fC",
             data.currentTemperatureC, data.temperatureSetpointC);
  }
  return String(buffer);
}

String formatHumidityLine(const SystemData &data) {
  char buffer[24];
  if (!data.hasValidSensorReading) {
    snprintf(buffer, sizeof(buffer), "H:--.-%% SP:%3.0f%%", data.humiditySetpointRh);
  } else {
    snprintf(buffer, sizeof(buffer), "H:%5.1f%% SP:%3.0f%%",
             data.currentHumidityRh, data.humiditySetpointRh);
  }
  return String(buffer);
}
}

void displayInit(SystemData &data) {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  lcd.init();
  lcd.backlight();
  lcd.clear();

  for (uint8_t i = 0; i < LCD_ROWS; i++) {
    lastRenderedLines[i] = "";
  }

  writeLine(0, "Calibration Cabinet");
  writeLine(1, "Initializing");
  writeLine(2, "");
  writeLine(3, data.lastMessage);
}

void displayUpdate(const SystemData &data) {
  static unsigned long lastUpdateMs = 0;
  const unsigned long now = millis();
  if (now - lastUpdateMs < LCD_UPDATE_INTERVAL_MS) {
    return;
  }
  lastUpdateMs = now;

  writeLine(0, formatTemperatureLine(data));
  writeLine(1, formatHumidityLine(data));

  String stateLine = "Status: ";
  stateLine += stateToString(data.state);
  if (data.alarmActive) {
    stateLine += " ";
    stateLine += alarmReasonToString(data.alarmReason);
  }
  writeLine(2, stateLine);
  writeLine(3, data.lastMessage);
}

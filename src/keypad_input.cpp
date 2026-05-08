#include "keypad_input.h"

#include "alarm.h"
#include "logger.h"

namespace {
char keymap[KEYPAD_ROWS][KEYPAD_COLS] = {
    {'1', '2', '3'},
    {'4', '5', '6'},
    {'7', '8', '9'},
    {'*', '0', '#'}};

byte rowPins[KEYPAD_ROWS] = {
    KEYPAD_ROW_PINS[0], KEYPAD_ROW_PINS[1],
    KEYPAD_ROW_PINS[2], KEYPAD_ROW_PINS[3]};
byte colPins[KEYPAD_COLS] = {
    KEYPAD_COL_PINS[0], KEYPAD_COL_PINS[1], KEYPAD_COL_PINS[2]};

enum InputMode {
  INPUT_NONE,
  INPUT_TEMPERATURE,
  INPUT_HUMIDITY
};

InputMode inputMode = INPUT_NONE;
String inputBuffer;
uint8_t lastKeyRow = 0;
uint8_t lastKeyCol = 0;
unsigned long lastStarPressMs = 0;

void configureKeypadPinsIdle() {
  for (uint8_t row = 0; row < KEYPAD_ROWS; row++) {
    pinMode(rowPins[row], OUTPUT);
    digitalWrite(rowPins[row], HIGH);
  }
  for (uint8_t col = 0; col < KEYPAD_COLS; col++) {
    pinMode(colPins[col], INPUT_PULLUP);
  }
}

char keyAt(uint8_t row, uint8_t col) {
  if (row >= KEYPAD_ROWS || col >= KEYPAD_COLS) {
    return '\0';
  }

  return keymap[row][col];
}

void scanKeypadMatrix(bool pressed[KEYPAD_ROWS][KEYPAD_COLS]) {
  for (uint8_t row = 0; row < KEYPAD_ROWS; row++) {
    digitalWrite(rowPins[row], HIGH);
  }

  for (uint8_t row = 0; row < KEYPAD_ROWS; row++) {
    digitalWrite(rowPins[row], LOW);
    delayMicroseconds(5);

    for (uint8_t col = 0; col < KEYPAD_COLS; col++) {
      pressed[row][col] = digitalRead(colPins[col]) == LOW;
    }

    digitalWrite(rowPins[row], HIGH);
  }
}

char readKeypadRaw() {
  static bool previousPressed[KEYPAD_ROWS][KEYPAD_COLS] = {};
  bool currentPressed[KEYPAD_ROWS][KEYPAD_COLS] = {};
  scanKeypadMatrix(currentPressed);

  char newKey = '\0';
  uint8_t newRow = 0;
  uint8_t newCol = 0;
  for (uint8_t row = 0; row < KEYPAD_ROWS; row++) {
    for (uint8_t col = 0; col < KEYPAD_COLS; col++) {
      if (currentPressed[row][col] && !previousPressed[row][col] && !newKey) {
        newKey = keyAt(row, col);
        newRow = row + 1;
        newCol = col + 1;
      }
      previousPressed[row][col] = currentPressed[row][col];
    }
  }

  if (!newKey) {
    return '\0';
  }

  lastKeyRow = newRow;
  lastKeyCol = newCol;
  return newKey;
}

void beginInput(SystemData &data, InputMode mode) {
  inputMode = mode;
  inputBuffer = "";
  data.lastMessage = mode == INPUT_TEMPERATURE ? "Temp setpoint:" : "Humidity setpoint:";
}

void cancelInput(SystemData &data) {
  inputMode = INPUT_NONE;
  inputBuffer = "";
  data.lastMessage = "Input cancelled";
}

void appendDigit(SystemData &data, char key) {
  if (inputMode == INPUT_NONE) {
    beginInput(data, INPUT_TEMPERATURE);
  }

  if (inputBuffer.length() >= MAX_SETPOINT_INPUT_DIGITS) {
    data.lastMessage = "Input too long";
    return;
  }

  inputBuffer += key;
  data.lastMessage = inputMode == INPUT_TEMPERATURE ? "Temp: " : "Humidity: ";
  data.lastMessage += inputBuffer;
}

void handleStarKey(SystemData &data) {
  const unsigned long now = millis();
  if (inputMode == INPUT_TEMPERATURE &&
      inputBuffer.length() == 0 &&
      now - lastStarPressMs < 10000UL) {
    beginInput(data, INPUT_HUMIDITY);
    lastStarPressMs = 0;
    return;
  }

  beginInput(data, INPUT_TEMPERATURE);
  lastStarPressMs = now;
}

void confirmInput(SystemData &data) {
  if (inputMode == INPUT_NONE) {
    return;
  }

  if (inputBuffer.length() == 0) {
    loggerAdd(data, "Invalid empty input");
    inputMode = INPUT_NONE;
    return;
  }

  const int value = inputBuffer.toInt();
  if (inputMode == INPUT_TEMPERATURE) {
    if (value < static_cast<int>(MIN_TEMPERATURE_SETPOINT_C) ||
        value > static_cast<int>(MAX_TEMPERATURE_SETPOINT_C)) {
      loggerAdd(data, "Invalid temp setpoint");
    } else {
      data.temperatureSetpointC = static_cast<float>(value);
      data.controlEnabled = true;
      loggerAdd(data, "Temp SP: " + String(value) + "C");
    }
  } else if (inputMode == INPUT_HUMIDITY) {
    if (value < static_cast<int>(MIN_HUMIDITY_SETPOINT_RH) ||
        value > static_cast<int>(MAX_HUMIDITY_SETPOINT_RH)) {
      loggerAdd(data, "Invalid humidity setpoint");
    } else {
      data.humiditySetpointRh = static_cast<float>(value);
      data.controlEnabled = true;
      loggerAdd(data, "Humidity SP: " + String(value) + "%");
    }
  }

  inputMode = INPUT_NONE;
  inputBuffer = "";
}
}

void keypadInputInit(SystemData &data) {
  inputMode = INPUT_NONE;
  inputBuffer = "";
  configureKeypadPinsIdle();
  data.lastMessage = "Keypad ready";
}

void keypadInputUpdate(SystemData &data) {
  static unsigned long lastScanMs = 0;
  const unsigned long now = millis();

  if (now - lastScanMs < KEYPAD_SCAN_INTERVAL_MS) {
    return;
  }
  lastScanMs = now;

  const char key = readKeypadRaw();
  if (!key) {
    return;
  }

  if (key >= '0' && key <= '9') {
    appendDigit(data, key);
    return;
  }

  switch (key) {
    case '#':
      if (inputMode != INPUT_NONE && inputBuffer.length() > 0) {
        confirmInput(data);
      } else {
        inputMode = INPUT_NONE;
        inputBuffer = "";
        alarmAcknowledge(data);
      }
      break;
    case '*':
      if (inputMode != INPUT_NONE && inputBuffer.length() > 0) {
        confirmInput(data);
      } else {
        handleStarKey(data);
      }
      break;
    default:
      break;
  }
}

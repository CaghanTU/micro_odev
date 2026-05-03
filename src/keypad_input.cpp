#include "keypad_input.h"

// Keypad.h defines a global enum value named IDLE. The project requirements
// also require a SystemState value named IDLE, so isolate the library token in
// this translation unit without changing the required firmware enum.
#define IDLE KEYPAD_LIBRARY_IDLE
#include <Keypad.h>
#undef IDLE

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

Keypad keypad = Keypad(makeKeymap(keymap), rowPins, colPins, KEYPAD_ROWS, KEYPAD_COLS);

enum InputMode {
  INPUT_NONE,
  INPUT_TEMPERATURE,
  INPUT_HUMIDITY
};

InputMode inputMode = INPUT_NONE;
String inputBuffer;

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

void toggleInputMode(SystemData &data) {
  if (inputMode == INPUT_TEMPERATURE) {
    beginInput(data, INPUT_HUMIDITY);
    return;
  }

  beginInput(data, INPUT_TEMPERATURE);
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
      loggerAdd(data, "Temp SP: " + String(value) + "C");
    }
  } else if (inputMode == INPUT_HUMIDITY) {
    if (value < static_cast<int>(MIN_HUMIDITY_SETPOINT_RH) ||
        value > static_cast<int>(MAX_HUMIDITY_SETPOINT_RH)) {
      loggerAdd(data, "Invalid humidity setpoint");
    } else {
      data.humiditySetpointRh = static_cast<float>(value);
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
  data.lastMessage = "Keypad ready";
}

void keypadInputUpdate(SystemData &data) {
  static unsigned long lastScanMs = 0;
  const unsigned long now = millis();

  if (now - lastScanMs < KEYPAD_SCAN_INTERVAL_MS) {
    return;
  }
  lastScanMs = now;

  const char key = keypad.getKey();
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
      toggleInputMode(data);
      break;
    default:
      break;
  }
}

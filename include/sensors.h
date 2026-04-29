#pragma once

#include <Arduino.h>
#include "system_types.h"

void sensorsInit(SystemData &data);
bool sensorsUpdate(SystemData &data);

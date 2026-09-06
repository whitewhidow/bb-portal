// A wall clock provided by the phone over BLE (no radio needed — WiFi and BLE can't
// run together on the no-PSRAM boards). The portal sends the current epoch on connect
// via __TIME__; the board keeps the offset and reports real time to any app that wants
// it (e.g. to timestamp records). Returns 0 until the phone has set it.
#pragma once
#include <Arduino.h>

void     clockSet(uint32_t epochSecs);   // called from the __TIME__ command
uint32_t clockNow();                      // epoch seconds now, or 0 if never set

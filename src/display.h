// Display abstraction. Real (LovyanGFX) on boards with a panel; all no-ops on a
// headless board (so a missing panel can't hang init). Colors are 24-bit RGB.
#pragma once
#include <stdint.h>

void dispBegin();
void dispSplash(const char* version, const char* board);        // boot splash
void dispCenter(const char* header, const char* body, uint32_t color);  // centered header + body
void dispStatus(bool ble, bool wifi, int batt);                 // bottom status; batt < 0 hides it
void dispOff();                                                 // backlight off (before deep sleep)
void dispOn();

// Tiny RGB-status-LED helper (WS2812/NeoPixel via the Arduino neopixelWrite()).
// A board declares APP_LED_PIN / APP_LED_COUNT in board.h; boards without an LED
// leave APP_LED_COUNT 0 and every call here is a safe no-op.
#pragma once
#include <stdint.h>

void ledInit();               // call once in setup()
bool ledHasLed();             // true if this board has an addressable LED
void ledFlash(uint32_t ms);   // start a non-blocking green flash for ms (drive with ledTick)
void ledTick();               // call every loop() — advances/ends the flash

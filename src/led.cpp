#include "led.h"
#include "board.h"
#include <Arduino.h>

bool ledHasLed() { return APP_LED_COUNT > 0; }

#if APP_LED_COUNT > 0
// neopixelWrite() drives a single WS2812 pixel (one RGB per call); on a strip it lights
// LED 0 — plenty for a status flash. Non-blocking: ledFlash() sets a deadline, ledTick()
// blinks green until then. A green flash confirms an event (e.g. a portal submission).
static uint32_t s_until = 0, s_lastToggle = 0;
static bool     s_on = false;
static inline void led(uint8_t r, uint8_t g, uint8_t b) {
#if defined(APP_BOARD_WAVESHARE_C5)
  neopixelWrite(APP_LED_PIN, g, r, b);   // this board's LED has R/G swapped vs neopixelWrite's WS2812 GRB order
#else
  neopixelWrite(APP_LED_PIN, r, g, b);
#endif
}

void ledInit()            { led(0, 0, 0); }        // off (also lazily inits the RMT channel)
void ledFlash(uint32_t ms){ s_until = millis() + ms; s_lastToggle = 0; s_on = false; }
void ledTick() {
  if (!s_until) return;
  uint32_t now = millis();
  if ((int32_t)(now - s_until) >= 0) { s_until = 0; s_on = false; led(0, 0, 0); return; }  // done -> off
  if (now - s_lastToggle >= 250) { s_lastToggle = now; s_on = !s_on; led(0, s_on ? 70 : 0, 0); }  // blink green
}
#else
void ledInit()             {}
void ledFlash(uint32_t)    {}
void ledTick()             {}
#endif

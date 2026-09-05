// ESP32 board-app template — boot/loop skeleton wiring the common pieces together.
// The reusable infra (display / BLE control / WiFi OTA / splash / status) is here;
// your feature code goes in app.cpp.
#include <Arduino.h>
#include "board.h"
#include "version.h"
#include "display.h"
#include "netota.h"
#include "ble_control.h"
#include "config.h"
#include "battery.h"
#include "app.h"
#include "led.h"

void setup() {
  Serial.begin(115200);
  dispBegin();

  // Reboot-to-fetch OTA: if the portal flagged an update, do it now — at a clean
  // heap, WiFi alone, before BLE/USB come up (safe on no-PSRAM boards). Reboots on
  // success; falls through to a normal boot on failure.
  netRunOtaAtBoot();

  // Boot splash — on unless turned off in config (Options tab). No-op on headless.
  bool showSplash = (cfgGet("splash", "1") != "0");
  if (showSplash) dispSplash(APP_VERSION, APP_BOARD_NAME);

  netBegin();                        // load saved WiFi creds (no auto-connect)
  bleBegin(APP_NAME);                // advertised BLE name (set APP_NAME in version.h)
  appSetup();
  ledInit();

#if APP_BTN >= 0
  pinMode(APP_BTN, INPUT_PULLUP);
#endif

  if (showSplash) delay(1500);       // only linger if we showed it
  dispCenter(APP_BOARD_NAME, (String("v") + APP_VERSION + "\nready").c_str(), 0x3FB950);
  dispStatus(bleConnected(), netConfigured(), batteryPct());   // WiFi badge = creds saved (config-only board)
}

void loop() {
  bleTick();                         // process portal commands + push status
  appLoop();                         // your app
  ledTick();                         // advance any LED flash

#if APP_HAS_DISPLAY
  // Screen sleep: backlight off after "sleepsecs" of idle (0 = never); a button press
  // wakes it (saves the backlight draw — the always-on SoftAP radio is the bigger heat).
  static uint32_t lastAct = 0; static bool asleep = false, actInit = false;
  if (!actInit) { actInit = true; lastAct = millis(); }
  int ss = cfgGet("sleepsecs", "0").toInt();
  if (ss > 0 && !asleep && millis() - lastAct > (uint32_t)ss * 1000) { dispOff(); asleep = true; }
#endif

#if APP_BTN >= 0
  // GPIO0/28 is the BOOT strap on most boards: ignore a press held from boot until it's
  // released once, so we don't misfire while the board is entering download mode.
  static bool ready = false, last = true;
  bool b = digitalRead(APP_BTN);
  if (!ready) { if (b == HIGH) ready = true; }
  else if (b == LOW && last == HIGH) {
#if APP_HAS_DISPLAY
    lastAct = millis();
    if (asleep) { dispOn(); asleep = false; dispStatus(bleConnected(), netConfigured(), batteryPct()); }
#endif
  }
  last = b;
#endif

  static uint32_t t = 0;
  if (millis() - t > 1000) { t = millis();
#if APP_HAS_DISPLAY
    if (!asleep) dispStatus(bleConnected(), netConfigured(), batteryPct());   // don't redraw (wake) while asleep
#else
    dispStatus(bleConnected(), netConfigured(), batteryPct());
#endif
  }
  delay(10);
}

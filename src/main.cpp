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

#if APP_BTN >= 0
  // GPIO0 is the BOOT strap on most boards: ignore a press held from boot until it's
  // released once, so we don't misfire while the board is entering download mode.
  static bool ready = false, last = true;
  bool b = digitalRead(APP_BTN);
  if (!ready) { if (b == HIGH) ready = true; }
  else if (b == LOW && last == HIGH) { /* TODO: your button click action */ }
  last = b;
#endif

  static uint32_t t = 0;
  if (millis() - t > 1000) { t = millis(); dispStatus(bleConnected(), netConfigured(), batteryPct()); }
  delay(10);
}

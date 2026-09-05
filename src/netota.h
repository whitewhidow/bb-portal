// WiFi station + in-app OTA self-update over WiFi (A/B partitions). Creds are
// provisioned over BLE and stored in NVS. "Reboot-to-fetch": the portal flags an
// update and reboots; the boot hook downloads it at a clean heap (WiFi alone, no
// BLE contention) and reboots into the new image.
#pragma once
#include <Arduino.h>

void        netBegin();                                   // load saved creds (no auto-connect)
void        netConnect();                                 // connect with the saved creds
void        netSetCreds(const String& ssid, const String& pass);
void        netClearCreds();
bool        netConnected();                               // live WiFi link (up only during OTA)
bool        netConfigured();                               // creds saved (ready to self-update)
String      netStatus();                                  // "wifi:<ssid>|<state>|<ip>|<ver>"
const char* netVersion();

void        netRequestOta();      // flag a self-update + reboot (call from a BLE command)
void        netRequestSwitch(int targetIdx);  // flag a switch to SWITCH_TARGETS[idx] + reboot
void        netRunOtaAtBoot();    // the boot hook — call EARLY in setup(), before BLE/USB
                                  // (handles both self-update and switch)

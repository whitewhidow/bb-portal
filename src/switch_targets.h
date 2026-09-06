// Firmware-switch targets: the OTHER apps' latest-release app-bin for THIS board.
// Board-aware switch mesh: T-Embed, T-Dongle and Cardputer are 3-way (BBoink + PoC),
// the T-Display C5 is 2-way (BBoink only — PoC has no C5 build). All share byte-compatible
// A/B partition tables. The 4MB Waveshare C5 is single-app, so it's off the mesh: the portal
// shows no switch options and __SWITCH__ is a clean no-op. See netota.cpp / ble_control.cpp.
#pragma once

struct SwitchTarget { const char* name; const char* url; };

#define GH_ "https://github.com/whitewhidow/"

#if defined(APP_BOARD_TEMBED)
static const SwitchTarget SWITCH_TARGETS[] = {
  { "BBoink", GH_ "bboink/releases/latest/download/bboink-app-t-embed-cc1101.bin" },
  { "PoC",    GH_ "hid-ble-poc/releases/latest/download/hid-ble-poc-app-tembed.bin" },
};
static const int SWITCH_TARGET_COUNT = (int)(sizeof(SWITCH_TARGETS) / sizeof(SWITCH_TARGETS[0]));
#elif defined(APP_BOARD_TDONGLE)
static const SwitchTarget SWITCH_TARGETS[] = {
  { "BBoink", GH_ "bboink/releases/latest/download/bboink-app-tdongle-s3.bin" },
  { "PoC",    GH_ "hid-ble-poc/releases/latest/download/hid-ble-poc-app-tdongle.bin" },
};
static const int SWITCH_TARGET_COUNT = (int)(sizeof(SWITCH_TARGETS) / sizeof(SWITCH_TARGETS[0]));
#elif defined(APP_BOARD_CARDPUTER)
static const SwitchTarget SWITCH_TARGETS[] = {
  { "BBoink", GH_ "bboink/releases/latest/download/bboink-app-cardputer-adv.bin" },
  { "PoC",    GH_ "hid-ble-poc/releases/latest/download/hid-ble-poc-app-cardputer.bin" },
};
static const int SWITCH_TARGET_COUNT = (int)(sizeof(SWITCH_TARGETS) / sizeof(SWITCH_TARGETS[0]));
#elif defined(APP_BOARD_TDISPLAY_C5)
// T-Display C5 (16MB A/B): 2-way with BBoink only — PoC has no C5 build. (The Waveshare
// C5 is 4MB single-app on both apps, so it can't A/B-switch and stays off the mesh.)
static const SwitchTarget SWITCH_TARGETS[] = {
  { "BBoink", GH_ "bboink/releases/latest/download/bboink-app-tdisplay-c5.bin" },
};
static const int SWITCH_TARGET_COUNT = (int)(sizeof(SWITCH_TARGETS) / sizeof(SWITCH_TARGETS[0]));
#else
static const SwitchTarget SWITCH_TARGETS[1] = { { "", "" } };   // dummy; not in the mesh here
static const int SWITCH_TARGET_COUNT = 0;
#endif

#undef GH_

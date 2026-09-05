// Firmware-switch targets: the OTHER apps' latest-release app-bin for THIS board.
// Board-aware — only the S3 16MB boards (T-Embed, T-Dongle) are in the switch mesh
// (identical A/B partition table + PoC has no C5 build). Empty elsewhere -> the portal
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
#else
static const SwitchTarget SWITCH_TARGETS[1] = { { "", "" } };   // dummy; not in the mesh here
static const int SWITCH_TARGET_COUNT = 0;
#endif

#undef GH_

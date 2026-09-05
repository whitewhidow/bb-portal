// Firmware version + self-update source. Bump APP_VERSION on each release and tag it
// (git tag vX.Y.Z); CI publishes the matching app .bin to the GitHub release that
// APP_OTA_URL ("latest") points at, and the board pulls it over WiFi.
#pragma once

// A gitignored dev_secrets.h can override APP_OTA_URL to a self-hosted test bin (no
// CI build / no billing while iterating). Pulled here so every unit sees it.
#if defined(__has_include)
#  if __has_include("dev_secrets.h")
#    include "dev_secrets.h"
#  endif
#endif

#define APP_VERSION "0.1.5"

// Your app's name — the BLE advertised name + shown on the display. The portal /
// flasher / landing pages carry their own APP_NAME const (top of each HTML) since
// they're served standalone; keep them in sync. This is the one firmware-side spot.
#define APP_NAME "Terms Portal"

// Splash subtitle / tagline (shown under the title on the boot splash).
#define APP_TAGLINE "Wi-Fi terms & access code"

// Change these two to your GitHub org/repo. Each board pulls its OWN asset (different
// pins/panel -> different binary), named after the env: <repo>-app-<env>.bin.
#define APP_GH_OWNER "whitewhidow"
#define APP_GH_REPO  "bb-portal"

#if defined(APP_BOARD_TEMBED)
#  define APP_OTA_ENV "tembed-cc1101"
#elif defined(APP_BOARD_TDONGLE)
#  define APP_OTA_ENV "tdongle-s3"
#elif defined(APP_BOARD_CARDPUTER)
#  define APP_OTA_ENV "cardputer"
#elif defined(APP_BOARD_TDISPLAY_C5)
#  define APP_OTA_ENV "tdisplay-c5"
#elif defined(APP_BOARD_WAVESHARE_C5)
#  define APP_OTA_ENV "waveshare-c5"
#else
#  define APP_OTA_ENV "s3-headless"
#endif

#if !defined(APP_OTA_URL)
#  define APP_OTA_URL \
     "https://github.com/" APP_GH_OWNER "/" APP_GH_REPO \
     "/releases/latest/download/" APP_GH_REPO "-app-" APP_OTA_ENV ".bin"
#endif

// Companion web page (Web-Bluetooth portal on GitHub Pages) — shown on the idle
// screen so you know where to connect. Derived from owner/repo (Pages serves the
// repo root, so the portal lives under /portal/); override in dev_secrets.h if you
// host it elsewhere.
#if !defined(APP_PAGE_URL)
#  define APP_PAGE_URL "https://" APP_GH_OWNER ".github.io/" APP_GH_REPO "/portal/"
#endif

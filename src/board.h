// Per-board hardware map — pins, panel, button, name — selected by the APP_BOARD_*
// flag from platformio.ini. Panel configs are the raw LovyanGFX values used across
// our two existing apps. Add a board: add an env in platformio.ini + a block here.
#pragma once

// A firmware-specific tweak byte for the BLE base MAC (see ble_control.cpp). If two
// DIFFERENT firmwares run on the SAME board (e.g. an OTA "switch"), give each a
// different tag so the host doesn't serve a stale GATT cache. One app: any fixed value.
// Switch-mesh tags in use: template 0x00, PoC 0x70, bboink 0xB0, Terms Portal 0xE0.
#define APP_BLE_MAC_TAG 0xE0

// ==== ESP32-S3 boards WITH a display =============================================
#if defined(APP_BOARD_TEMBED)
  #define APP_BOARD_NAME "T-Embed CC1101 / Plus"
  #define APP_HAS_DISPLAY 1
  #define PANEL_ST7789
  #define PANEL_SPI_HOST SPI2_HOST
  #define PIN_SCLK 11
  #define PIN_MOSI 9
  #define PIN_MISO 10
  #define PIN_CS   41
  #define PIN_DC   16
  #define PIN_RST  40
  #define PIN_BL   21
  #define PANEL_W  170
  #define PANEL_H  320
  #define OFFX     35
  #define OFFY     0
  #define PANEL_FREQ 40000000
  #define APP_BTN  0
  #define APP_BOARD_PWR_EN 15         // drive HIGH to power the LCD rail
  #define APP_ROTATION 3              // T-Embed panel is mounted flipped 180 (matches PoC/bboink)
  #define APP_LED_PIN 14
  #define APP_LED_COUNT 8

#elif defined(APP_BOARD_TDONGLE)
  #define APP_BOARD_NAME "T-Dongle S3"
  #define APP_HAS_DISPLAY 1
  #define PANEL_ST7735
  #define PANEL_SPI_HOST SPI2_HOST
  #define PIN_SCLK 5
  #define PIN_MOSI 3
  #define PIN_MISO -1
  #define PIN_CS   4
  #define PIN_DC   2
  #define PIN_RST  1
  #define PIN_BL   38
  #define PANEL_W  80
  #define PANEL_H  160
  #define OFFX     26
  #define OFFY     1
  #define PANEL_FREQ 27000000
  #define APP_BTN  0

#elif defined(APP_BOARD_CARDPUTER)
  // M5Cardputer ADV — StampS3. NOTE: platformio env MUST use board=m5stack-stamps3
  // (esp32-s3-devkitc-1 bootloops on a StampS3). Panel on SPI3_HOST.
  #define APP_BOARD_NAME "Cardputer ADV"
  #define APP_HAS_DISPLAY 1
  #define PANEL_ST7789
  #define PANEL_SPI_HOST SPI3_HOST
  #define PIN_SCLK 36
  #define PIN_MOSI 35
  #define PIN_MISO -1
  #define PIN_CS   37
  #define PIN_DC   34
  #define PIN_RST  33
  #define PIN_BL   38
  #define PANEL_W  135
  #define PANEL_H  240
  #define OFFX     52
  #define OFFY     40
  #define PANEL_FREQ 40000000
  #define APP_BTN  0
  #define APP_BATT_ADC 10          // Cardputer battery voltage on GPIO10 (M5's BAT_ADC)...
  #define APP_BATT_DIV 2.0f        // ...through a 1:1 divider (x2). VERIFIED on hardware.
  #define APP_LED_PIN 21             // WS2812 x1 — VERIFIED on hardware (green, no R/G swap)
  #define APP_LED_COUNT 1

// ==== ESP32-C5 boards — ST7789 over SPI2, same LovyanGFX path as the S3 boards ======
#elif defined(APP_BOARD_TDISPLAY_C5)
  #define APP_BOARD_NAME "T-Display C5"
  #define APP_HAS_DISPLAY 1
  #define PANEL_SPI_HOST SPI2_HOST
  #define PIN_SCLK 7
  #define PIN_MOSI 9
  #define PIN_MISO -1
  #define PIN_CS   26
  #define PIN_DC   8
  #define PIN_RST  23
  #define PIN_BL   25            // also the panel power rail — Light_PWM drives it high
  #define PANEL_W  170
  #define PANEL_H  320
  #define OFFX     35
  #define OFFY     0
  #define PANEL_FREQ 40000000
  #define APP_BTN  0              // BOOT button (GPIO0)

#elif defined(APP_BOARD_WAVESHARE_C5)
  #define APP_BOARD_NAME "Waveshare C5"
  #define APP_HAS_DISPLAY 1
  #define PANEL_SPI_HOST SPI2_HOST
  #define PIN_SCLK 7
  #define PIN_MOSI 6
  #define PIN_MISO -1
  #define PIN_CS   23
  #define PIN_DC   24
  #define PIN_RST  26
  #define PIN_BL   10
  #define PANEL_W  172
  #define PANEL_H  320
  #define OFFX     34
  #define OFFY     0
  #define PANEL_FREQ 40000000
  #define APP_BTN  28             // BOOT button (GPIO28, C5 boot strap — bboink-verified)
  #define APP_LED_PIN 8               // WS2812 x1 — neopixelWrite()
  #define APP_LED_COUNT 1

// ==== Generic / headless =========================================================
#elif defined(APP_BOARD_HEADLESS)
  #define APP_BOARD_NAME "Headless S3"
  #define APP_HAS_DISPLAY 0           // null display — the BLE portal is the whole UI
  #define APP_BTN  -1

#else
  #error "define an APP_BOARD_* in platformio.ini (see board.h for the options)"
#endif

// Display rotation (LovyanGFX setRotation). Default landscape; a board overrides it
// above when its panel is mounted differently (e.g. the T-Embed is flipped 180).
#ifndef APP_ROTATION
#define APP_ROTATION 1
#endif

// RGB status LED (WS2812/NeoPixel via neopixelWrite). Boards without one leave COUNT 0.
#ifndef APP_LED_COUNT
#define APP_LED_COUNT 0
#endif
#ifndef APP_LED_PIN
#define APP_LED_PIN -1
#endif

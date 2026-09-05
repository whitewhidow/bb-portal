# esp32-board-app-template

A starting point for an **ESP32 board app**: one codebase, many boards, controlled from
a phone over **BLE** and updated over **WiFi**. It gives you the boring-but-fiddly parts
so a new app is mostly `app.cpp`.

## What you get

- **Multi-board HAL** (`board.h`) — display pins/panel, button, name per board, selected
  by a build flag. Boards included: T-Embed CC1101, T-Dongle S3, Cardputer ADV (StampS3),
  T-Display C5, Waveshare C5-LCD, and a **generic headless S3**.
- **Display** (`display.cpp`) — LovyanGFX with a **null path** for headless boards
  (a missing panel would otherwise hang init). Splash + centered text + status bar.
- **BLE control service** (`ble_control.cpp`) — the phone portal writes `__CMD__`s, the
  board notifies replies. Built-in: version, WiFi provisioning, self-update, status,
  config. Your commands via `appHandleCommand()`.
- **WiFi OTA** (`netota.cpp`) — reboot-to-fetch self-update into an A/B slot (works even
  on headless / single-button boards).
- **Config** (`config.cpp`) — NVS settings; declare fields once, the portal auto-renders
  the form. Includes the **boot-splash on/off** toggle.
- **Portal** (`portal/index.html`) — Web-Bluetooth control page (status header with a
  version pill checked against the latest GitHub release, WiFi/Update/Config tabs).
- **Web flasher** (`flasher/`) — ESP Web Tools browser USB flash of the **merged** image.
- **CI** (`.github/workflows/release.yml`) — on a tag, builds every board, publishes
  app-only + merged bins, and auto-updates the flasher.
- **[GOTCHAS.md](GOTCHAS.md)** — the expensive lessons (MAC/GATT cache, notify race,
  StampS3 board setting, buttonless flashing, app-only vs merged bins, …).

## Boards

| env | board | chip | display | notes |
|-----|-------|------|---------|-------|
| `s3-headless`  | Generic ESP32-S3 (8MB) | S3 | none | BLE-only; the simplest start |
| `tembed`       | LilyGo T-Embed CC1101  | S3 | ST7789 320×170 | |
| `tdongle`      | LilyGo T-Dongle S3     | S3 | ST7735S 80×160 | |
| `cardputer`    | M5Cardputer ADV        | S3 | ST7789 135×240 | `board = m5stack-stamps3` (required) |
| `tdisplay-c5`  | LilyGo T-Display C5    | C5 | (headless for now) | pins in board.h |
| `waveshare-c5` | Waveshare C5-LCD-1.47  | C5 | (headless for now) | pins in board.h |

## Use it

1. **Use this template** on GitHub → your repo. Then find-and-replace
   `esp32-board-app-template` / `whitewhidow` in `version.h`, `release.yml`, the portal,
   and the flasher with your repo/owner. Set Pages to serve from **`/` (root)**.
2. **Name your app**: set `APP_NAME` in `version.h` (the BLE advertised name + on-screen
   title). Each web page carries its own `const APP_NAME` at the top of its script
   (`portal/`, `flasher/`, root `index.html`) that drives its title/heading — keep those in
   sync with `version.h`.
3. Build/flash: `pio run -e s3-headless -t upload` (add `-e <board>` for others).
4. Write your app in **`app.cpp`** (`appSetup`/`appLoop` + `appHandleCommand`) and add
   settings in **`config.cpp`** (`CFG_FIELDS`).
5. Release: `git tag v0.1.0 && git push --tags` — CI publishes bins + the flasher.

The demo app just echoes text from the portal onto the board's screen, to show the wiring.

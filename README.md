# bb-portal — Wi-Fi Terms & Access-Code Portal

A **captive portal** for building or guest Wi-Fi on an ESP32. Visitors join the
board's open Wi-Fi, land on a **terms / sign-up page** you control, and — once they
sign — get an **access code** on a thank-you page. Staff edit both pages and the
branding from a phone over Bluetooth (no app to install); every sign-up is saved on
the board and exportable as a **CSV**.

Runs on one codebase across S3 **and** C5 boards — the C5 captive portal is verified
working on hardware (T-Display C5 + Waveshare C5-LCD), so the old "S3-only" caveat no
longer applies. Built on the
[esp32-board-app-template](https://github.com/whitewhidow/esp32-board-app-template)
boilerplate (BLE control + Wi-Fi OTA + multi-board HAL).

> **Handling people's data:** sign-ups (including anything visitors type) are stored in
> cleartext CSV on the board and exportable over BLE. You are responsible for obtaining
> consent and for retention under your local law (e.g. GDPR). Don't collect more than you
> need, and clear the records when you're done.

## Boards

| env | board | chip | display | notes |
|-----|-------|------|---------|-------|
| `s3-headless`  | Generic ESP32-S3 (8MB) | S3 | none | BLE-only; the simplest start |
| `tembed-cc1101` | LilyGo T-Embed CC1101 | S3 | ST7789 320×170 | |
| `tdongle-s3`    | LilyGo T-Dongle S3    | S3 | ST7735S 80×160 | |
| `cardputer`    | M5Cardputer ADV        | S3 | ST7789 135×240 | `board = m5stack-stamps3` (required) |
| `tdisplay-c5`  | LilyGo T-Display C5   | C5 | ST7789 320×170 | |
| `waveshare-c5` | Waveshare C5-LCD-1.47 | C5 | ST7789 320×172 | |

## Deploy it

1. **Flash a board** — the [web flasher](https://whitewhidow.github.io/bb-portal/flasher/) (browser
   USB, no toolchain), or `pio run -e <board> -t upload` (`tembed-cc1101`, `tdongle-s3`, `cardputer`,
   `tdisplay-c5`, `waveshare-c5`, `s3-headless`). All work; the C5s are verified.
2. **Open the BLE console** — [whitewhidow.github.io/bb-portal/portal/](https://whitewhidow.github.io/bb-portal/portal/)
   (Chrome/Edge), connect to **Terms Portal** (it opens on the **Records** tab):
   - **Records** — view, CSV-export, or clear submissions. Each is timestamped with real local
     time (the portal sets the board's clock over BLE on connect); old records auto-rotate so a
     full flash can't stop sign-ups.
   - **Editor** — customise the terms/form page + the thank-you/code page HTML (Reset-to-default
     restores the starter), or **Backup both pages / Restore** them as `bb-portal-pages.json`.
     Placeholders are injected on every page load.
   - **Config** — building SSID, the access **code** (`{{CODE}}`), the 4 generic slots
     (`{{VALUE1}}`–`{{VALUE4}}`), brightness, **Screen off after (s)**, **LED flash** (on LED
     boards), **Export all / Import** of the whole board config as a JSON file (with an option to
     leave the Wi-Fi password out), and named **config profiles** (saved in the browser, each
     snapshotting the config + both pages) for switching between setups.
   - **WiFi / Update** — Wi-Fi creds for OTA, and self-update / firmware switch.
3. **Residents** join the board's open SoftAP → the captive portal shows the terms page → they
   sign → get the access code.
4. **Updates** — WiFi tab (creds for OTA) → Update. Boards with A/B slots can also **switch**
   to a sibling firmware over OTA: **T-Embed CC1101 / T-Dongle S3 / Cardputer ADV** hop to
   [BBoink](https://github.com/whitewhidow/bboink) *and* [hid-ble-poc](https://github.com/whitewhidow/hid-ble-poc);
   the **T-Display C5** hops to BBoink only (the PoC has no C5 build). The 4 MB Waveshare C5 is
   single-app and stays on bb-portal.

## Replace a board (adopt)

Swapping an old board for a new one? The **Replace** tab lets the new board *take over the old
one's identity*, so every device currently on the old board's Wi-Fi auto-rejoins the new one — no
one has to reconnect by hand. It works because clients auto-rejoin an **open** network by its
**SSID** (and, for a truly invisible swap, its **BSSID/MAC** and **channel**).

1. Open **Replace** — it shows *this* board's live identity (`SSID · MAC · channel`).
2. **Scan for boards** — lists nearby **2.4 GHz open** networks (SSID · BSSID · channel · RSSI).
   (The C5 also scans 5 GHz; those are filtered out — the captive AP is 2.4 GHz.)
3. **Pick the old board**, then **power the old board off** and tick the box.
4. **Adopt** — the new board copies the old board's **SSID + channel + AP MAC** and **reboots** as
   a drop-in replacement. Reopen **Replace** afterwards to confirm the identity took.

You can also set these **by hand** in **Config**: `ssid`, `channel` (1–13), and `apmac`
(`AA:BB:CC:DD:EE:FF`; blank = the board's own MAC). SSID and channel apply live on Save; an **AP
MAC change needs a reboot** (Save prompts you), because the MAC can only be set before the SoftAP
starts. **Don't run two boards with the same MAC + channel at once** — power the old one off first.

## Under the hood

Built on the [boilerplate](https://github.com/whitewhidow/esp32-board-app-template), so the infra
below is shared and `app.cpp` / `config.cpp` extend it the same way.

- **Multi-board HAL** (`board.h`) — display pins/panel, button, name per board, selected
  by a build flag. Boards included: T-Embed CC1101, T-Dongle S3, Cardputer ADV (StampS3),
  T-Display C5, Waveshare C5-LCD, and a **generic headless S3**.
- **Display** (`display.cpp`) — LovyanGFX with a **null path** for headless boards
  (a missing panel would otherwise hang init). Splash + centered text + status bar.
- **BLE control service** (`ble_control.cpp`) — the phone portal writes `__CMD__`s, the
  board notifies replies. Built-in: version, WiFi provisioning, self-update, status,
  config (served in MTU-safe chunks so it doesn't silently drop on the C5). Your commands
  via `appHandleCommand()`.
- **WiFi OTA** (`netota.cpp`) — reboot-to-fetch self-update into an A/B slot (works even
  on headless / single-button boards).
- **Config** (`config.cpp`) — NVS settings; declare fields once, the portal auto-renders
  the form. Includes the **boot-splash on/off** toggle.
- **Portal** (`portal/index.html`) — Web-Bluetooth control page: a status header with a
  version pill (checked against the latest GitHub release) plus live board badges
  (uptime, BLE RSSI, battery %, free heap, free flash, and Wi-Fi client count), and
  **Records / Editor / Config / Replace / WiFi / Update**
  tabs (opens on Records). Writes are serialized through a send-queue so overlapping BLE
  ops don't collide. A **Board-feedback** log echoes every action (`→`) and reply (`←`),
  filtering out background polls / chunk streams / status so only what you did shows.
- **Replace a board** (`Replace` tab) — swap an old board for a new one and keep everyone
  connected; see below.
- **Web flasher** (`flasher/`) — ESP Web Tools browser USB flash of the **merged** image.
- **CI** (`.github/workflows/release.yml`) — on a tag, builds every board, publishes
  app-only + merged bins, and auto-updates the flasher.
- **[GOTCHAS.md](GOTCHAS.md)** — the expensive lessons (MAC/GATT cache, notify race,
  StampS3 board setting, buttonless flashing, app-only vs merged bins, …).

## Getting help

Questions or a bug? **Open a GitHub issue** — <https://github.com/whitewhidow/bb-portal/issues>.
Include your board, firmware version (the pill in the portal header), and what you tried.

# GOTCHAS

Hard-won lessons baked into this template. Read before you fight them again.

## BLE

- **Distinct MAC per firmware.** If two *different* firmwares can run on the *same*
  board (e.g. an OTA "switch"), the host caches GATT/pairing by MAC — writes then
  silently no-op on the second firmware. Fix (in `bleBegin`): derive the base MAC from
  the chip MAC XOR a per-firmware tag before NimBLE init. Chrome "forget" does **not**
  clear this; on Linux the real cache is BlueZ (`bluetoothctl remove <MAC>`).
- **`notify()` has no flush.** Two rapid `setValue()+notify()` calls **race** — the 2nd
  clobbers the 1st. Send **one** notification per response; pack fields with a separator
  (`ver:1.0.0|Board Name`), never two calls.
- **Control writes are open** (`WRITE | WRITE_NR`, no encryption) so a command lands even
  if bonding is stale. The portal auto-reloads after an OTA so it reconnects on fresh GATT.
- WiFi is **configure-only** in the portal — bringing WiFi up while BLE runs churns the
  radio on no-PSRAM boards. The reboot-to-fetch OTA connects WiFi itself, alone, at boot.
- **A notify bigger than the negotiated MTU silently fails on the C5.** The config JSON can
  exceed it, so it's served in small **chunks** (`__CFGGET__:<off>` → `cfg:<end>:<total>:<part>`),
  reassembled by the portal — same trick as the page/doc loader. A single big notify just
  vanishes with no error.
- **Web Bluetooth runs one GATT op at a time.** Overlapping `writeValue()` calls (e.g. the
  switch-list loading while records/config are still fetching) reject instantly with *"GATT
  operation already in progress"*. The portal funnels every write through a **send-queue**.

## Boards / build

- **StampS3 (Cardputer) needs `board = m5stack-stamps3`.** `esp32-s3-devkitc-1` bootloops
  on it before `setup()`.
- **Buttonless boards + USB-HID.** `ARDUINO_USB_MODE=0` (TinyUSB HID) means no serial and
  no auto-reset → you can't re-enter download without a BOOT button. On a buttonless board
  keep `USB_MODE=1` (hardware CDC/JTAG) so esptool can auto-reset (this template's default).
- **Headless display must be a no-op**, not just skipped — initializing a panel that isn't
  wired **hangs** boot. See the `#if !APP_HAS_DISPLAY` path in `display.cpp`.
- **ESP32-C5** flashes with the bootloader at `0x2000` (not `0x0`), needs esptool ≥ 5, and
  its ROM often rejects the stub loader → `upload_flags = --no-stub`. The pioarduino C5
  build also hits a cold-cache `FRAMEWORK_DIR=None` transient — the CI retries 3×.
- **4 MB can't fit two OTA slots.** The 4 MB C5 (Waveshare) ships a **single app slot — no A/B
  OTA**, so it can't self-update or firmware-switch; update it by USB/web-flasher reflash. The
  16 MB boards get real A/B slots.

## OTA / bins

- The **app-only** bin (`<repo>-app-<env>.bin`, published for OTA) is *not* flashable to a
  blank board — it's only the app partition. A blank board needs the **merged** bin
  (`<repo>-<env>.bin`: bootloader + partitions + app at `0x0`/`0x2000`), which the web
  flasher uses. Don't hand someone the app bin for a first flash.
- **Rollback is already on** in the stock Arduino SDK (`CONFIG_APP_ROLLBACK_ENABLE` + a
  9 s bootloader watchdog), and `initArduino()` auto-marks a booted app valid. So an OTA'd
  app that crashes early auto-reverts to the previous slot.
- **OTA URL uses `releases/latest`**, so the OTA can't resolve until you've cut a real
  release. A gitignored `src/dev_secrets.h` can define `APP_OTA_URL` to a self-hosted test
  bin while iterating (no CI build).

## Replace a board (adopt / clone identity)

- **Auto-rejoin is by SSID, not by box.** Clients auto-rejoin an **open** network by its **SSID**,
  so a new board with the same SSID picks up everyone who was on the old one — no per-client state
  to migrate. For a truly invisible swap, also match the **channel** and clone the **AP MAC (BSSID)**.
- **An AP MAC change only applies at boot.** `esp_wifi_set_mac(WIFI_IF_AP, …)` must run **before**
  `WiFi.softAP()`; a live re-apply on a running AP fails (`ESP_ERR_WIFI_MODE`). So `__ADOPT__` and a
  manual `apmac` edit both take effect via a **reboot** (`captiveBegin` applies MAC → channel → SSID).
- **Never run two boards with the same MAC + channel at once** — it confuses clients. The adopt flow
  forces you to confirm the old board is **off** first. During an unavoidable overlap, keep the same
  SSID but a *different* BSSID (still auto-rejoins, just less seamless), then drop the old one.
- **C5 scans 5 GHz too.** `__APSCAN__` filters to **2.4 GHz** (`channel ≤ 13`) and open networks —
  the captive AP is 2.4 GHz, and `channel` is clamped to 1–13 (5 GHz values fall back to the default).
- **Verify it took** via `__APINFO__` (the Replace tab's "this board: SSID · MAC · ch" line) — the
  live values, so you can compare before/after a clone.

## Process

- **Never tag / cut a release without an explicit go.** Committing + pushing to `main`
  (incl. Pages deploys) is fine; `git tag vX.Y.Z` triggers the release build + flasher
  publish. Test first, tag on purpose.

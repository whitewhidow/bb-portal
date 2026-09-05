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
- **4 MB can't fit two 3 MB OTA slots.** The C5 4 MB partition uses 1.75 MB slots.

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

## Process

- **Never tag / cut a release without an explicit go.** Committing + pushing to `main`
  (incl. Pages deploys) is fine; `git tag vX.Y.Z` triggers the release build + flasher
  publish. Test first, tag on purpose.

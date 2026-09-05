// See ble_control.h.
#include "ble_control.h"
#include "board.h"
#include "version.h"
#include "netota.h"
#include "config.h"
#include "battery.h"
#include "switch_targets.h"
#include <NimBLEDevice.h>
#include <WiFi.h>
#include <esp_mac.h>

// Your own UUIDs (change the base if you want). RX = phone->board, TX = board->phone.
#define CTRL_SVC "a0b00000-1234-4b0a-9c5e-000000000000"
#define CTRL_RX  "a0b00001-1234-4b0a-9c5e-000000000000"
#define CTRL_TX  "a0b00002-1234-4b0a-9c5e-000000000000"

static NimBLECharacteristic* s_tx = nullptr;
static char          g_cmd[320] = "";
static volatile bool g_cmdReq   = false;
static bool          g_connected = false;
static char          g_mac[18]  = "";

// GOTCHA: setValue()+notify() has no flush, so two rapid notifies RACE (the 2nd
// clobbers the 1st). Always send ONE line per response; pack multiple fields with a
// separator (e.g. "ver:1.0.0|Board Name") rather than two notify() calls.
void bleNotify(const char* line) {
  if (s_tx) { s_tx->setValue((uint8_t*)line, strlen(line)); s_tx->notify(); }
  Serial.printf("[BLE tx %d] %.48s\n", (int)strlen(line), line);
}
bool bleConnected() { return g_connected; }
const char* bleMac() { return g_mac; }

class RxCB : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo&) override {
    NimBLEAttValue v = c->getValue();
    if (v.length() && v.length() < sizeof(g_cmd) && !g_cmdReq) {
      memcpy(g_cmd, v.data(), v.length()); g_cmd[v.length()] = 0; g_cmdReq = true;
    }
  }
};
class SrvCB : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* s, NimBLEConnInfo&) override { g_connected = true; }
  void onDisconnect(NimBLEServer* s, NimBLEConnInfo&, int) override { g_connected = false; NimBLEDevice::startAdvertising(); }
};

void bleBegin(const char* advName) {
  // GOTCHA: give this firmware a distinct BLE MAC (base ^ tag) so that if you ever
  // run a DIFFERENT firmware on the same board, the host doesn't serve a stale GATT
  // cache (writes silently no-op). Must run BEFORE NimBLE init.
  uint8_t mac[6];
  if (esp_efuse_mac_get_default(mac) == ESP_OK) { mac[5] ^= APP_BLE_MAC_TAG; esp_base_mac_addr_set(mac); }

  NimBLEDevice::init(advName);
  NimBLEDevice::setMTU(517);                                 // allow large notifies (config JSON, doc chunks)
  NimBLEDevice::setSecurityAuth(true, false, true);         // bond, no MITM, SC
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(new SrvCB());
  NimBLEService* svc = server->createService(CTRL_SVC);
  NimBLECharacteristic* rx = svc->createCharacteristic(CTRL_RX, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  rx->setCallbacks(new RxCB());
  s_tx = svc->createCharacteristic(CTRL_TX, NIMBLE_PROPERTY::NOTIFY);
  svc->start();
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  // GOTCHA: a 128-bit service UUID (18 bytes) + the name won't both fit the 31-byte
  // primary adv packet ("Data length exceeded" -> name silently dropped). Keep the UUID
  // in the primary (the portal filters by it) and move the NAME into the scan response.
  adv->addServiceUUID(CTRL_SVC);
  NimBLEAdvertisementData scanResp;
  scanResp.setName(advName);
  adv->setScanResponseData(scanResp);
  adv->enableScanResponse(true);
  NimBLEDevice::startAdvertising();
  strncpy(g_mac, NimBLEDevice::getAddress().toString().c_str(), sizeof(g_mac) - 1);
}

// Built-in commands, then the app's. Returns after handling.
static void handleCmd(const char* cmd) {
  Serial.printf("[BLE rx] %.48s\n", cmd);
  if (!strcmp(cmd, "__VER__")) {
    bleNotify((String("ver:") + APP_VERSION + "|" + APP_BOARD_NAME).c_str());   // ONE notify
  } else if (!strcmp(cmd, "__STATUS__")) {
    char b[48]; snprintf(b, sizeof(b), "st:ble=1:wifi=%d:batt=%d", netConnected()?1:0, batteryPct()); bleNotify(b);
  } else if (!strcmp(cmd, "__WIFIST__")) {
    bleNotify(netStatus().c_str());
  } else if (!strncmp(cmd, "__WIFI__:", 9)) {                 // "__WIFI__:ssid|pass"
    const char* a = cmd + 9; const char* bar = strchr(a, '|');
    if (bar) { netSetCreds(String(a).substring(0, bar - a), bar + 1); bleNotify("wifi:saved"); }
  } else if (!strcmp(cmd, "__WIFICLR__")) {
    netClearCreds(); bleNotify("wifi:cleared");
  } else if (!strcmp(cmd, "__OTA__")) {                        // reboot-to-fetch self-update
    bleNotify("ota:0 rebooting to update — watch the board"); delay(400); netRequestOta();
  } else if (!strcmp(cmd, "__SWITCHLIST__")) {                 // sibling firmwares for this board
    String s = "sw:";
    for (int i = 0; i < SWITCH_TARGET_COUNT; i++) { if (i) s += "|"; s += SWITCH_TARGETS[i].name; }
    bleNotify(s.c_str());
  } else if (!strncmp(cmd, "__SWITCH__:", 11)) {               // "__SWITCH__:<idx>" -> other firmware
    int idx = atoi(cmd + 11);
    if (idx >= 0 && idx < SWITCH_TARGET_COUNT) {
      bleNotify((String("ota:0 switching to ") + SWITCH_TARGETS[idx].name + " — watch the board").c_str());
      delay(400); netRequestSwitch(idx);
    } else bleNotify("err:no such switch target");
  } else if (!strcmp(cmd, "__CFGGET__")) {                     // config schema + values
    bleNotify(cfgJson().c_str());
  } else if (!strncmp(cmd, "__CFGSET__:", 11)) {               // "__CFGSET__:key=value"
    const char* a = cmd + 11; const char* eq = strchr(a, '=');
    if (eq) { cfgSet(String(a).substring(0, eq - a).c_str(), eq + 1); bleNotify("cfg:ok"); }
  } else {
    if (!appHandleCommand(cmd)) bleNotify((String("err:unknown ") + cmd).c_str());
  }
}

void bleTick() {
  // GOTCHA: bleNotify has no flush, so two notifies in one tick RACE (2nd clobbers 1st).
  // If we just answered a command, skip the status push THIS tick so its reply survives.
  if (g_cmdReq) { g_cmdReq = false; handleCmd(g_cmd); return; }
  // push status to the portal when it changes, plus a slow refresh so battery drifts up
  static int8_t lastW = -1, lastB = -1; static uint32_t lastPush = 0;
  if (g_connected) {
    int8_t w = netConnected() ? 1 : 0, b = batteryPct();
    if (w != lastW || b/5 != lastB/5 || millis() - lastPush > 30000) {
      lastW = w; lastB = b; lastPush = millis();
      char m[48]; snprintf(m, sizeof(m), "st:ble=1:wifi=%d:batt=%d", w, b); bleNotify(m);
    }
  } else { lastW = -1; lastB = -1; }
}

// >>> APP: Terms Portal <<<
// Runs the captive portal (open SoftAP + DNS + editable terms/form flow) alongside
// the template's BLE control service. The portal HTML ("portal"/"done" docs) is
// edited over BLE from the web portal; submitted records are pulled the same way.
#include "app.h"
#include "captive.h"
#include "config.h"
#include "ble_control.h"
#include "display.h"
#include "version.h"
#include <Arduino.h>
#include <WiFi.h>          // scan nearby APs for the "Replace a board" adopt flow
#include "mbedtls/base64.h"

#define RAW_CHUNK 120     // raw bytes per doc chunk (base64 -> ~160 chars, fits BLE + the 320B RX buffer)

// --------------------------- base64 helpers --------------------------------
static String b64enc(const uint8_t* data, size_t len) {
  size_t olen = 0;
  unsigned char out[4 * ((RAW_CHUNK + 2) / 3) + 4];
  if (mbedtls_base64_encode(out, sizeof(out), &olen, data, len) != 0) return "";
  return String((char*)out).substring(0, olen);
}
static size_t b64dec(const char* b64, uint8_t* out, size_t maxout) {
  size_t olen = 0;
  if (mbedtls_base64_decode(out, maxout, &olen, (const unsigned char*)b64, strlen(b64)) != 0) return 0;
  return olen;
}

// ------------------------------ display ------------------------------------
static uint32_t s_lastShown = 0xFFFFFFFF;
static void showStatus() {
  uint32_t n = captiveRecordCount();
  String body = cfgGet("ssid", "Building-WiFi") + "\n" + String(n) + " records"
                "\nopen on phone:\n" APP_PAGE_URL;
  dispCenter("TERMS PORTAL", body.c_str(), 0x3FB950);
  s_lastShown = n;
}

void appSetup() {
  captiveBegin();
  showStatus();
}

void appLoop() {
  captiveTick();
  static bool first = true;
  if (first) { first = false; showStatus(); }   // own the screen right after boot (over main's "ready")
  static uint32_t t = 0;
  if (millis() - t > 1000) {
    t = millis();
    if (captiveRecordCount() != s_lastShown) showStatus();
  }
}

// ---- BLE commands from the web portal ----
// Doc editing:  __HGET__:<doc>:<off> · __HPUT__:<doc> · __HADD__:<b64> · __HEND__
// Records:      __RECN__ · __RECCLR__   (docs: portal | done | records)
static size_t s_upTotal = 0;

bool appHandleCommand(const char* cmd) {
  if (!strncmp(cmd, "__HGET__:", 9)) {
    const char* a = cmd + 9;
    const char* colon = strchr(a, ':');
    if (!colon) { bleNotify("hget:err"); return true; }
    String doc = String(a).substring(0, colon - a);
    size_t off = strtoul(colon + 1, nullptr, 10);
    uint8_t raw[RAW_CHUNK];
    int got = captiveDocRead(doc, off, raw, RAW_CHUNK);
    size_t total = captiveDocSize(doc);
    String line = "hget:" + String(off + (got > 0 ? got : 0)) + ":" + String(total) + ":";
    if (got > 0) line += b64enc(raw, got);
    bleNotify(line.c_str());
    return true;
  }
  if (!strncmp(cmd, "__HPUT__:", 9)) {
    s_upTotal = 0;
    bleNotify(captiveDocPutBegin(String(cmd + 9)) ? "hput:ready" : "hput:err");
    return true;
  }
  if (!strncmp(cmd, "__HADD__:", 9)) {
    uint8_t raw[RAW_CHUNK + 8];
    size_t n = b64dec(cmd + 9, raw, sizeof(raw));
    bool ok = n && captiveDocPutChunk(raw, n);
    if (ok) s_upTotal += n;
    bleNotify(ok ? (String("hack:") + s_upTotal).c_str() : "hack:err");
    return true;
  }
  if (!strcmp(cmd, "__HEND__")) {
    bleNotify((String("hdone:") + captiveDocPutEnd()).c_str());
    showStatus();
    return true;
  }
  if (!strncmp(cmd, "__HRESET__:", 11)) {                 // restore a page to the built-in default
    bool ok = captiveDocReset(String(cmd + 11));
    bleNotify(ok ? "hreset:ok" : "hreset:err");
    return true;
  }
  if (!strcmp(cmd, "__APREST__")) {   // apply a changed SSID live (no reboot)
    captiveRestartAp();
    showStatus();
    bleNotify("ap:ok");
    return true;
  }
  if (!strcmp(cmd, "__RECN__")) {
    bleNotify((String("recn:") + captiveRecordCount()).c_str());
    return true;
  }
  if (!strcmp(cmd, "__RECCLR__")) {
    captiveRecordClear();
    bleNotify("recn:0");
    showStatus();
    return true;
  }
  if (!strcmp(cmd, "__REBOOT__")) { bleNotify("reboot:ok"); delay(400); ESP.restart(); return true; }
  // This board's own live SoftAP identity (so the portal can show before/after a clone took effect).
  if (!strcmp(cmd, "__APINFO__")) {
    bleNotify((String("apinfo:") + WiFi.softAPmacAddress() + "|" + cfgGet("ssid", "Building-WiFi") + "|" + WiFi.channel()).c_str());
    return true;
  }
  // "Replace a board": scan nearby 2.4GHz open APs so the portal can list them.
  if (!strcmp(cmd, "__APSCAN__")) {
    WiFi.mode(WIFI_AP_STA);                          // need STA to scan; AP stays up (clients blip during the ~2s scan)
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; i++) {
      if (WiFi.encryptionType(i) != WIFI_AUTH_OPEN) continue;   // open networks only
      if (WiFi.channel(i) > 13) continue;            // 2.4GHz only (the C5 also scans 5GHz — irrelevant for the captive AP)
      String line = "apscan:" + WiFi.SSID(i) + "|" + WiFi.BSSIDstr(i) + "|" + WiFi.channel(i) + "|" + WiFi.RSSI(i);
      bleNotify(line.c_str()); delay(45);            // space out notifies — no flush, so back-to-back would clobber
    }
    WiFi.scanDelete();
    WiFi.mode(WIFI_AP);                              // back to AP-only
    bleNotify("apscan:done");
    return true;
  }
  // Adopt the old board's identity: "__ADOPT__:<ssid>|<bssid>|<channel>" (parse from the tail so an SSID may contain '|').
  if (!strncmp(cmd, "__ADOPT__:", 10)) {
    String a = cmd + 10;
    int pc = a.lastIndexOf('|');
    int pb = (pc > 0) ? a.lastIndexOf('|', pc - 1) : -1;
    if (pb < 0) { bleNotify("adopt:err"); return true; }
    String s = a.substring(0, pb), bssid = a.substring(pb + 1, pc), ch = a.substring(pc + 1);
    cfgSet("ssid", s.c_str()); cfgSet("apmac", bssid.c_str()); cfgSet("channel", ch.c_str());
    bleNotify("adopt:ok");
    delay(400); ESP.restart();                       // reboot -> captiveBegin applies the cloned SSID/channel/MAC cleanly
    return true;
  }
  return false;
}

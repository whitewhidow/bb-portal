// See netota.h.
#include "netota.h"
#include "version.h"
#include "display.h"
#include "switch_targets.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <Preferences.h>

const char* netVersion() { return APP_VERSION; }

static Preferences s_prefs;
static String      s_ssid, s_pass;

#define OTA_MAGIC    0xB007A007u
#define SWITCH_MAGIC 0x5757C0D2u
RTC_NOINIT_ATTR static uint32_t s_bootOta;
RTC_NOINIT_ATTR static uint32_t s_bootSwitch;
RTC_NOINIT_ATTR static int32_t  s_switchIdx;

void netBegin() {
  s_prefs.begin("app", true);
  s_ssid = s_prefs.getString("ssid", "");
  s_pass = s_prefs.getString("pass", "");
  s_prefs.end();
}
void netConnect() {
  if (!s_ssid.length()) return;
  WiFi.mode(WIFI_STA); WiFi.setSleep(true); WiFi.disconnect();
  WiFi.begin(s_ssid.c_str(), s_pass.c_str());
}
void netSetCreds(const String& ssid, const String& pass) {
  s_prefs.begin("app", false);
  s_prefs.putString("ssid", ssid); s_prefs.putString("pass", pass);
  s_prefs.end();
  s_ssid = ssid; s_pass = pass;
}
void netClearCreds() {
  s_prefs.begin("app", false); s_prefs.remove("ssid"); s_prefs.remove("pass"); s_prefs.end();
  s_ssid = ""; s_pass = ""; WiFi.disconnect(true);
}
bool netConnected() { return WiFi.status() == WL_CONNECTED; }
bool netConfigured() { return s_ssid.length() > 0; }

String netStatus() {
  wl_status_t w = WiFi.status();
  // config-only board: creds are stored, not kept connected — so "saved", not "connecting".
  const char* st = (w == WL_CONNECTED) ? "connected" : (s_ssid.length() ? "saved" : "unset");
  String ip = (w == WL_CONNECTED) ? WiFi.localIP().toString() : String("-");
  return String("wifi:") + (s_ssid.length() ? s_ssid : String("-")) + "|" + st + "|" + ip + "|" + APP_VERSION;
}

// Blocking OTA download into the spare slot. cb(pct,msg) reports progress/errors.
// `url` = the bin to flash (this firmware's latest for self-update, or a sibling's
// for a switch). The spare A/B slot doesn't care whose app it is.
static String otaDownload(void (*cb)(int, const char*), const char* url) {
  if (WiFi.status() != WL_CONNECTED) { if (cb) cb(0, "no wifi"); return "err:wifi"; }
  WiFiClientSecure client; client.setInsecure();          // GitHub certs valid; skip the bundle
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); // release assets 302 to a CDN
  http.setTimeout(15000);
  if (!http.begin(client, url)) { if (cb) cb(0, "http begin"); return "err:begin"; }
  int code = http.GET();
  if (code != HTTP_CODE_OK) { char e[24]; snprintf(e,sizeof(e),"http %d",code); if (cb) cb(0,e); http.end(); return "err:http"; }
  int total = http.getSize();
  if (total <= 0) { if (cb) cb(0, "no length"); http.end(); return "err:size"; }
  if (!Update.begin((size_t)total)) { if (cb) cb(0, Update.errorString()); http.end(); return "err:begin2"; }
  if (cb) Update.onProgress([cb](size_t d, size_t t){ if (t){ char m[24]; int p=(int)(d*100/t); snprintf(m,sizeof(m),"writing %d%%",p); cb(p,m);} });
  size_t w = Update.writeStream(*http.getStreamPtr());
  http.end();
  if (w != (size_t)total) { Update.abort(); if (cb) cb(0, "incomplete"); return "err:write"; }
  if (!Update.end(true))  { if (cb) cb(0, Update.errorString()); return "err:end"; }
  return "ok";
}

void netRequestOta() { s_bootOta = OTA_MAGIC; delay(200); ESP.restart(); }
void netRequestSwitch(int idx) { s_bootSwitch = SWITCH_MAGIC; s_switchIdx = idx; delay(200); ESP.restart(); }

static void otaScreen(int pct, const char* msg) {
  char b[48]; snprintf(b, sizeof(b), "%s\n%d%%", msg, pct);
  dispCenter("UPDATE", b, 0xF7C948);
}

void netRunOtaAtBoot() {
  bool self = (s_bootOta == OTA_MAGIC);
  bool sw   = (s_bootSwitch == SWITCH_MAGIC);
  if (!self && !sw) return;
  s_bootOta = 0; s_bootSwitch = 0;

  // Pick the bin: a switch to a valid sibling, else this firmware's latest.
  const char* url   = APP_OTA_URL;
  const char* label = "UPDATE";
  if (sw && s_switchIdx >= 0 && s_switchIdx < SWITCH_TARGET_COUNT) {
    url = SWITCH_TARGETS[s_switchIdx].url; label = SWITCH_TARGETS[s_switchIdx].name;
  }

  dispCenter(label, "connecting wifi", 0x22D3E0);
  netBegin(); netConnect();
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) delay(200);
  if (WiFi.status() != WL_CONNECTED) { dispCenter(label, "NO WIFI", 0xE5484D); delay(2500); return; }
  String r = otaDownload(otaScreen, url);
  if (r == "ok") { dispCenter(label, "booting", 0x3FB950); delay(700); ESP.restart(); }
  dispCenter("FAILED", r.c_str(), 0xE5484D); delay(3000);   // fall through to a normal boot
}

// See netota.h.
#include "netota.h"
#include "version.h"
#include "display.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <Preferences.h>

const char* netVersion() { return APP_VERSION; }

static Preferences s_prefs;
static String      s_ssid, s_pass;

#define OTA_MAGIC 0xB007A007u
RTC_NOINIT_ATTR static uint32_t s_bootOta;

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
static String otaDownload(void (*cb)(int, const char*)) {
  if (WiFi.status() != WL_CONNECTED) { if (cb) cb(0, "no wifi"); return "err:wifi"; }
  WiFiClientSecure client; client.setInsecure();          // GitHub certs valid; skip the bundle
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); // release assets 302 to a CDN
  http.setTimeout(15000);
  if (!http.begin(client, APP_OTA_URL)) { if (cb) cb(0, "http begin"); return "err:begin"; }
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

static void otaScreen(int pct, const char* msg) {
  char b[48]; snprintf(b, sizeof(b), "%s\n%d%%", msg, pct);
  dispCenter("UPDATE", b, 0xF7C948);
}

void netRunOtaAtBoot() {
  if (s_bootOta != OTA_MAGIC) return;
  s_bootOta = 0;
  dispCenter("UPDATE", "connecting wifi", 0x22D3E0);
  netBegin(); netConnect();
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) delay(200);
  if (WiFi.status() != WL_CONNECTED) { dispCenter("UPDATE", "NO WIFI", 0xE5484D); delay(2500); return; }
  String r = otaDownload(otaScreen);
  if (r == "ok") { dispCenter("UPDATE", "booting", 0x3FB950); delay(700); ESP.restart(); }
  dispCenter("UPDATE FAILED", r.c_str(), 0xE5484D); delay(3000);   // fall through to normal boot
}

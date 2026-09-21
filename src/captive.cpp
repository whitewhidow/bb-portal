// See captive.h. HTML docs live in LittleFS and are edited over BLE; /sign stores
// every submitted form value generically (nothing hardcoded).
#include "captive.h"
#include "config.h"
#include "led.h"
#include <WiFi.h>
#include <esp_wifi.h>   // esp_wifi_set_mac — clone an old board's AP MAC (the "Replace a board" adopt flow)
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

static const IPAddress AP_IP(4, 3, 2, 1);
static const IPAddress AP_MASK(255, 255, 255, 0);
static const String    PORTAL_URL = "http://4.3.2.1";
#define WIFI_CHANNEL 6
#define MAX_CLIENTS  8
#include "clock.h"
#define REC_FILE     "/records.jsonl"
#define TMP_FILE     "/upload.tmp"

static DNSServer      dnsServer;
static AsyncWebServer server(80);

static String ssid() { String s = cfgGet("ssid", "Building-WiFi"); return s.length() ? s : "Building-WiFi"; }
static String code() { String s = cfgGet("code", "REPLACE-WITH-CODE"); return s.length() ? s : "REPLACE-WITH-CODE"; }

// Last channel-scan results, kept so the portal can display them (antenna comparison) and so a
// restart reuses the pick without rescanning. Filled by runChannelScan() at boot and on __CHSCAN__.
struct ScanAp { char ssid[33]; char bssid[18]; int8_t ch; int8_t rssi; };
#define SCAN_MAX 40
static ScanAp s_scan[SCAN_MAX];
static int    s_scanN = 0;                           // APs stored (capped at SCAN_MAX)
static int    s_scanSeen = 0;                         // APs the radio actually saw (may exceed SCAN_MAX)
static long   s_scanScore[3] = { 0, 0, 0 };          // congestion score for 1 / 6 / 11
static int    s_scanPick = 0;

// Scan the 2.4GHz band (channels 1-13 only) and score channels 1/6/11 by summing each AP's
// strength weighted by how much it bleeds into that band. We scan ONE channel per call
// (scanNetworks' channel arg) and loop 1-13, rather than a single all-channel scan: on the
// dual-band C5 an all-channel scan also sweeps 5GHz, whose DFS channels need long *passive*
// listening (radar detection) that max_ms_per_chan can't shorten — that made the scan ~20s and
// timed the portal out. 2.4GHz-only keeps it to ~2-4s. (The captive AP is 2.4GHz and only ch
// 1-13 matter for its channel choice; 5GHz neighbours are irrelevant here.)
// Needs STA, so callers run it before softAP() (boot) or restore AP mode after (on-demand).
static int runChannelScan() {
  const int cands[3] = { 1, 6, 11 };
  // overlap[d] = fraction of an AP d channels away that lands in a candidate's ~22MHz band.
  static const float overlap[5] = { 1.0f, 0.8f, 0.6f, 0.4f, 0.2f };
  uint32_t t0 = millis();
  s_scanScore[0] = s_scanScore[1] = s_scanScore[2] = 0; s_scanN = 0; s_scanSeen = 0;
  WiFi.mode(WIFI_AP_STA);                            // STA needed to scan; AP (if up) blips briefly
  for (int ch = 1; ch <= 13; ch++) {                // 2.4GHz only — one channel per call
    int n = WiFi.scanNetworks(false, true, false, 150, ch);   // blocking, hidden, 150ms dwell, this channel
    if (n < 0) { WiFi.scanDelete(); continue; }
    s_scanSeen += n;
    for (int i = 0; i < n; i++) {
      long w = WiFi.RSSI(i) + 100; if (w < 0) w = 0;  // strong neighbour hurts more (-45->55, -90->10)
      for (int c = 0; c < 3; c++) { int d = abs(ch - cands[c]); if (d < 5) s_scanScore[c] += (long)(w * overlap[d]); }
      if (s_scanN < SCAN_MAX) {
        ScanAp& a = s_scan[s_scanN++];
        String ss = WiFi.SSID(i); if (!ss.length()) ss = "(hidden)";
        strlcpy(a.ssid, ss.c_str(), sizeof(a.ssid));
        strlcpy(a.bssid, WiFi.BSSIDstr(i).c_str(), sizeof(a.bssid));
        a.ch = (int8_t)ch; a.rssi = (int8_t)WiFi.RSSI(i);
      }
    }
    WiFi.scanDelete();
  }
  int best = 1;                                      // lowest score wins; tie-breaks toward 6 then lower ch
  for (int c = 0; c < 3; c++) if (s_scanScore[c] < s_scanScore[best] || (s_scanScore[c] == s_scanScore[best] && cands[c] == 6)) best = c;
  s_scanPick = s_scanSeen > 0 ? cands[best] : WIFI_CHANNEL;   // saw nothing -> default 6
  Serial.printf("[CP] channel scan: 1=%ld 6=%ld 11=%ld -> pick %d (%d APs, %d stored) in %lums\n",
                s_scanScore[0], s_scanScore[1], s_scanScore[2], s_scanPick, s_scanSeen, s_scanN, millis() - t0);
  return s_scanPick;
}

// On-demand rescan (portal "Channel scan" / antenna test): refresh the stored list without changing
// the live AP channel, then restore AP-only. Returns the channel it *would* pick now.
int captiveChannelScan() { int p = runChannelScan(); WiFi.mode(WIFI_AP); return p; }
int    captiveScanCount()       { return s_scanN; }
String captiveScanLine(int i)   { if (i < 0 || i >= s_scanN) return ""; const ScanAp& a = s_scan[i];
  return String(a.ssid) + "|" + a.bssid + "|" + a.ch + "|" + a.rssi; }
String captiveScanSummary()     { return String("1=") + s_scanScore[0] + "|6=" + s_scanScore[1] + "|11=" + s_scanScore[2]
  + "|pick=" + s_scanPick + "|n=" + s_scanSeen; }

// Resolve the SoftAP channel. Config "channel" 1-13 = manual; blank/0/"auto" = auto-pick 1/6/11.
// Cached in s_chan after the first resolve so status reporting and restarts reuse the same value
// (re-scanning would bounce the AP and drop clients).
static int s_chan = 0;
static int chan() {
  if (s_chan) return s_chan;
  String cv = cfgGet("channel", ""); cv.trim();
  int c = cv.toInt();
  s_chan = (c >= 1 && c <= 13) ? c : runChannelScan();
  return s_chan;
}
// Live channel for status/BLE reporting (0 until the AP has been brought up once).
int captiveChannel() { return s_chan; }
// Parse "AA:BB:CC:DD:EE:FF" -> 6 bytes. Returns false on any malformed field.
static bool parseMac(const String& s, uint8_t out[6]) {
  int n = sscanf(s.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &out[0], &out[1], &out[2], &out[3], &out[4], &out[5]);
  return n == 6;
}
// Cap the SoftAP TX power from config. Blank/0 -> leave the stack default (its max, ~20dBm).
// esp_wifi_set_max_tx_power() wants 0.25dBm units [8..84] = 2..21dBm; must run AFTER the AP is up.
static void applyTxPower() {
  int dbm = cfgGet("txpower", "").toInt();
  if (dbm <= 0) return;                              // unset -> keep driver default (max)
  if (dbm < 2)  dbm = 2;
  if (dbm > 20) dbm = 20;
  esp_err_t e = esp_wifi_set_max_tx_power((int8_t)(dbm * 4));
  int8_t cur = 0; esp_wifi_get_max_tx_power(&cur);   // read back what the stack actually accepted
  Serial.printf("[CP] TX power -> %ddBm (%s), now %.2fdBm\n", dbm, e == ESP_OK ? "ok" : esp_err_to_name(e), cur / 4.0);
}

// If a cloned AP MAC is configured, apply it to the SoftAP interface (must precede WiFi.softAP()).
// Works on S3 and C5 alike. Blank config -> keep this board's own factory MAC.
static void applyApMac() {
  String m = cfgGet("apmac", ""); m.trim();
  uint8_t mac[6];
  if (m.length() && parseMac(m, mac)) {
    esp_err_t e = esp_wifi_set_mac(WIFI_IF_AP, mac);
    Serial.printf("[CP] clone AP MAC %s -> %s\n", m.c_str(), e == ESP_OK ? "ok" : esp_err_to_name(e));
  }
}

// Substitute the configurable placeholders into a page. Works for the start page and
// the done page alike: {{CODE}} plus four generic slots {{VALUE1}}..{{VALUE4}} that
// map to the value1..value4 config fields (empty if unset). Add more here as needed.
static void applyPlaceholders(String& html) {
  html.replace("{{CODE}}", code());
  html.replace("{{VALUE1}}", cfgGet("value1", ""));
  html.replace("{{VALUE2}}", cfgGet("value2", ""));
  html.replace("{{VALUE3}}", cfgGet("value3", ""));
  html.replace("{{VALUE4}}", cfgGet("value4", ""));
}

// doc name -> file path. Only "portal"/"done" are writable; "records" is read-only.
static String docPath(const String& doc) {
  if (doc == "portal")  return "/portal.html";
  if (doc == "done")    return "/done.html";
  if (doc == "records") return REC_FILE;
  return "";
}

// ------------------------------- defaults ----------------------------------

static const char DEFAULT_PORTAL[] PROGMEM =
"<!DOCTYPE html>\n"
"<html lang='en'>\n"
"<head>\n"
"  <meta charset='utf-8'>\n"
"  <meta name='viewport' content='width=device-width,initial-scale=1'>\n"
"  <title>Wi-Fi Access - Terms</title>\n"
"  <style>\n"
"    body{font-family:-apple-system,Segoe UI,Roboto,sans-serif;margin:0;background:#0e1726;color:#e8edf5}\n"
"    .wrap{max-width:640px;margin:0 auto;padding:20px}\n"
"    h1{font-size:1.35rem}\n"
"    .terms{background:#182338;border:1px solid #2a3b57;border-radius:10px;padding:14px;height:200px;overflow:auto;font-size:.86rem;line-height:1.5}\n"
"    label{display:block;margin:14px 0 6px;font-weight:600}\n"
"    input[type=text],input[type=number]{width:100%;padding:12px;font-size:1rem;border-radius:8px;border:1px solid #2a3b57;background:#0b1220;color:#fff;box-sizing:border-box}\n"
"    .chk{display:flex;gap:10px;margin:14px 0;font-size:.9rem}\n"
"    .chk input{width:20px;height:20px}\n"
"    button{width:100%;padding:14px;font-size:1.05rem;font-weight:700;border:0;border-radius:8px;background:#2f7bff;color:#fff;margin-top:8px}\n"
"  </style>\n"
"</head>\n"
"<body>\n"
"  <div class='wrap'>\n"
"    <h1>Welcome</h1>\n"
"    <p>Please read and accept the terms to receive your access code.</p>\n"
"    <div class='terms'>\n"
"      <b>Terms &amp; Conditions / Legal Notice</b>\n"
"      <p>[Edit this page from the BLE web portal to set your building's real legal text and fields.]</p>\n"
"      <p>By entering your details below you confirm you have read and agree to these terms.</p>\n"
"    </div>\n"
"    <form method='POST' action='http://4.3.2.1/sign' accept-charset='utf-8'>\n"
"      <label>Full name</label>\n"
"      <input type='text' name='name' required maxlength='80'>\n"
"      <label>Apartment / unit</label>\n"
"      <input type='text' name='unit' required maxlength='40'>\n"
"      <label>Number of people</label>\n"
"      <input type='number' name='people' min='1' max='20'>\n"
"      <label class='chk'><input type='checkbox' name='agree' value='yes' required> I have read and agree to the terms.</label>\n"
"      <button type='submit'>Agree &amp; get my access code</button>\n"
"    </form>\n"
"  </div>\n"
"</body>\n"
"</html>\n";

static const char DEFAULT_DONE[] PROGMEM =
"<!DOCTYPE html>\n"
"<html lang='en'>\n"
"<head>\n"
"  <meta charset='utf-8'>\n"
"  <meta name='viewport' content='width=device-width,initial-scale=1'>\n"
"  <title>Your access code</title>\n"
"  <style>\n"
"    body{font-family:-apple-system,Segoe UI,Roboto,sans-serif;margin:0;background:#0e1726;color:#e8edf5}\n"
"    .wrap{max-width:640px;margin:0 auto;padding:24px;text-align:center}\n"
"    .code{font-size:2rem;font-weight:800;letter-spacing:2px;background:#12351d;border:1px solid #2f7d46;color:#7CFFA0;border-radius:12px;padding:18px;margin:20px 0;word-break:break-all}\n"
"    ol{text-align:left;line-height:1.6}\n"
"  </style>\n"
"</head>\n"
"<body>\n"
"  <div class='wrap'>\n"
"    <h1>Thank you</h1>\n"
"    <p>Your acceptance has been recorded. Your access code:</p>\n"
"    <div class='code'>{{CODE}}</div>\n"
"    <ol>\n"
"      <li>Write down or screenshot this code.</li>\n"
"      <li>Connect to the building internet Wi-Fi.</li>\n"
"      <li>Enter the code when prompted.</li>\n"
"    </ol>\n"
"  </div>\n"
"</body>\n"
"</html>\n";

static void writeIfMissing(const char* path, const char* body) {
  if (LittleFS.exists(path)) {                 // keep a real file, but restore a
    File e = LittleFS.open(path, FILE_READ);   // 0-byte one (e.g. from a bad save)
    size_t sz = e ? e.size() : 0; if (e) e.close();
    if (sz > 0) return;
  }
  File f = LittleFS.open(path, FILE_WRITE);
  if (f) { f.print(body); f.close(); }
}

// Overwrite a page with its built-in (now nicely-indented) default. Lets the editor
// offer "Reset to default" so a board that saved an older minified page can pull the
// readable template back.
bool captiveDocReset(const String& doc) {
  const char* body = (doc == "portal") ? DEFAULT_PORTAL
                   : (doc == "done")   ? DEFAULT_DONE : nullptr;
  if (!body) return false;
  File f = LittleFS.open(docPath(doc), FILE_WRITE);
  if (!f) return false;
  f.print(body); f.close();
  return true;
}

// --------------------------------- helpers ---------------------------------

static String jsonEscape(const String& in) {
  String o; o.reserve(in.length() + 8);
  for (char c : in) {
    switch (c) {
      case '"':  o += "\\\""; break;
      case '\\': o += "\\\\"; break;
      case '\n': o += "\\n";  break;
      case '\r': o += "\\r";  break;
      case '\t': o += "\\t";  break;
      default: if ((uint8_t)c < 0x20) { char b[7]; snprintf(b, 7, "\\u%04x", c); o += b; } else o += c;
    }
  }
  return o;
}

static String readWhole(const String& path) {
  File f = LittleFS.open(path, FILE_READ);
  if (!f) return "";
  String s = f.readString(); f.close(); return s;
}

// ------------------------------ record storage -----------------------------

// Store EVERY posted form field as one JSON record line. No field list — whatever
// the (editable) form submits is captured.
// Safety net: if the records file grows past REC_MAX, keep only the last REC_KEEP
// bytes (line-aligned) so a full LittleFS can never silently stop accepting sign-ups.
#define REC_MAX_BYTES  (256u * 1024u)
#define REC_KEEP_BYTES (192u * 1024u)
static void trimRecordsIfNeeded() {
  File f = LittleFS.open(REC_FILE, FILE_READ);
  if (!f) return;
  size_t sz = f.size();
  if (sz <= REC_MAX_BYTES) { f.close(); return; }
  f.seek(sz - REC_KEEP_BYTES);
  while (f.available()) { if (f.read() == '\n') break; }   // align to a line start
  File t = LittleFS.open("/records.tmp", FILE_WRITE);
  if (!t) { f.close(); return; }
  uint8_t buf[256]; int n;
  while ((n = f.read(buf, sizeof(buf))) > 0) t.write(buf, n);
  f.close(); t.close();
  LittleFS.remove(REC_FILE); LittleFS.rename("/records.tmp", REC_FILE);
  Serial.println("[REC] rotated — kept the newest ~192KB");
}

static void recordSubmission(AsyncWebServerRequest* req) {
  String row = "{\"id\":" + String(captiveRecordCount() + 1) +
               ",\"up\":" + String(millis()) +
               ",\"at\":" + String(clockNow()) +
               ",\"ip\":\"" + req->client()->remoteIP().toString() + "\"";
  int n = req->params();
  for (int i = 0; i < n; i++) {
    const AsyncWebParameter* p = req->getParam(i);
    if (!p->isPost()) continue;
    String k = p->name(); String v = p->value();
    if (v.length() > 200) v = v.substring(0, 200);
    row += ",\"" + jsonEscape(k) + "\":\"" + jsonEscape(v) + "\"";
  }
  row += "}\n";
  File f = LittleFS.open(REC_FILE, FILE_APPEND);
  if (f) { f.print(row); f.close(); Serial.print("[REC] "); Serial.print(row); trimRecordsIfNeeded(); }
  else Serial.println("[REC] open failed");
}

uint32_t captiveRecordCount() {
  if (!LittleFS.exists(REC_FILE)) return 0;
  File r = LittleFS.open(REC_FILE, FILE_READ);
  if (!r) return 0;
  uint32_t n = 0; while (r.available()) if (r.read() == '\n') n++;
  r.close(); return n;
}

void captiveRecordClear() { LittleFS.remove(REC_FILE); Serial.println("[REC] cleared"); }

// ------------------------------ doc transfer -------------------------------

size_t captiveDocSize(const String& doc) {
  String p = docPath(doc); if (!p.length() || !LittleFS.exists(p)) return 0;
  File f = LittleFS.open(p, FILE_READ); if (!f) return 0;
  size_t s = f.size(); f.close(); return s;
}

int captiveDocRead(const String& doc, size_t off, uint8_t* out, size_t maxlen) {
  String p = docPath(doc); if (!p.length() || !LittleFS.exists(p)) return 0;
  File f = LittleFS.open(p, FILE_READ); if (!f) return 0;
  if (off) f.seek(off);
  int r = f.read(out, maxlen); f.close();
  return r < 0 ? 0 : r;
}

static File   s_putFile;
static String s_putDoc;

bool captiveDocPutBegin(const String& doc) {
  if (doc != "portal" && doc != "done") return false;   // records are read-only
  s_putDoc = doc;
  s_putFile = LittleFS.open(TMP_FILE, FILE_WRITE);       // truncate
  return (bool)s_putFile;
}
bool captiveDocPutChunk(const uint8_t* data, size_t len) {
  if (!s_putFile) return false;
  return s_putFile.write(data, len) == len;
}
size_t captiveDocPutEnd() {
  if (!s_putFile) return 0;
  s_putFile.flush();
  s_putFile.close();                       // size() on an open write handle can read 0
  size_t sz = 0;                            // — stat the file AFTER closing
  { File t = LittleFS.open(TMP_FILE, FILE_READ); if (t) { sz = t.size(); t.close(); } }
  String dst = docPath(s_putDoc);
  if (sz > 0) { LittleFS.remove(dst); LittleFS.rename(TMP_FILE, dst); }  // never clobber with an empty file
  else LittleFS.remove(TMP_FILE);
  Serial.printf("[DOC] %s <- %u bytes\n", dst.c_str(), (unsigned)sz);
  return sz;
}

// --------------------------------- web -------------------------------------

static void startWeb() {
  server.on("/connecttest.txt", [](AsyncWebServerRequest* r){ r->redirect("http://logout.net"); });
  server.on("/wpad.dat",        [](AsyncWebServerRequest* r){ r->send(404); });
  server.on("/generate_204",    [](AsyncWebServerRequest* r){ r->redirect(PORTAL_URL); });
  server.on("/gen_204",         [](AsyncWebServerRequest* r){ r->redirect(PORTAL_URL); });
  server.on("/redirect",        [](AsyncWebServerRequest* r){ r->redirect(PORTAL_URL); });
  server.on("/hotspot-detect.html",[](AsyncWebServerRequest* r){ r->redirect(PORTAL_URL); });
  server.on("/library/test/success.html",[](AsyncWebServerRequest* r){ r->redirect(PORTAL_URL); });
  server.on("/canonical.html",  [](AsyncWebServerRequest* r){ r->redirect(PORTAL_URL); });
  server.on("/success.txt",     [](AsyncWebServerRequest* r){ r->send(200); });
  server.on("/ncsi.txt",        [](AsyncWebServerRequest* r){ r->redirect(PORTAL_URL); });
  server.on("/favicon.ico",     [](AsyncWebServerRequest* r){ r->send(404); });

  // the terms/form page (edited over BLE) — read + substitute placeholders, then send
  // (was streamed raw; now goes through applyPlaceholders so {{VALUE1..4}}/{{CODE}}
  // work on the START page too, not just the done page).
  server.on("/", HTTP_ANY, [](AsyncWebServerRequest* r){
    String html = readWhole("/portal.html");
    if (!html.length()) { r->send(200, "text/html", "portal not configured"); return; }
    applyPlaceholders(html);
    r->send(200, "text/html; charset=utf-8", html);
  });

  // submit: capture ALL fields, then show the (editable) done page with the placeholders
  server.on("/sign", HTTP_POST, [](AsyncWebServerRequest* r){
    recordSubmission(r);
    if (cfgGet("led", "1") != "0") ledFlash(cfgLedMs());   // green flash on a signed agreement (no-op if no LED)
    String done = readWhole("/done.html");
    if (!done.length()) done = "<h1>Thank you</h1><p>Code: {{CODE}}</p>";
    applyPlaceholders(done);
    r->send(200, "text/html; charset=utf-8", done);
  });

  server.onNotFound([](AsyncWebServerRequest* r){ r->redirect(PORTAL_URL); });
  server.begin();
  Serial.println("[CP] web server up on :80");
}

void captiveBegin() {
  // Mount the "littlefs" data partition by LABEL (canonical family table also has a
  // separate small "spiffs" for bboink's config, so the default first-spiffs match would
  // grab the wrong one). Fall back to the default for older single-partition tables.
  bool fs = LittleFS.begin(true, "/littlefs", 10, "littlefs") || LittleFS.begin(true);
  Serial.printf("[CP] LittleFS %s\n", fs ? "mounted" : "MOUNT FAILED");

  // Seed config defaults into NVS on first boot so the Config form shows real,
  // editable values (otherwise the fields render empty and the SSID/code shown by
  // the AP live only as C++ fallbacks).
  if (cfgGet("ssid", "") == "") cfgSet("ssid", "Building-WiFi");
  if (cfgGet("code", "") == "") cfgSet("code", "REPLACE-WITH-CODE");

  // DIAGNOSTIC: log each doc's size as read from flash at boot, BEFORE writeIfMissing.
  // If a page you saved comes back 0/missing here, the save didn't persist (and
  // writeIfMissing then restores the default) — the "edits vanish on reboot" bug.
  { size_t ps = 0, ds = 0; File a = LittleFS.open("/portal.html", FILE_READ);
    if (a) { ps = a.size(); a.close(); } File b = LittleFS.open("/done.html", FILE_READ);
    if (b) { ds = b.size(); b.close(); }
    Serial.printf("[CP] boot doc sizes (from flash): portal=%u done=%u\n", (unsigned)ps, (unsigned)ds); }

  writeIfMissing("/portal.html", DEFAULT_PORTAL);
  writeIfMissing("/done.html",   DEFAULT_DONE);

  WiFi.mode(WIFI_AP);
  applyApMac();                                     // clone an old board's AP MAC if configured (before softAP)
  WiFi.softAPConfig(AP_IP, AP_IP, AP_MASK);
  WiFi.softAP(ssid().c_str(), nullptr /*open*/, chan(), 0, MAX_CLIENTS);
  applyTxPower();                                    // cap TX power if configured (after the AP is up)
  Serial.printf("[CP] SoftAP '%s' at %s ch%d mac %s\n", ssid().c_str(),
                WiFi.softAPIP().toString().c_str(), chan(), WiFi.softAPmacAddress().c_str());

  dnsServer.setTTL(3600);
  dnsServer.start(53, "*", AP_IP);
  Serial.println("[CP] DNS catch-all up (* -> 4.3.2.1)");

  startWeb();
  Serial.printf("[CP] ready. %u records on file.\n", captiveRecordCount());
}

void captiveRestartAp() {
  WiFi.softAPdisconnect(false);
  applyApMac();                                     // best-effort live MAC clone (a full clone is most reliable via reboot; see __ADOPT__)
  WiFi.softAPConfig(AP_IP, AP_IP, AP_MASK);
  WiFi.softAP(ssid().c_str(), nullptr, chan(), 0, MAX_CLIENTS);
  applyTxPower();                                    // re-apply the TX power cap after the AP restart
  Serial.printf("[CP] SoftAP re-applied: '%s' ch%d mac %s\n", ssid().c_str(), chan(), WiFi.softAPmacAddress().c_str());
}

void captiveTick() { dnsServer.processNextRequest(); }

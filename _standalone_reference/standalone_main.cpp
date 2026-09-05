// ===========================================================================
//  Terms Portal  —  ESP32 captive portal for an apartment building
// ---------------------------------------------------------------------------
//  Flow:  join open Wi-Fi  ->  phone auto-pops the captive portal  ->  resident
//         reads the Terms & Conditions + legal notice  ->  types their name and
//         ticks "I agree" (the signature)  ->  submits  ->  screen shows the
//         ACCESS CODE they enter on the building's real internet routers.
//
//  This device has NO internet behind it. Its only job is: reliably fire the
//  "Sign in to Wi-Fi" popup, present the terms, record the signature, show the
//  code. Nothing is NAT'd or gated here.
//
//  Why this reliably triggers the popup in 2026 (the hard part everyone hits):
//    1) AP IP lives in PUBLIC space (4.3.2.1), not 192.168.x.x. Samsung/Android
//       hardcode a connectivity check and ignore the DNS redirect on private IPs.
//    2) AMPDU RX is disabled in the Wi-Fi init (an Android association bug).
//    3) Every OS captive-probe URL is answered explicitly + a catch-all redirect.
//    4) The sign form is PLAIN HTML POST: the iOS captive mini-browser runs NO
//       JavaScript and stores NO cookies, so the whole flow must be server-side.
//  Reference: github.com/CDFER/Captive-Portal-ESP32 (the "actually works" one).
// ===========================================================================
#include <Arduino.h>
#include <AsyncTCP.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <esp_wifi.h>
#include <LittleFS.h>

// ======================= THINGS YOU MUST EDIT ==============================
static const char *AP_SSID   = "Building-WiFi";     // the network name residents see
static const char *AP_PASS   = nullptr;             // nullptr = OPEN network (recommended for a portal)
static const char *ACCESS_CODE = "REPLACE-WITH-YOUR-CODE";  // code shown after signing (for the real routers)
static const char *ADMIN_KEY = "change-this-admin-key";     // /log?key=... to read the signature list
static const char *BUILDING_NAME = "Maple Court Residences"; // shown in the page header / legal text
// ===========================================================================

// --- Captive-portal network constants (do NOT move to 192.168.x — see note 1) ---
static const IPAddress AP_IP(4, 3, 2, 1);
static const IPAddress AP_GW(4, 3, 2, 1);
static const IPAddress AP_MASK(255, 255, 255, 0);
static const String    PORTAL_URL = "http://4.3.2.1";
#define WIFI_CHANNEL 6
#define MAX_CLIENTS  8
#define DNS_INTERVAL 30
#define SIG_FILE     "/signatures.csv"

DNSServer dnsServer;
AsyncWebServer server(80);

// --------------------------- helpers ---------------------------------------

// Escape text before putting it back into HTML (defends the code page / logs).
static String htmlEscape(const String &in) {
  String o; o.reserve(in.length() + 8);
  for (char c : in) {
    switch (c) {
      case '&': o += "&amp;";  break;
      case '<': o += "&lt;";   break;
      case '>': o += "&gt;";   break;
      case '"': o += "&quot;"; break;
      case '\'':o += "&#39;";  break;
      default:  o += c;
    }
  }
  return o;
}

// Clean a name for CSV storage: strip CR/LF/commas, trim, cap length.
static String cleanForCsv(String s) {
  s.replace('\n', ' '); s.replace('\r', ' '); s.replace(',', ';');
  s.trim();
  if (s.length() > 80) s = s.substring(0, 80);
  return s;
}

// The Terms & Conditions + legal notice + signature form.
// Kept lean (well under iOS's 128 KB captive-browser limit) and JS-free.
// NOTE: never let the visible text contain the word "Success" — iOS treats
// that as "you have internet" and refuses to show the portal.
static String pageTerms(const String &err = "") {
  String h;
  h.reserve(4096);
  h += F("<!DOCTYPE html><html lang='en'><head><meta charset='utf-8'>"
         "<meta name='viewport' content='width=device-width,initial-scale=1'>"
         "<title>Wi-Fi Access — Terms</title><style>"
         "body{font-family:-apple-system,Segoe UI,Roboto,sans-serif;margin:0;"
         "background:#0e1726;color:#e8edf5}"
         ".wrap{max-width:640px;margin:0 auto;padding:20px}"
         "h1{font-size:1.35rem;margin:.2em 0}h2{font-size:1rem;color:#9fb3d1}"
         ".terms{background:#182338;border:1px solid #2a3b57;border-radius:10px;"
         "padding:14px;height:230px;overflow:auto;font-size:.86rem;line-height:1.5}"
         "label{display:block;margin:16px 0 6px;font-weight:600}"
         "input[type=text]{width:100%;padding:12px;font-size:1rem;border-radius:8px;"
         "border:1px solid #2a3b57;background:#0b1220;color:#fff;box-sizing:border-box}"
         ".chk{display:flex;align-items:flex-start;gap:10px;margin:14px 0;font-size:.9rem}"
         ".chk input{width:20px;height:20px;margin-top:2px}"
         "button{width:100%;padding:14px;font-size:1.05rem;font-weight:700;border:0;"
         "border-radius:8px;background:#2f7bff;color:#fff;margin-top:8px}"
         ".err{background:#5b1a1a;border:1px solid #a33;padding:10px;border-radius:8px;"
         "margin:12px 0;font-size:.9rem}.muted{color:#7f92ad;font-size:.8rem;margin-top:18px}"
         "</style></head><body><div class='wrap'>");
  h += "<h1>Welcome to " + htmlEscape(BUILDING_NAME) + "</h1>";
  h += F("<h2>Please read and accept the terms to receive your access code.</h2>");
  if (err.length()) h += "<div class='err'>" + htmlEscape(err) + "</div>";
  h += F("<div class='terms'>"
         "<b>Terms &amp; Conditions of Use / Legal Notice</b>"
         "<p>By using this Wi-Fi network you agree to the following. "
         "<i>[Replace this block with your building's real legal text.]</i></p>"
         "<p>1. This network is provided as-is, without warranty. Availability "
         "and speed are not guaranteed.</p>"
         "<p>2. You must not use the network for any unlawful purpose, to infringe "
         "third-party rights, or to distribute malware or unsolicited communications.</p>"
         "<p>3. You are responsible for all activity carried out under your access. "
         "The operator may log the fact and time of your acceptance of these terms.</p>"
         "<p>4. The operator accepts no liability for loss or damage arising from use "
         "of the network to the fullest extent permitted by law.</p>"
         "<p>5. By entering your name below you confirm you have read, understood and "
         "agree to be bound by these terms.</p>"
         "</div>");
  // Absolute action URL: the captive browser is already on 4.3.2.1, but being
  // explicit avoids any base-URL surprises inside the mini-browser.
  h += F("<form method='POST' action='http://4.3.2.1/sign' accept-charset='utf-8'>"
         "<label for='name'>Full name (your signature)</label>"
         "<input type='text' id='name' name='name' autocomplete='name' "
         "autocapitalize='words' required maxlength='80' placeholder='e.g. Jane Doe'>"
         "<div class='chk'><input type='checkbox' id='agree' name='agree' value='yes' required>"
         "<label for='agree' style='margin:0;font-weight:400'>I have read and agree to the "
         "Terms &amp; Conditions and Legal Notice above.</label></div>"
         "<button type='submit'>Agree &amp; get my access code</button></form>");
  h += F("<p class='muted'>No internet is provided on this network. After you accept, "
         "you will receive a code to use on the building's internet Wi-Fi.</p>"
         "</div></body></html>");
  return h;
}

// The page shown AFTER a valid signature: the access code + what to do with it.
static String pageCode(const String &name) {
  String h; h.reserve(2048);
  h += F("<!DOCTYPE html><html lang='en'><head><meta charset='utf-8'>"
         "<meta name='viewport' content='width=device-width,initial-scale=1'>"
         "<title>Your access code</title><style>"
         "body{font-family:-apple-system,Segoe UI,Roboto,sans-serif;margin:0;"
         "background:#0e1726;color:#e8edf5}.wrap{max-width:640px;margin:0 auto;padding:24px;text-align:center}"
         "h1{font-size:1.3rem}.code{font-size:2rem;font-weight:800;letter-spacing:2px;"
         "background:#12351d;border:1px solid #2f7d46;color:#7CFFA0;border-radius:12px;"
         "padding:18px;margin:20px 0;word-break:break-all}"
         "ol{text-align:left;line-height:1.6}.muted{color:#7f92ad;font-size:.85rem;margin-top:20px}"
         "</style></head><body><div class='wrap'>");
  h += "<h1>Thank you, " + htmlEscape(name) + "</h1>";
  h += F("<p>Your acceptance has been recorded. Here is your access code:</p>");
  h += "<div class='code'>" + htmlEscape(ACCESS_CODE) + "</div>";
  h += F("<ol><li>Write down or screenshot this code now.</li>"
         "<li>Leave this network and connect to the building internet Wi-Fi.</li>"
         "<li>Enter the code above when prompted.</li></ol>"
         "<p class='muted'>Keep this code private. If you lose it, reconnect here and accept the terms again.</p>"
         "</div></body></html>");
  return h;
}

// Append one signature line to LittleFS. No RTC/NTP on an offline AP, so we log
// an incrementing id + uptime(ms) + client IP. (See README for adding real time.)
static void recordSignature(const String &name, const IPAddress &ip) {
  File f = LittleFS.open(SIG_FILE, FILE_APPEND);
  if (!f) { Serial.println("[SIG] open failed"); return; }
  static uint32_t seq = 0;
  if (seq == 0) {  // seed seq from existing line count so ids stay unique across reboots
    File r = LittleFS.open(SIG_FILE, FILE_READ);
    if (r) { while (r.available()) if (r.read() == '\n') seq++; r.close(); }
  }
  String line = String(++seq) + "," + String(millis()) + "," +
                ip.toString() + "," + cleanForCsv(name) + "\n";
  f.print(line);
  f.close();
  Serial.print("[SIG] "); Serial.print(line);
}

// ------------------------------ Wi-Fi / DNS --------------------------------

static void startAP() {
  WiFi.mode(WIFI_MODE_AP);
  WiFi.softAPConfig(AP_IP, AP_GW, AP_MASK);
  WiFi.softAP(AP_SSID, AP_PASS, WIFI_CHANNEL, 0, MAX_CLIENTS);

  // Fix 2: disable AMPDU RX — some Android builds fail to associate otherwise.
  esp_wifi_stop();
  esp_wifi_deinit();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  cfg.ampdu_rx_enable = false;
  esp_wifi_init(&cfg);
  esp_wifi_start();
  vTaskDelay(100 / portTICK_PERIOD_MS);

  Serial.printf("[AP] SSID '%s'  IP %s  ch %d\n", AP_SSID,
                WiFi.softAPIP().toString().c_str(), WIFI_CHANNEL);
}

static void startDNS() {
  dnsServer.setTTL(3600);
  dnsServer.start(53, "*", AP_IP);   // every lookup -> us
  Serial.println("[DNS] catch-all up (* -> 4.3.2.1)");
}

static void startWeb() {
  // ---- OS captive-probe endpoints (fix 3) ----
  server.on("/connecttest.txt", [](AsyncWebServerRequest *r){ r->redirect("http://logout.net"); }); // Win 11
  server.on("/wpad.dat",        [](AsyncWebServerRequest *r){ r->send(404); });                      // stop Win retry storm
  server.on("/generate_204",    [](AsyncWebServerRequest *r){ r->redirect(PORTAL_URL); });           // Android
  server.on("/gen_204",         [](AsyncWebServerRequest *r){ r->redirect(PORTAL_URL); });           // Android (older)
  server.on("/redirect",        [](AsyncWebServerRequest *r){ r->redirect(PORTAL_URL); });           // MS
  server.on("/hotspot-detect.html",[](AsyncWebServerRequest *r){ r->redirect(PORTAL_URL); });        // Apple
  server.on("/library/test/success.html",[](AsyncWebServerRequest *r){ r->redirect(PORTAL_URL); });  // Apple
  server.on("/canonical.html",  [](AsyncWebServerRequest *r){ r->redirect(PORTAL_URL); });           // Firefox
  server.on("/success.txt",     [](AsyncWebServerRequest *r){ r->send(200); });                      // Firefox
  server.on("/ncsi.txt",        [](AsyncWebServerRequest *r){ r->redirect(PORTAL_URL); });           // Win
  server.on("/favicon.ico",     [](AsyncWebServerRequest *r){ r->send(404); });

  // ---- The portal itself ----
  server.on("/", HTTP_ANY, [](AsyncWebServerRequest *r){
    r->send(200, "text/html; charset=utf-8", pageTerms());
  });

  // ---- Signature submit (plain POST, no JS/cookies needed) ----
  server.on("/sign", HTTP_POST, [](AsyncWebServerRequest *r){
    String name = r->hasParam("name", true) ? r->getParam("name", true)->value() : "";
    bool agreed = r->hasParam("agree", true);
    name.trim();
    if (!agreed || name.length() < 2) {
      r->send(200, "text/html; charset=utf-8",
              pageTerms("Please enter your name and tick the box to agree."));
      return;
    }
    recordSignature(name, r->client()->remoteIP());
    r->send(200, "text/html; charset=utf-8", pageCode(name));
  });

  // ---- Admin: read the signature log ----  GET /log?key=ADMIN_KEY
  server.on("/log", HTTP_GET, [](AsyncWebServerRequest *r){
    if (!r->hasParam("key") || r->getParam("key")->value() != ADMIN_KEY) {
      r->send(403, "text/plain", "forbidden"); return;
    }
    if (!LittleFS.exists(SIG_FILE)) { r->send(200, "text/plain", "id,uptime_ms,ip,name\n"); return; }
    AsyncWebServerResponse *resp = r->beginResponse(LittleFS, SIG_FILE, "text/csv");
    resp->addHeader("Content-Disposition", "inline; filename=signatures.csv");
    r->send(resp);
  });

  // ---- Everything else -> back to the portal (fix 3, catch-all) ----
  server.onNotFound([](AsyncWebServerRequest *r){
    Serial.printf("[HTTP] miss host=%s url=%s -> redirect\n",
                  r->host().c_str(), r->url().c_str());
    r->redirect(PORTAL_URL);
  });

  server.begin();
  Serial.println("[WEB] server up on :80");
}

// --------------------------------- main ------------------------------------

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n\n=== Terms Portal ===");

  if (!LittleFS.begin(true)) Serial.println("[FS] LittleFS mount failed (formatting)");
  else Serial.println("[FS] LittleFS ready");

  startAP();
  startDNS();
  startWeb();
  Serial.println("[READY] join the Wi-Fi and the terms page should pop up.");
}

void loop() {
  dnsServer.processNextRequest();
  delay(DNS_INTERVAL);

  static uint32_t last = 0;
  if (millis() - last > 3000) {
    last = millis();
    Serial.printf("[hb] up=%lus AP='%s' ip=%s clients=%d heap=%u\n",
                  millis() / 1000, AP_SSID, WiFi.softAPIP().toString().c_str(),
                  WiFi.softAPgetStationNum(), (unsigned)ESP.getFreeHeap());
  }
}

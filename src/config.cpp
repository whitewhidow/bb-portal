// See config.h. Backed by NVS (Preferences), namespace "cfg".
#include "config.h"
#include "led.h"
#include <Preferences.h>
#include <string.h>

// >>> YOUR APP SETTINGS <<< — add fields here; the portal auto-renders a form and
// stores each by key. type: 's' text, 'n' number, 'b' 0/1 (checkbox).
const CfgField CFG_FIELDS[] = {
  { "ssid",       "WiFi name (SSID)",    's' },   // the network residents join   (reboot to apply)
  { "code",       "Access code",         's' },   // injected into the done page as {{CODE}} (applies immediately)
  { "value1",     "Value 1",             's' },   // generic slots injected into BOTH pages as {{VALUE1}}..{{VALUE4}}
  { "value2",     "Value 2",             's' },
  { "value3",     "Value 3",             's' },
  { "value4",     "Value 4",             's' },
  { "brightness", "Brightness (0-255)",  'n' },
  { "splash",     "Boot splash",         'b' },   // default on (unset -> on; see main.cpp)
  { "led",        "Flash LED on sign",   'b' },   // default on; hidden by cfgJson on LED-less boards
};
const int CFG_FIELD_COUNT = sizeof(CFG_FIELDS) / sizeof(CFG_FIELDS[0]);

static Preferences s_p;

String cfgGet(const char* key, const char* def) {
  s_p.begin("cfg", true); String v = s_p.getString(key, def); s_p.end(); return v;
}
void cfgSet(const char* key, const char* val) {
  s_p.begin("cfg", false); s_p.putString(key, val); s_p.end();
}

static String jesc(const String& in) {
  String o; for (char c : in) { if (c == '"' || c == '\\') o += '\\'; o += c; } return o;
}
String cfgJson() {
  String s = "cfg:["; bool first = true;
  for (int i = 0; i < CFG_FIELD_COUNT; i++) {
    if (!strcmp(CFG_FIELDS[i].key, "led") && !ledHasLed()) continue;   // hide LED toggle where there's no LED
    if (!first) s += ','; first = false;
    s += "{\"k\":\""; s += CFG_FIELDS[i].key;
    s += "\",\"l\":\""; s += CFG_FIELDS[i].label;
    s += "\",\"t\":\""; s += CFG_FIELDS[i].type;
    s += "\",\"v\":\""; s += jesc(cfgGet(CFG_FIELDS[i].key)); s += "\"}";
  }
  s += "]"; return s;
}

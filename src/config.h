// Persistent settings (NVS). Declare your fields in CFG_FIELDS (config.cpp) — the
// portal renders a form from them automatically and each value is stored by key.
#pragma once
#include <Arduino.h>

struct CfgField { const char* key; const char* label; char type; };  // type: 's' text, 'n' number, 'b' 0/1

String cfgGet(const char* key, const char* def = "");
void   cfgSet(const char* key, const char* val);   // persists immediately
String cfgJson();
uint32_t cfgLedMs();   // LED sign-flash duration (ms) from config, clamped                                  // "cfg:[{k,l,t,v}...]" for the portal (one BLE notify)

extern const CfgField CFG_FIELDS[];
extern const int      CFG_FIELD_COUNT;

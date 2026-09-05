// Captive portal: open SoftAP + DNS hijack + HTTP terms/sign/code flow.
// Coexists with BLE (shared 2.4 GHz radio, time-sliced by the coex arbiter) so the
// unit is managed over the Web-Bluetooth portal while the AP is live.
//
// NOTHING about the form is hardcoded. The terms/form page ("portal") and the
// thank-you/code page ("done") are HTML documents stored in LittleFS and EDITED
// over BLE from the web portal. Whatever <input name=...> fields you put in the
// form, /sign stores every submitted value as a JSON record. Add/rename fields by
// editing the HTML — no reflash.
//
// Popup reliability (see captive.cpp): AP IP in PUBLIC space (4.3.2.1) so
// Samsung/Android don't ignore it, explicit handlers for every OS probe URL + a
// catch-all redirect, and the served form is plain HTML (the iOS captive
// mini-browser runs no JS and keeps no cookies — use HTML5 `required` for
// mandatory fields).
#pragma once
#include <Arduino.h>

void captiveBegin();     // start AP+DNS+HTTP, write default docs on first boot
void captiveTick();      // call from loop(): service DNS
void captiveRestartAp(); // re-apply the SSID from config live (no reboot needed)

// ---- editable documents ("portal" | "done") and read-only "records" ----------
size_t captiveDocSize(const String& doc);                              // bytes, 0 if missing
int    captiveDocRead(const String& doc, size_t off, uint8_t* out, size_t maxlen); // bytes read (0 = EOF)
bool   captiveDocPutBegin(const String& doc);   // "portal"/"done": open temp for overwrite
bool   captiveDocPutChunk(const uint8_t* data, size_t len);            // append raw bytes to temp
size_t captiveDocPutEnd();                       // commit temp -> live doc, returns final size

// ---- submitted records --------------------------------------------------------
uint32_t captiveRecordCount();
void     captiveRecordClear();

// BLE control service: a custom GATT service the phone portal writes commands to
// (RX) and the board notifies replies on (TX). Built-in commands cover version,
// WiFi provisioning, self-update, and status; the app adds its own via
// appHandleCommand(). Handles the MAC + notify gotchas — see ble_control.cpp.
#pragma once
#include <Arduino.h>

void        bleBegin(const char* advName);  // sets a distinct MAC, starts NimBLE + the control service
void        bleTick();                      // call from loop(): process queued commands + push status
void        bleNotify(const char* line);    // board -> portal (ONE line per call)
bool        bleConnected();                 // is a portal connected?
const char* bleMac();

// The app implements this (app.cpp) to handle its OWN "__CMD__" writes. Return true
// if you handled it. Built-in commands are tried first, so you can't shadow them.
bool        appHandleCommand(const char* cmd);

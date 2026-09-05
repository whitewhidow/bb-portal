// Battery level, 0-100. Boards with no battery hardware report 100 ("powered/full").
// To enable a real reading on a board, define APP_BATT_ADC (and optionally APP_BATT_DIV,
// the resistor-divider ratio) in board.h — see battery.cpp for the mapping.
#pragma once

int batteryPct();

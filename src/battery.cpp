// See battery.h. Default: no battery hardware -> report 100 ("powered/full").
// A board enables a real reading by defining APP_BATT_ADC (the ADC pin) in board.h,
// and optionally APP_BATT_DIV (external divider ratio, default 2.0 for a 1:1 divider).
#include "battery.h"
#include "board.h"
#include <Arduino.h>

int batteryPct() {
#ifdef APP_BATT_ADC
  #ifndef APP_BATT_DIV
  #  define APP_BATT_DIV 2.0f
  #endif
  // battery mV = pin mV * divider ratio. Map a Li-ion window (3.30V empty..4.20V full)
  // to 0..100. Adjust the window / add smoothing for your board if needed.
  int mv  = (int)(analogReadMilliVolts(APP_BATT_ADC) * (APP_BATT_DIV));
  int pct = (mv - 3300) * 100 / (4200 - 3300);
  return pct < 0 ? 0 : pct > 100 ? 100 : pct;
#else
  return 100;   // no battery on this board
#endif
}

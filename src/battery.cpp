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
  // The ADC is noisy: cache the result ~8s and average a few samples, so the % doesn't
  // jitter across buckets (which would spam the st: status notify every loop). battery
  // mV = pin mV * divider ratio, mapped over a Li-ion window (3.30V empty..4.20V full).
  static int cached = -1; static uint32_t last = 0;
  if (cached >= 0 && millis() - last < 8000) return cached;
  last = millis();
  long acc = 0; for (int i = 0; i < 8; i++) acc += analogReadMilliVolts(APP_BATT_ADC);
  int mv  = (int)((acc / 8) * (APP_BATT_DIV));
  int pct = (mv - 3300) * 100 / (4200 - 3300);
  cached = pct < 0 ? 0 : pct > 100 ? 100 : pct;
  return cached;
#else
  return 100;   // no battery on this board
#endif
}

// See clock.h.
#include "clock.h"

static uint32_t s_base = 0;   // epoch at the moment the phone set it
static uint32_t s_atMs = 0;   // millis() at that moment

void clockSet(uint32_t epochSecs) { s_base = epochSecs; s_atMs = millis(); }
uint32_t clockNow() { return s_base ? s_base + (millis() - s_atMs) / 1000 : 0; }

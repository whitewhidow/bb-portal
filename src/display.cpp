// See display.h. Headless boards get no-op stubs (and never touch LovyanGFX).
#include "display.h"
#include "board.h"
#include "version.h"

#if !APP_HAS_DISPLAY
// ---- Headless: the BLE portal is the UI. Every draw is a no-op. -------------------
void dispBegin() {}
void dispSplash(const char*, const char*) {}
void dispCenter(const char*, const char*, uint32_t) {}
void dispStatus(bool, bool, int) {}
void dispOff() {}
void dispOn() {}

#else
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

#if defined(PANEL_ST7735)
  typedef lgfx::Panel_ST7735S AppPanel;
#else
  typedef lgfx::Panel_ST7789  AppPanel;
#endif

class LGFX : public lgfx::LGFX_Device {
  AppPanel _panel; lgfx::Bus_SPI _bus;
public:
  LGFX() {
    { auto c = _bus.config();
      c.spi_host = PANEL_SPI_HOST; c.spi_mode = 0; c.freq_write = PANEL_FREQ;
      c.pin_sclk = PIN_SCLK; c.pin_mosi = PIN_MOSI; c.pin_miso = PIN_MISO; c.pin_dc = PIN_DC;
      _bus.config(c); _panel.setBus(&_bus); }
    { auto c = _panel.config();
      c.pin_cs = PIN_CS; c.pin_rst = PIN_RST; c.panel_width = PANEL_W; c.panel_height = PANEL_H;
      c.offset_x = OFFX; c.offset_y = OFFY; c.readable = false; c.invert = true; c.rgb_order = false;
      _panel.config(c); }
    setPanel(&_panel);
    { auto c = _light.config(); c.pin_bl = PIN_BL; _light.config(c); _panel.setLight(&_light); }
  }
  lgfx::Light_PWM _light;
};
static LGFX lcd;

// Landscape usable area; smaller panels need smaller fonts.
#if PANEL_W <= 90
  static const int TS = 2, CTR_TY = 4, CTR_BY = 26, CTR_DY = 11, STAT_Y = 68;
#else
  static const int TS = 3, CTR_TY = 12, CTR_BY = 60, CTR_DY = 24, STAT_Y = 150;
#endif

void dispBegin() {
#ifdef APP_BOARD_PWR_EN
  pinMode(APP_BOARD_PWR_EN, OUTPUT); digitalWrite(APP_BOARD_PWR_EN, HIGH);
#endif
  lcd.init(); lcd.setRotation(APP_ROTATION); lcd.setBrightness(200); lcd.fillScreen(0x000000u);
}

void dispCenter(const char* header, const char* body, uint32_t color) {
  lcd.fillScreen(0x000000u); lcd.setTextWrap(false);
  int W = lcd.width();
  lcd.setTextColor(lcd.color888((color>>16)&0xFF,(color>>8)&0xFF,color&0xFF), 0x000000u);
  lcd.setTextSize(TS);
  { int x = (W - lcd.textWidth(header))/2; if (x<0) x=0; lcd.setCursor(x, CTR_TY); lcd.print(header); }
  lcd.setTextColor(lcd.color888(0xC8,0xD2,0xDA), 0x000000u); lcd.setTextSize(TS-1<1?1:TS-1);
  String b = body; int start = 0, y = CTR_BY;
  while (true) {
    int nl = b.indexOf('\n', start);
    String ln = (nl<0) ? b.substring(start) : b.substring(start, nl);
    int x = (W - lcd.textWidth(ln.c_str()))/2; if (x<0) x=0;
    lcd.setCursor(x, y); lcd.print(ln); y += CTR_DY;
    if (nl<0) break; start = nl+1;
  }
}

// Boot splash — a small graphical intro that scales to the panel (works from the
// 80px T-Dongle up to the 320px C5s): faint scanlines, a decorative block row with
// one accent, the APP_NAME title (drop shadow, auto-shrunk to fit the width), a
// subtitle, an accent underline on big screens, and a version/board footer.
void dispSplash(const char* version, const char* board) {
  const int W = lcd.width(), H = lcd.height();
  const bool big = W >= 240;
  uint32_t cyan = lcd.color888(0x22,0xD3,0xE0), mag = lcd.color888(0xE8,0x79,0xF9);
  uint32_t dim  = lcd.color888(0x5A,0x67,0x72), dark = lcd.color888(0x0B,0x2A,0x2E);
  lcd.fillScreen(0x000000u);
  for (int y = 0; y < H; y += 4) lcd.drawFastHLine(0, y, W, lcd.color888(0x0A,0x12,0x16));

  // decorative block row (one cyan accent), sizes scale with the panel
  int kw = big ? 18 : 10, kh = big ? 14 : 8, gap = big ? 5 : 3;
  int kn = (W - 16) / (kw + gap); if (kn > 12) kn = 12; if (kn < 1) kn = 1;
  int startx = (W - (kn*(kw+gap) - gap)) / 2, ky = (int)(H * 0.16);
  for (int i = 0; i < kn; i++)
    lcd.fillRoundRect(startx + i*(kw+gap), ky, kw, kh, 2, (i%5==2) ? cyan : dark);

  // title = APP_NAME, drop shadow, shrunk until it fits (long names on a narrow panel)
  lcd.setTextDatum(middle_center);
  int ts = big ? 4 : 2;
  while (ts > 1) { lcd.setTextSize(ts); if (lcd.textWidth(APP_NAME) <= W - 10) break; ts--; }
  lcd.setTextSize(ts);
  lcd.setTextColor(dark); lcd.drawString(APP_NAME, W/2 + 2, H/2 + 2);
  lcd.setTextColor(cyan); lcd.drawString(APP_NAME, W/2, H/2);

  // subtitle + accent underline (underline only where there's vertical room)
  lcd.setTextSize(big ? 2 : 1);
  lcd.setTextColor(mag); lcd.drawString(APP_TAGLINE, W/2, H/2 + (big ? 30 : 14));
  if (big) { int uw = (int)(W * 0.5); lcd.fillRect((W-uw)/2, H/2 + 46, uw, 2, cyan); }

  // footer: github repo (big screens only) + version/board
  lcd.setTextSize(1); lcd.setTextColor(dim); lcd.setTextDatum(bottom_center);
  if (big) lcd.drawString(APP_GH_OWNER "/" APP_GH_REPO, W/2, H - 14);
  char f[48]; snprintf(f, sizeof(f), "v%s  -  %s", version, board);
  lcd.drawString(f, W/2, H - 3);
  lcd.setTextDatum(top_left);
}

void dispStatus(bool ble, bool wifi, int batt) {
  int W = lcd.width();
  lcd.fillRect(0, STAT_Y, W, 12, 0x000000u);
  lcd.setTextSize(1); lcd.setCursor(2, STAT_Y);
  lcd.setTextColor(ble  ? lcd.color888(0x3F,0xB9,0x50) : lcd.color888(0x5A,0x63,0x6B), 0); lcd.print("BLE ");
  lcd.setTextColor(wifi ? lcd.color888(0x3F,0xB9,0x50) : lcd.color888(0x5A,0x63,0x6B), 0); lcd.print("WIFI ");
  if (batt >= 0) { lcd.setTextColor(lcd.color888(0xC8,0xD2,0xDA), 0); lcd.printf("%d%%", batt); }
}

void dispOff() { lcd.setBrightness(0); lcd.sleep(); }
void dispOn()  { lcd.wakeup(); lcd.setBrightness(200); }
#endif

// LVGL render test for the SenseCAP Indicator — mock Fuel Finder UI.
//
// Reuses the verified display config (Arduino_GFX RGB + PCA9535 + bounce buffer,
// see tools/esp32-display-test). Draws a header + a station list + a geo: QR code
// with dummy data, to validate LVGL 8.4 and lv_qrcode before porting this UI into
// the main app's ui/ module.

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <PCA95x5.h>
#include <Wire.h>
#include <lvgl.h>

// ---- Display wiring (verified) ----
#define LCD_SCK 41
#define LCD_MOSI 48
#define LCD_BL 45
#define I2C_SDA 39
#define I2C_SCL 40
#define SCREEN_W 480
#define SCREEN_H 480

static PCA9535 ioex;  // P04 = LCD CS, P05 = LCD RST

static Arduino_DataBus *bus =
    new Arduino_SWSPI(GFX_NOT_DEFINED, GFX_NOT_DEFINED, LCD_SCK, LCD_MOSI, GFX_NOT_DEFINED);

static Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
    18, 17, 16, 21,
    4, 3, 2, 1, 0,
    10, 9, 8, 7, 6, 5,
    15, 14, 13, 12, 11,
    1, 10, 8, 50, 1, 10, 8, 20,
    1 /* pclk_active_neg */, 14000000 /* 14 MHz */, false, 0, 0,
    SCREEN_W * 20 /* bigger bounce buffer for bandwidth headroom */);

// Custom ST7701 init = Arduino_GFX's type1 sequence + the SDIR command (0xC7,0x04)
// in BK0, which flips the panel 180° in HARDWARE. The panel is mounted upside-down
// in the case; doing the flip in the panel (not via software rotation) keeps text
// sharp — software rotation smears partial flushes. (Per Arduino_GFX discussion #334
// for this exact board.) So Arduino_RGB_Display rotation stays 0.
static const uint8_t st7701_indicator_init[] = {
    BEGIN_WRITE,
    WRITE_COMMAND_8, 0xFF,
    WRITE_BYTES, 5, 0x77, 0x01, 0x00, 0x00, 0x10,  // BK0
    WRITE_C8_D16, 0xC0, 0x3B, 0x00,
    WRITE_C8_D16, 0xC1, 0x0D, 0x02,
    WRITE_C8_D16, 0xC2, 0x31, 0x05,
    WRITE_C8_D8, 0xCD, 0x08,
    WRITE_C8_D8, 0xC7, 0x04,  // SDIR: 180° hardware flip
    WRITE_COMMAND_8, 0xB0,
    WRITE_BYTES, 16, 0x00, 0x11, 0x18, 0x0E, 0x11, 0x06, 0x07, 0x08, 0x07, 0x22, 0x04, 0x12, 0x0F, 0xAA, 0x31, 0x18,
    WRITE_COMMAND_8, 0xB1,
    WRITE_BYTES, 16, 0x00, 0x11, 0x19, 0x0E, 0x12, 0x07, 0x08, 0x08, 0x08, 0x22, 0x04, 0x11, 0x11, 0xA9, 0x32, 0x18,
    WRITE_COMMAND_8, 0xFF,
    WRITE_BYTES, 5, 0x77, 0x01, 0x00, 0x00, 0x11,  // BK1
    WRITE_C8_D8, 0xB0, 0x60,
    WRITE_C8_D8, 0xB1, 0x32,
    WRITE_C8_D8, 0xB2, 0x07,
    WRITE_C8_D8, 0xB3, 0x80,
    WRITE_C8_D8, 0xB5, 0x49,
    WRITE_C8_D8, 0xB7, 0x85,
    WRITE_C8_D8, 0xB8, 0x21,
    WRITE_C8_D8, 0xC1, 0x78,
    WRITE_C8_D8, 0xC2, 0x78,
    WRITE_COMMAND_8, 0xE0,
    WRITE_BYTES, 3, 0x00, 0x1B, 0x02,
    WRITE_COMMAND_8, 0xE1,
    WRITE_BYTES, 11, 0x08, 0xA0, 0x00, 0x00, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x44, 0x44,
    WRITE_COMMAND_8, 0xE2,
    WRITE_BYTES, 12, 0x11, 0x11, 0x44, 0x44, 0xED, 0xA0, 0x00, 0x00, 0xEC, 0xA0, 0x00, 0x00,
    WRITE_COMMAND_8, 0xE3,
    WRITE_BYTES, 4, 0x00, 0x00, 0x11, 0x11,
    WRITE_C8_D16, 0xE4, 0x44, 0x44,
    WRITE_COMMAND_8, 0xE5,
    WRITE_BYTES, 16, 0x0A, 0xE9, 0xD8, 0xA0, 0x0C, 0xEB, 0xD8, 0xA0, 0x0E, 0xED, 0xD8, 0xA0, 0x10, 0xEF, 0xD8, 0xA0,
    WRITE_COMMAND_8, 0xE6,
    WRITE_BYTES, 4, 0x00, 0x00, 0x11, 0x11,
    WRITE_C8_D16, 0xE7, 0x44, 0x44,
    WRITE_COMMAND_8, 0xE8,
    WRITE_BYTES, 16, 0x09, 0xE8, 0xD8, 0xA0, 0x0B, 0xEA, 0xD8, 0xA0, 0x0D, 0xEC, 0xD8, 0xA0, 0x0F, 0xEE, 0xD8, 0xA0,
    WRITE_COMMAND_8, 0xEB,
    WRITE_BYTES, 7, 0x02, 0x00, 0xE4, 0xE4, 0x88, 0x00, 0x40,
    WRITE_C8_D16, 0xEC, 0x3C, 0x00,
    WRITE_COMMAND_8, 0xED,
    WRITE_BYTES, 16, 0xAB, 0x89, 0x76, 0x54, 0x02, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x20, 0x45, 0x67, 0x98, 0xBA,
    WRITE_COMMAND_8, 0xFF,
    WRITE_BYTES, 5, 0x77, 0x01, 0x00, 0x00, 0x13,
    WRITE_C8_D8, 0xE5, 0xE4,
    WRITE_COMMAND_8, 0xFF,
    WRITE_BYTES, 5, 0x77, 0x01, 0x00, 0x00, 0x00,
    WRITE_COMMAND_8, 0x21,        // IPS
    WRITE_C8_D8, 0x3A, 0x60,      // RGB565
    WRITE_COMMAND_8, 0x11,        // Sleep Out
    END_WRITE,
    DELAY, 120,
    BEGIN_WRITE,
    WRITE_COMMAND_8, 0x29,        // Display On
    END_WRITE};

// NOTE (WIP): LVGL renders on this panel but display polish is unresolved — see the
// "Known issues" section in tools/esp32-display-test/README.md / docs/hardware/display.md.
// Tried: SW rotation (rotation=2) vs HW rotation (st7701_indicator_init SDIR 0xC7,0x04);
// LV_COLOR_16_SWAP 0 vs 1; PCLK 12/14/16 MHz; bounce buffer 10/20 lines; forced vs
// one-shot redraw. Best so far: rotation=2 + type1 init + swap=0 (oriented, but text
// blurry / occasionally unstable). The custom st7701_indicator_init above is kept for
// the next attempt at hardware rotation.
static Arduino_RGB_Display *gfx = new Arduino_RGB_Display(
    SCREEN_W, SCREEN_H, rgbpanel, 2 /* software 180° (best-rendering so far) */,
    true, bus, GFX_NOT_DEFINED, st7701_type1_init_operations, sizeof(st7701_type1_init_operations));

// ---- LVGL plumbing ----
static lv_disp_draw_buf_t draw_buf;
static lv_disp_drv_t disp_drv;

static volatile uint32_t g_flushes = 0;

static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *px) {
  g_flushes++;
  const int32_t w = area->x2 - area->x1 + 1;
  const int32_t h = area->y2 - area->y1 + 1;
  // ISOLATION TEST: simplest possible blit, no 180° flip (expect upside-down).
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px, w, h);
  lv_disp_flush_ready(drv);
}

// One dummy station for the mock list.
struct Demo {
  const char *brand;
  const char *addr;
  int dist;
  int brg;
};

// Diagnostic pattern: pure R/G/B bars (to check LVGL colour order) + big white
// text on black (to check sharpness), separate from the styled UI.
static void buildDiag() {
  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

  struct { lv_color_t c; lv_coord_t y; const char *n; } bars[] = {
      {lv_color_hex(0xFF0000), 0, "RED"},
      {lv_color_hex(0x00FF00), 90, "GREEN"},
      {lv_color_hex(0x0000FF), 180, "BLUE"},
  };
  for (auto &b : bars) {
    lv_obj_t *r = lv_obj_create(scr);
    lv_obj_set_size(r, 480, 88);
    lv_obj_set_pos(r, 0, b.y);
    lv_obj_set_style_bg_color(r, b.c, 0);
    lv_obj_set_style_border_width(r, 0, 0);
    lv_obj_set_style_radius(r, 0, 0);
    lv_obj_t *l = lv_label_create(r);
    lv_label_set_text(l, b.n);
    lv_obj_set_style_text_color(l, lv_color_white(), 0);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_28, 0);
    lv_obj_center(l);
  }
  lv_obj_t *big = lv_label_create(scr);
  lv_label_set_text(big, "Sharp? 12345");
  lv_obj_set_style_text_color(big, lv_color_white(), 0);
  lv_obj_set_style_text_font(big, &lv_font_montserrat_28, 0);
  lv_obj_align(big, LV_ALIGN_BOTTOM_MID, 0, -40);
}

static void buildUI() {
  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x0E1116), 0);

  // Header
  lv_obj_t *title = lv_label_create(scr);
  lv_label_set_text(title, "Fuel ahead");
  lv_obj_set_style_text_color(title, lv_color_hex(0x4FC3F7), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 14, 10);

  // Station list
  static const Demo demos[] = {
      {"Shell", "1234 N Dale Mabry Hwy", 859, 166},
      {"Chevron", "901 S Orange Ave", 869, 163},
      {"7-Eleven", "83 E Colonial Dr", 1688, 6},
      {"BP", "520 S OBT", 1838, 262},
  };
  lv_obj_t *list = lv_list_create(scr);
  lv_obj_set_size(list, 452, 250);
  lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 56);
  lv_obj_set_style_bg_color(list, lv_color_hex(0x171C22), 0);
  lv_obj_set_style_border_width(list, 0, 0);
  for (auto &d : demos) {
    char txt[80];
    snprintf(txt, sizeof(txt), "%-9s  %4dm  %3d\xC2\xB0\n%s", d.brand, d.dist, d.brg, d.addr);
    lv_obj_t *btn = lv_list_add_btn(list, LV_SYMBOL_GPS, txt);
    lv_obj_set_style_text_font(btn, &lv_font_montserrat_18, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x171C22), 0);
    lv_obj_set_style_text_color(btn, lv_color_hex(0xE6E6E6), 0);
  }

  // geo: QR for the top station (the passenger-scan UX)
  lv_obj_t *qr = lv_qrcode_create(scr, 120, lv_color_black(), lv_color_white());
  const char *geo = "geo:27.9612,-82.5051";
  lv_qrcode_update(qr, geo, strlen(geo));
  lv_obj_set_style_border_color(qr, lv_color_white(), 0);
  lv_obj_set_style_border_width(qr, 4, 0);
  lv_obj_align(qr, LV_ALIGN_BOTTOM_MID, 0, -14);

  lv_obj_t *cap = lv_label_create(scr);
  lv_label_set_text(cap, "Scan for Maps / GasBuddy");
  lv_obj_set_style_text_color(cap, lv_color_hex(0x9AA0A6), 0);
  lv_obj_align(cap, LV_ALIGN_BOTTOM_MID, 0, -150);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n# LVGL mock UI test");

  // Panel reset + CS via the PCA9535.
  Wire.begin(I2C_SDA, I2C_SCL, 400000);
  ioex.attach(Wire);
  ioex.polarity(PCA95x5::Polarity::ORIGINAL_ALL);
  ioex.direction(0x0000);
  ioex.write(PCA95x5::Port::P05, PCA95x5::Level::H);
  delay(20);
  ioex.write(PCA95x5::Port::P05, PCA95x5::Level::L);
  delay(20);
  ioex.write(PCA95x5::Port::P05, PCA95x5::Level::H);
  delay(50);
  ioex.write(PCA95x5::Port::P04, PCA95x5::Level::L);

  if (!gfx->begin()) Serial.println("[disp] gfx->begin() FAILED");
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);

  // ISOLATION PROBE: direct Arduino_GFX fills (no LVGL) in THIS build/config.
  Serial.println("[probe] fillScreen RED");
  gfx->fillScreen(RGB565_RED);
  delay(1200);
  Serial.println("[probe] fillScreen GREEN");
  gfx->fillScreen(RGB565_GREEN);
  delay(1200);
  Serial.println("[probe] fillScreen BLUE");
  gfx->fillScreen(RGB565_BLUE);
  delay(1200);

  lv_init();
  lv_color_t *buf = (lv_color_t *)heap_caps_malloc(
      SCREEN_W * 60 * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  lv_disp_draw_buf_init(&draw_buf, buf, NULL, SCREEN_W * 60);
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = SCREEN_W;
  disp_drv.ver_res = SCREEN_H;
  disp_drv.flush_cb = flush_cb;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);  // 180° handled manually in flush_cb

  buildUI();  // the real mock UI (header + station list + geo: QR)
  Serial.println("[lvgl] UI built");
}

void loop() {
  lv_timer_handler();
  delay(5);
  static uint32_t last = 0, lastCount = 0;
  if (millis() - last > 1000) {
    last = millis();
    Serial.printf("[lvgl] flushes/s = %u\n", g_flushes - lastCount);
    lastCount = g_flushes;
    // No forced redraw: the UI is static, draw it once and let it persist (forcing
    // a full redraw each second tore the image -> "trembling" letters).
  }
}

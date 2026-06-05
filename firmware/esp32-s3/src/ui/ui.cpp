// LVGL UI for the Indicator Fuel Finder — list view + geo: QR detail, on the real
// ST7701S 480x480 RGB panel via esp_lcd.
//
// The display backend is the clean path ported from Seeed's ESP-IDF SDK and proven
// in tools/esp32-esplcd-lvgl: an esp_lcd RGB panel with a DOUBLE framebuffer +
// refresh-on-demand + vsync, the two framebuffers wired straight to LVGL as its two
// draw buffers (full-refresh). The ST7701 is init'd over a bit-banged 3-wire SPI with
// CS/RST on a PCA9535 I2C expander. All values are documented in
// docs/hardware/display.md ("Port spec"). This replaces the old serial-log stub.
//
// Threading: every LVGL call here runs from loop() (begin/showStations/tick are all
// called from the single Arduino task). The only other task is lcdRefreshTask, which
// touches esp_lcd but never LVGL — so LVGL stays single-threaded and lock-free.

#include "ui.h"

#include <Arduino.h>
#include <PCA95x5.h>
#include <Wire.h>
#include <lvgl.h>

#include <algorithm>

#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

// The geekendzone.com logo (GZ monogram), generated as an LVGL image in gz_logo.c.
extern "C" const lv_img_dsc_t gz_logo;

// ---- Panel wiring (docs/hardware/display.md) ----
#define LCD_SCK 41
#define LCD_MOSI 48
#define LCD_BL 45
#define I2C_SDA 39
#define I2C_SCL 40
#define SCREEN_W 480
#define SCREEN_H 480

namespace {

// PCA9535 I2C port expander (addr 0x20): P04 = ST7701 CS, P05 = ST7701 RST.
PCA9535 ioex;
inline void csLevel(bool hi) {
  ioex.write(PCA95x5::Port::P04, hi ? PCA95x5::Level::H : PCA95x5::Level::L);
}
inline void rstLevel(bool hi) {
  ioex.write(PCA95x5::Port::P05, hi ? PCA95x5::Level::H : PCA95x5::Level::L);
}

// 3-wire 9-bit bit-bang SPI: 1 D/C bit (0=cmd, 1=data) + 8 data bits, MSB first,
// sampled on the rising edge. CS framed low per word on the expander.
inline void clockBit(uint8_t bit) {
  digitalWrite(LCD_MOSI, bit ? HIGH : LOW);
  digitalWrite(LCD_SCK, LOW);
  delayMicroseconds(1);
  digitalWrite(LCD_SCK, HIGH);
  delayMicroseconds(1);
}
void spi9(uint8_t dc, uint8_t value) {
  csLevel(false);
  clockBit(dc);
  for (int i = 7; i >= 0; i--) clockBit((value >> i) & 0x01);
  csLevel(true);
}
inline void writeCommand(uint8_t cmd) { spi9(0, cmd); }
inline void writeData(uint8_t data) { spi9(1, data); }

// ST7701S init, transcribed verbatim from Seeed's lcd_panel_st7701s_init.
enum { CMD = 0xF0, DAT = 0xF1, DLY = 0xF2, END = 0xFF };
const uint8_t ST7701_INIT[] = {
    CMD, 0xFF, DAT, 0x77, DAT, 0x01, DAT, 0x00, DAT, 0x00, DAT, 0x10,
    CMD, 0xC0, DAT, 0x3B, DAT, 0x00,
    CMD, 0xC1, DAT, 0x0D, DAT, 0x02,
    CMD, 0xC2, DAT, 0x31, DAT, 0x05,
    CMD, 0xC7, DAT, 0x04,
    CMD, 0xCD, DAT, 0x08,
    CMD, 0xB0, DAT, 0x00, DAT, 0x11, DAT, 0x18, DAT, 0x0E, DAT, 0x11, DAT, 0x06,
    DAT, 0x07, DAT, 0x08, DAT, 0x07, DAT, 0x22, DAT, 0x04, DAT, 0x12, DAT, 0x0F,
    DAT, 0xAA, DAT, 0x31, DAT, 0x18,
    CMD, 0xB1, DAT, 0x00, DAT, 0x11, DAT, 0x19, DAT, 0x0E, DAT, 0x12, DAT, 0x07,
    DAT, 0x08, DAT, 0x08, DAT, 0x08, DAT, 0x22, DAT, 0x04, DAT, 0x11, DAT, 0x11,
    DAT, 0xA9, DAT, 0x32, DAT, 0x18,
    CMD, 0xFF, DAT, 0x77, DAT, 0x01, DAT, 0x00, DAT, 0x00, DAT, 0x11,
    CMD, 0xB0, DAT, 0x60,
    CMD, 0xB1, DAT, 0x32,
    CMD, 0xB2, DAT, 0x07,
    CMD, 0xB3, DAT, 0x80,
    CMD, 0xB5, DAT, 0x49,
    CMD, 0xB7, DAT, 0x85,
    CMD, 0xB8, DAT, 0x21,
    CMD, 0xC1, DAT, 0x78,
    CMD, 0xC2, DAT, 0x78,
    DLY, 20,
    CMD, 0xE0, DAT, 0x00, DAT, 0x1B, DAT, 0x02,
    CMD, 0xE1, DAT, 0x08, DAT, 0xA0, DAT, 0x00, DAT, 0x00, DAT, 0x07, DAT, 0xA0,
    DAT, 0x00, DAT, 0x00, DAT, 0x00, DAT, 0x44, DAT, 0x44,
    CMD, 0xE2, DAT, 0x11, DAT, 0x11, DAT, 0x44, DAT, 0x44, DAT, 0xED, DAT, 0xA0,
    DAT, 0x00, DAT, 0x00, DAT, 0xEC, DAT, 0xA0, DAT, 0x00, DAT, 0x00,
    CMD, 0xE3, DAT, 0x00, DAT, 0x00, DAT, 0x11, DAT, 0x11,
    CMD, 0xE4, DAT, 0x44, DAT, 0x44,
    CMD, 0xE5, DAT, 0x0A, DAT, 0xE9, DAT, 0xD8, DAT, 0xA0, DAT, 0x0C, DAT, 0xEB,
    DAT, 0xD8, DAT, 0xA0, DAT, 0x0E, DAT, 0xED, DAT, 0xD8, DAT, 0xA0, DAT, 0x10,
    DAT, 0xEF, DAT, 0xD8, DAT, 0xA0,
    CMD, 0xE6, DAT, 0x00, DAT, 0x00, DAT, 0x11, DAT, 0x11,
    CMD, 0xE7, DAT, 0x44, DAT, 0x44,
    CMD, 0xE8, DAT, 0x09, DAT, 0xE8, DAT, 0xD8, DAT, 0xA0, DAT, 0x0B, DAT, 0xEA,
    DAT, 0xD8, DAT, 0xA0, DAT, 0x0D, DAT, 0xEC, DAT, 0xD8, DAT, 0xA0, DAT, 0x0F,
    DAT, 0xEE, DAT, 0xD8, DAT, 0xA0,
    CMD, 0xEB, DAT, 0x02, DAT, 0x00, DAT, 0xE4, DAT, 0xE4, DAT, 0x88, DAT, 0x00,
    DAT, 0x40,
    CMD, 0xEC, DAT, 0x3C, DAT, 0x00,
    CMD, 0xED, DAT, 0xAB, DAT, 0x89, DAT, 0x76, DAT, 0x54, DAT, 0x02, DAT, 0xFF,
    DAT, 0xFF, DAT, 0xFF, DAT, 0xFF, DAT, 0xFF, DAT, 0xFF, DAT, 0x20, DAT, 0x45,
    DAT, 0x67, DAT, 0x98, DAT, 0xBA,
    CMD, 0x36, DAT, 0x10,
    CMD, 0xFF, DAT, 0x77, DAT, 0x01, DAT, 0x00, DAT, 0x00, DAT, 0x13,
    CMD, 0xE5, DAT, 0xE4,
    CMD, 0xFF, DAT, 0x77, DAT, 0x01, DAT, 0x00, DAT, 0x00, DAT, 0x00,
    CMD, 0x3A, DAT, 0x60,
    CMD, 0x21,
    CMD, 0x11,
    DLY, 120,
    CMD, 0x29,
    DLY, 120,
    END};

void runST7701Init() {
  for (size_t i = 0; i < sizeof(ST7701_INIT);) {
    uint8_t op = ST7701_INIT[i++];
    if (op == END) break;
    uint8_t arg = ST7701_INIT[i++];
    if (op == CMD) writeCommand(arg);
    else if (op == DAT) writeData(arg);
    else if (op == DLY) delay(arg);
  }
}

// ---- esp_lcd RGB panel (double FB + bounce buffer + vsync) ----
//
// The framebuffers live in PSRAM, but at 80 MHz PSRAM (the precompiled Arduino core
// default) the RGB scanout starves when Wi-Fi/TLS saturates the PSRAM bus -> the panel
// shows "bad-TV" noise. Seeed's own firmware avoids this by running PSRAM at 120 MHz
// (CONFIG_SPIRAM_SPEED_120M), which we can't set on the precompiled core. The
// Espressif-documented alternative is a BOUNCE BUFFER: the driver DMAs the framebuffer
// into small internal-SRAM buffers ahead of scanout, so a busy PSRAM bus no longer
// corrupts the image. We keep num_fbs=2 for tear-free double buffering. Bounce buffer
// requires the panel to free-run, so refresh_on_demand is OFF (no manual refresh task).
esp_lcd_panel_handle_t panel = nullptr;
SemaphoreHandle_t sem_gui_ready;  // given on vsync (a framebuffer swap completed)
void *fb0 = nullptr;
void *fb1 = nullptr;

IRAM_ATTR bool onVsync(esp_lcd_panel_handle_t p,
                       const esp_lcd_rgb_panel_event_data_t *e, void *ctx) {
  BaseType_t hp = pdFALSE;
  xSemaphoreGiveFromISR(sem_gui_ready, &hp);
  return hp == pdTRUE;
}

void initRGBPanel() {
  esp_lcd_rgb_panel_config_t cfg = {};
  cfg.clk_src = LCD_CLK_SRC_PLL160M;
  // 16 MHz (down from Seeed's 18): a little less scanout bandwidth = more headroom for
  // the bounce-buffer refill under Wi-Fi PSRAM contention. ~56 Hz, still flicker-free.
  cfg.timings.pclk_hz = 16 * 1000 * 1000;
  cfg.timings.h_res = SCREEN_W;
  cfg.timings.v_res = SCREEN_H;
  cfg.timings.hsync_pulse_width = 8;
  cfg.timings.hsync_back_porch = 50;
  cfg.timings.hsync_front_porch = 10;
  cfg.timings.vsync_pulse_width = 8;
  cfg.timings.vsync_back_porch = 20;
  cfg.timings.vsync_front_porch = 10;
  cfg.timings.flags.pclk_active_neg = 0;
  cfg.data_width = 16;
  cfg.bits_per_pixel = 16;
  cfg.num_fbs = 2;
  // Bounce buffer = 20 lines. 480 % 20 == 0, ~38 KB internal SRAM for both halves.
  // This is what makes the panel survive Wi-Fi/TLS PSRAM contention.
  cfg.bounce_buffer_size_px = SCREEN_W * 20;
  cfg.hsync_gpio_num = 16;
  cfg.vsync_gpio_num = 17;
  cfg.de_gpio_num = 18;
  cfg.pclk_gpio_num = 21;
  cfg.disp_gpio_num = -1;
  const int dpins[16] = {15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
  for (int i = 0; i < 16; i++) cfg.data_gpio_nums[i] = dpins[i];
  cfg.flags.fb_in_psram = 1;
  cfg.flags.bb_invalidate_cache = 1;  // keep bounce reads coherent with CPU writes

  ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&cfg, &panel));
  esp_lcd_rgb_panel_event_callbacks_t cbs = {};
  cbs.on_vsync = onVsync;
  ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(panel, &cbs, nullptr));
  ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
  ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
  ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(panel, 2, &fb0, &fb1));

  sem_gui_ready = xSemaphoreCreateBinary();
}

// ---- LVGL plumbing ----
lv_disp_draw_buf_t draw_buf;
lv_disp_drv_t disp_drv;

void flushCb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
  esp_lcd_panel_draw_bitmap(panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1,
                            color_p);
  xSemaphoreTake(sem_gui_ready, portMAX_DELAY);
  lv_disp_flush_ready(drv);
}

// ---- Screen contents (persistent widgets, updated in place) ----
inline lv_color_t COL_BG() { return lv_color_hex(0x0E1116); }
inline lv_color_t COL_CARD() { return lv_color_hex(0x171C22); }
inline lv_color_t COL_ACCENT() { return lv_color_hex(0x76B900); }  // GeekendZone green
inline lv_color_t COL_TEXT() { return lv_color_hex(0xE6E6E6); }
inline lv_color_t COL_MUTED() { return lv_color_hex(0x9AA0A6); }

lv_obj_t *s_list = nullptr;
lv_obj_t *s_status = nullptr;
lv_obj_t *s_qr = nullptr;
lv_obj_t *s_qrcap = nullptr;
bool s_ready = false;

void buildBase() {
  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, COL_BG(), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

  // Header (top-left).
  lv_obj_t *title = lv_label_create(scr);
  lv_label_set_text(title, "Fuel ahead");
  lv_obj_set_style_text_color(title, lv_color_hex(0x4FC3F7), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 14, 10);

  // geekendzone.com logo (top-right).
  lv_obj_t *logo = lv_img_create(scr);
  lv_img_set_src(logo, &gz_logo);
  lv_obj_align(logo, LV_ALIGN_TOP_RIGHT, -10, 6);

  // Status line under the header.
  s_status = lv_label_create(scr);
  lv_label_set_text(s_status, "Connecting...");
  lv_obj_set_style_text_color(s_status, COL_MUTED(), 0);
  lv_obj_set_style_text_font(s_status, &lv_font_montserrat_14, 0);
  lv_obj_align(s_status, LV_ALIGN_TOP_LEFT, 16, 46);

  // Station list — a plain flex column (not lv_list) so each row can stack a big
  // brand/distance line over a small address line, with no scrolling (there's no
  // touch yet, and you don't scroll while driving).
  s_list = lv_obj_create(scr);
  lv_obj_set_size(s_list, 452, 238);  // snug for 5 two-line rows
  lv_obj_align(s_list, LV_ALIGN_TOP_MID, 0, 66);
  lv_obj_set_style_bg_color(s_list, COL_CARD(), 0);
  lv_obj_set_style_border_width(s_list, 0, 0);
  lv_obj_set_style_radius(s_list, 8, 0);
  lv_obj_set_style_pad_all(s_list, 8, 0);
  lv_obj_set_style_pad_row(s_list, 6, 0);
  lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
  lv_obj_clear_flag(s_list, LV_OBJ_FLAG_SCROLLABLE);

  // geo: QR for the nearest station (passenger-scan UX), bottom-centre.
  s_qr = lv_qrcode_create(scr, 104, lv_color_black(), lv_color_white());
  lv_obj_set_style_border_color(s_qr, lv_color_white(), 0);
  lv_obj_set_style_border_width(s_qr, 4, 0);
  lv_obj_align(s_qr, LV_ALIGN_BOTTOM_MID, 0, -10);
  lv_obj_add_flag(s_qr, LV_OBJ_FLAG_HIDDEN);

  s_qrcap = lv_label_create(scr);
  lv_label_set_text(s_qrcap, "Scan nearest for Maps / GasBuddy");
  lv_obj_set_style_text_color(s_qrcap, COL_MUTED(), 0);
  lv_obj_set_style_text_font(s_qrcap, &lv_font_montserrat_14, 0);
  lv_obj_align(s_qrcap, LV_ALIGN_BOTTOM_MID, 0, -126);
  lv_obj_add_flag(s_qrcap, LV_OBJ_FLAG_HIDDEN);
}

// How many stations to show on-screen. A driving glance wants the few nearest ones,
// not a scrollable list of ten (there's no touch to scroll anyway). The QR is the
// nearest; the rest are in the serial log.
constexpr size_t MAX_ROWS = 5;

// 8-point compass letter for a bearing — easier to read at a glance than degrees.
const char *compass(double brg) {
  static const char *pts[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  int i = (int)((brg + 22.5) / 45.0) & 7;
  return pts[i];
}

// Distance in miles, one decimal, dropping a trailing ".0" (e.g. "0.7 mi", "1.3 mi",
// "2 mi"). Never shows "0 mi" — clamps to 0.1 when right on top of the station.
void fmtDist(double m, char *out, size_t n) {
  double mi = m / 1609.344;
  double r = round(mi * 10.0) / 10.0;
  if (r < 0.1) r = 0.1;
  if (r == (long)r) snprintf(out, n, "%ld mi", (long)r);
  else snprintf(out, n, "%.1f mi", r);
}

// Point the QR at one station, encoding geo:lat,lng (opens Maps when scanned).
void setQrTo(const Station &s) {
  char geo[48];
  snprintf(geo, sizeof(geo), "geo:%.5f,%.5f", s.lat, s.lng);
  lv_qrcode_update(s_qr, geo, strlen(geo));
  lv_obj_clear_flag(s_qr, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(s_qrcap, LV_OBJ_FLAG_HIDDEN);
}

}  // namespace

namespace ui {

bool begin() {
  // PCA9535: P04 = CS, P05 = RST. Reset pulse, then hold CS asserted for init.
  Wire.begin(I2C_SDA, I2C_SCL, 400000);
  ioex.attach(Wire);
  ioex.polarity(PCA95x5::Polarity::ORIGINAL_ALL);
  ioex.direction(0x0000);
  csLevel(true);
  rstLevel(true);
  delay(20);
  rstLevel(false);
  delay(20);
  rstLevel(true);
  delay(120);

  pinMode(LCD_SCK, OUTPUT);
  pinMode(LCD_MOSI, OUTPUT);
  digitalWrite(LCD_SCK, HIGH);
  digitalWrite(LCD_MOSI, HIGH);

  // Configure ST7701 registers before streaming RGB, then bring up the panel.
  runST7701Init();
  initRGBPanel();

  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);

  lv_init();
  lv_disp_draw_buf_init(&draw_buf, fb0, fb1, SCREEN_W * SCREEN_H);
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = SCREEN_W;
  disp_drv.ver_res = SCREEN_H;
  disp_drv.flush_cb = flushCb;
  disp_drv.draw_buf = &draw_buf;
  disp_drv.full_refresh = 1;
  lv_disp_drv_register(&disp_drv);

  buildBase();
  s_ready = true;
  Serial.println("[ui] LVGL + esp_lcd panel up.");
  return true;
}

void showStations(const std::vector<Station> &stations) {
  // Keep logging too — handy on serial while driving.
  Serial.printf("[ui] %u station(s):\n", (unsigned)stations.size());
  for (size_t i = 0; i < stations.size(); ++i) {
    const Station &s = stations[i];
    Serial.printf("  %u. %-12s %6.0f m  brg %3.0f  %s\n", (unsigned)(i + 1),
                  s.brand.c_str(), s.distanceM, s.bearingDeg, s.address.c_str());
  }
  if (!s_ready) return;

  lv_obj_clean(s_list);  // drop the previous result's rows

  if (stations.empty()) {
    lv_label_set_text(s_status, "No fuel ahead - searching...");
    lv_obj_add_flag(s_qr, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_qrcap, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  const size_t shown = std::min(stations.size(), MAX_ROWS);
  if (stations.size() > shown) {
    lv_label_set_text_fmt(s_status, "%u ahead  (showing nearest %u)",
                          (unsigned)stations.size(), (unsigned)shown);
  } else {
    lv_label_set_text_fmt(s_status, "%u ahead", (unsigned)stations.size());
  }

  for (size_t i = 0; i < shown; ++i) {
    const Station &s = stations[i];

    // One row = brand/distance/direction (big) over the full address (small, muted).
    lv_obj_t *row = lv_obj_create(s_list);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_pad_row(row, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    char dist[16];
    fmtDist(s.distanceM, dist, sizeof(dist));
    char head[64];
    snprintf(head, sizeof(head), "%-9s  %8s  %2s", s.brand.c_str(), dist,
             compass(s.bearingDeg));
    lv_obj_t *l1 = lv_label_create(row);
    lv_label_set_text(l1, head);
    lv_obj_set_style_text_font(l1, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(l1, i == 0 ? COL_ACCENT() : COL_TEXT(), 0);

    lv_obj_t *l2 = lv_label_create(row);
    lv_label_set_text(l2, s.address.c_str());
    lv_obj_set_style_text_font(l2, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(l2, COL_MUTED(), 0);
    lv_label_set_long_mode(l2, LV_LABEL_LONG_DOT);  // truncate over-long addresses
    lv_obj_set_width(l2, lv_pct(100));
  }

  setQrTo(stations.front());  // QR = the nearest one
}

void showDetail(const Station &s) {
  Serial.printf("[ui] detail: %s | %s | geo:%.6f,%.6f\n", s.brand.c_str(),
                s.address.c_str(), s.lat, s.lng);
  if (s_ready) setQrTo(s);
}

void tick() {
  if (s_ready) lv_timer_handler();
}

}  // namespace ui

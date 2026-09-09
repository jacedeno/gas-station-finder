// Clean esp_lcd bring-up of the SenseCAP Indicator's ST7701S 480x480 RGB panel,
// ported from Seeed's ESP-IDF SDK (Seeed-Solution/SenseCAP_Indicator_ESP32):
//   - peripherals/bsp_lcd.c            -> the esp_lcd RGB panel (double FB + vsync)
//   - examples/lvgl_demos/lv_port.c    -> LVGL full-refresh wiring (two FBs = draw bufs)
//   - boards/lcd_panel_config.c        -> lcd_panel_st7701s_init (SPI init via PCA9535)
// All values are documented in docs/hardware/display.md ("Port spec").
//
// Why this exists: Arduino_GFX (tools/esp32-lvgl-test) can only do a single PSRAM
// framebuffer + bounce buffer, which tore/blurred under LVGL. esp_lcd does the
// standard Espressif "two PSRAM framebuffers + refresh-on-demand + vsync" anti-tear
// pattern, and we hand those two framebuffers straight to LVGL as its draw buffers.
//
// The sketch brings the panel up in layers so each can be verified from the serial
// log + the screen:
//   1) esp_lcd RGB panel + ST7701 init  -> direct framebuffer colour fills (no LVGL)
//   2) LVGL full-refresh on top         -> the mock Fuel Finder UI + geo: QR

#include <Arduino.h>
#include <PCA95x5.h>
#include <Wire.h>
#include <lvgl.h>

#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

// ---- Wiring (docs/hardware/display.md) ----
#define LCD_SCK 41        // ST7701 3-wire SPI clock (real GPIO)
#define LCD_MOSI 48       // ST7701 3-wire SPI data/SDA (real GPIO)
#define LCD_BL 45         // backlight, active-high
#define I2C_SDA 39
#define I2C_SCL 40
#define SCREEN_W 480
#define SCREEN_H 480

// PCA9535 I2C port expander (addr 0x20): P04 = ST7701 CS, P05 = ST7701 RST.
static PCA9535 ioex;
static inline void csLevel(bool hi) {
  ioex.write(PCA95x5::Port::P04, hi ? PCA95x5::Level::H : PCA95x5::Level::L);
}
static inline void rstLevel(bool hi) {
  ioex.write(PCA95x5::Port::P05, hi ? PCA95x5::Level::H : PCA95x5::Level::L);
}

// ---- ST7701 3-wire 9-bit bit-bang SPI ----
// Frame = 1 D/C bit (0=command, 1=data) + 8 data bits, MSB first, sampled on the
// rising clock edge. CS is framed low around the whole 9-bit word (toggled on the
// expander, so once per word — fine for a one-shot init). Equivalent to the
// Arduino_SWSPI path that already clocked this panel cleanly.
static inline void clockBit(uint8_t bit) {
  digitalWrite(LCD_MOSI, bit ? HIGH : LOW);
  digitalWrite(LCD_SCK, LOW);
  delayMicroseconds(1);
  digitalWrite(LCD_SCK, HIGH);  // ST7701 latches on the rising edge
  delayMicroseconds(1);
}
static void spi9(uint8_t dc, uint8_t value) {
  csLevel(false);
  clockBit(dc);
  for (int i = 7; i >= 0; i--) clockBit((value >> i) & 0x01);
  csLevel(true);
}
static inline void writeCommand(uint8_t cmd) { spi9(0, cmd); }
static inline void writeData(uint8_t data) { spi9(1, data); }

// ST7701S init sequence, transcribed verbatim from Seeed's lcd_panel_st7701s_init.
// Encoded as a tiny opcode stream to keep the ~150-entry sequence readable.
enum { CMD = 0xF0, DAT = 0xF1, DLY = 0xF2, END = 0xFF };
static const uint8_t ST7701_INIT[] = {
    // ---- BK0 (command2 bank 0) ----
    CMD, 0xFF, DAT, 0x77, DAT, 0x01, DAT, 0x00, DAT, 0x00, DAT, 0x10,
    CMD, 0xC0, DAT, 0x3B, DAT, 0x00,                       // 480x480
    CMD, 0xC1, DAT, 0x0D, DAT, 0x02,
    CMD, 0xC2, DAT, 0x31, DAT, 0x05,
    CMD, 0xC7, DAT, 0x04,                                  // SDIR (source direction)
    CMD, 0xCD, DAT, 0x08,
    CMD, 0xB0, DAT, 0x00, DAT, 0x11, DAT, 0x18, DAT, 0x0E, DAT, 0x11, DAT, 0x06,
    DAT, 0x07, DAT, 0x08, DAT, 0x07, DAT, 0x22, DAT, 0x04, DAT, 0x12, DAT, 0x0F,
    DAT, 0xAA, DAT, 0x31, DAT, 0x18,
    CMD, 0xB1, DAT, 0x00, DAT, 0x11, DAT, 0x19, DAT, 0x0E, DAT, 0x12, DAT, 0x07,
    DAT, 0x08, DAT, 0x08, DAT, 0x08, DAT, 0x22, DAT, 0x04, DAT, 0x11, DAT, 0x11,
    DAT, 0xA9, DAT, 0x32, DAT, 0x18,
    // ---- BK1 (command2 bank 1) ----
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
    CMD, 0x36, DAT, 0x10,                                  // MADCTL (orientation, in HW)
    // ---- bank 3 tweak then back to user bank ----
    CMD, 0xFF, DAT, 0x77, DAT, 0x01, DAT, 0x00, DAT, 0x00, DAT, 0x13,
    CMD, 0xE5, DAT, 0xE4,
    CMD, 0xFF, DAT, 0x77, DAT, 0x01, DAT, 0x00, DAT, 0x00, DAT, 0x00,
    CMD, 0x3A, DAT, 0x60,                                  // RGB666 pixel format
    CMD, 0x21,                                             // display inversion on
    CMD, 0x11,                                             // sleep out
    DLY, 120,
    CMD, 0x29,                                             // display on
    DLY, 120,
    END};

static void runST7701Init() {
  for (size_t i = 0; i < sizeof(ST7701_INIT);) {
    uint8_t op = ST7701_INIT[i++];
    if (op == END) break;
    uint8_t arg = ST7701_INIT[i++];
    if (op == CMD) writeCommand(arg);
    else if (op == DAT) writeData(arg);
    else if (op == DLY) delay(arg);
  }
}

// ---- esp_lcd RGB panel (double FB + refresh-on-demand + vsync) ----
static esp_lcd_panel_handle_t panel = nullptr;
static SemaphoreHandle_t sem_vsync_end;  // "trans_ready": a refresh may start
static SemaphoreHandle_t sem_gui_ready;  // "flush_ready": vsync fired after a draw
static void *fb0 = nullptr;
static void *fb1 = nullptr;

static IRAM_ATTR bool onVsync(esp_lcd_panel_handle_t p,
                              const esp_lcd_rgb_panel_event_data_t *e, void *ctx) {
  BaseType_t hp = pdFALSE;
  xSemaphoreGiveFromISR(sem_vsync_end, &hp);
  xSemaphoreGiveFromISR(sem_gui_ready, &hp);
  return hp == pdTRUE;
}

// Mirror of Seeed's lcd_task: on each vsync, wait the refresh period then push.
static void lcdRefreshTask(void *arg) {
  TickType_t tick;
  for (;;) {
    tick = xTaskGetCurrentTaskHandle() ? xTaskGetTickCount() : 0;
    xSemaphoreTake(sem_vsync_end, portMAX_DELAY);
    vTaskDelayUntil(&tick, pdMS_TO_TICKS(40));  // CONFIG_LCD_TASK_REFRESH_TIME
    esp_lcd_rgb_panel_refresh(panel);
    xSemaphoreTake(sem_gui_ready, 0);
  }
}

static void initRGBPanel() {
  esp_lcd_rgb_panel_config_t cfg = {};
  cfg.clk_src = LCD_CLK_SRC_PLL160M;
  cfg.timings.pclk_hz = 18 * 1000 * 1000;  // 18 MHz (Seeed default)
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
  cfg.num_fbs = 2;                 // double framebuffer (the anti-tear core)
  cfg.bounce_buffer_size_px = 0;   // no bounce buffer — not needed with double FB
  cfg.hsync_gpio_num = 16;
  cfg.vsync_gpio_num = 17;
  cfg.de_gpio_num = 18;
  cfg.pclk_gpio_num = 21;
  cfg.disp_gpio_num = -1;
  // data_gpio_nums[0..15] = B0..B4, G0..G5, R0..R4 (Seeed mapping)
  const int dpins[16] = {15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
  for (int i = 0; i < 16; i++) cfg.data_gpio_nums[i] = dpins[i];
  cfg.flags.fb_in_psram = 1;
  cfg.flags.refresh_on_demand = 1;  // we drive esp_lcd_rgb_panel_refresh() ourselves

  ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&cfg, &panel));

  esp_lcd_rgb_panel_event_callbacks_t cbs = {};
  cbs.on_vsync = onVsync;
  ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(panel, &cbs, nullptr));

  ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
  ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
  ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(panel, 2, &fb0, &fb1));

  sem_vsync_end = xSemaphoreCreateBinary();
  xSemaphoreGive(sem_vsync_end);
  sem_gui_ready = xSemaphoreCreateBinary();
  xTaskCreate(lcdRefreshTask, "lcd_refresh", 4096, nullptr, 5, nullptr);
}

// Fill a framebuffer with a solid RGB565 colour and push it (used by the layer-1
// probe, before LVGL is wired in).
static void directFill(void *fb, uint16_t color) {
  uint16_t *p = (uint16_t *)fb;
  for (int i = 0; i < SCREEN_W * SCREEN_H; i++) p[i] = color;
  esp_lcd_panel_draw_bitmap(panel, 0, 0, SCREEN_W, SCREEN_H, fb);
  xSemaphoreTake(sem_gui_ready, pdMS_TO_TICKS(200));  // wait one vsync
}

// ---- LVGL ----
static lv_disp_draw_buf_t draw_buf;
static lv_disp_drv_t disp_drv;

// Full-refresh flush: color_p IS one of the two framebuffers, so draw_bitmap does a
// zero-copy swap (and the PSRAM cache writeback); then wait one vsync and report done.
static void flushCb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
  esp_lcd_panel_draw_bitmap(panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1,
                            color_p);
  xSemaphoreTake(sem_gui_ready, portMAX_DELAY);
  lv_disp_flush_ready(drv);
}

struct Demo {
  const char *brand;
  const char *addr;
  int dist;
  int brg;
};

static void buildUI() {
  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x0E1116), 0);

  lv_obj_t *title = lv_label_create(scr);
  lv_label_set_text(title, "Fuel ahead");
  lv_obj_set_style_text_color(title, lv_color_hex(0x4FC3F7), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 14, 10);

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
    snprintf(txt, sizeof(txt), "%-9s  %4dm  %3d\xC2\xB0\n%s", d.brand, d.dist, d.brg,
             d.addr);
    lv_obj_t *btn = lv_list_add_btn(list, LV_SYMBOL_GPS, txt);
    lv_obj_set_style_text_font(btn, &lv_font_montserrat_18, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x171C22), 0);
    lv_obj_set_style_text_color(btn, lv_color_hex(0xE6E6E6), 0);
  }

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
  Serial.println("\n# esp_lcd + LVGL bring-up (Seeed port)");

  // PCA9535: P04 = CS, P05 = RST. Reset pulse, then hold CS asserted for init.
  Wire.begin(I2C_SDA, I2C_SCL, 400000);
  ioex.attach(Wire);
  ioex.polarity(PCA95x5::Polarity::ORIGINAL_ALL);
  ioex.direction(0x0000);  // all expander pins as outputs
  csLevel(true);
  rstLevel(true);
  delay(20);
  rstLevel(false);
  delay(20);
  rstLevel(true);
  delay(120);

  // 3-wire SPI pins idle high.
  pinMode(LCD_SCK, OUTPUT);
  pinMode(LCD_MOSI, OUTPUT);
  digitalWrite(LCD_SCK, HIGH);
  digitalWrite(LCD_MOSI, HIGH);

  // Order matters: configure the ST7701 registers BEFORE streaming RGB.
  Serial.println("[disp] ST7701 SPI init...");
  runST7701Init();

  Serial.println("[disp] esp_lcd RGB panel (double FB)...");
  initRGBPanel();

  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);

  // ---- Layer 1: direct framebuffer fills (no LVGL). Must be sharp + correct. ----
  Serial.println("[probe] direct fill RED / GREEN / BLUE");
  directFill(fb0, 0xF800);
  delay(1000);
  directFill(fb0, 0x07E0);
  delay(1000);
  directFill(fb0, 0x001F);
  delay(1000);

  // ---- Layer 2: LVGL full-refresh, the two framebuffers as the draw buffers. ----
  Serial.println("[lvgl] init + full-refresh draw buffers");
  lv_init();
  lv_disp_draw_buf_init(&draw_buf, fb0, fb1, SCREEN_W * SCREEN_H);
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = SCREEN_W;
  disp_drv.ver_res = SCREEN_H;
  disp_drv.flush_cb = flushCb;
  disp_drv.draw_buf = &draw_buf;
  disp_drv.full_refresh = 1;  // render the whole frame each time, alternate FBs
  lv_disp_drv_register(&disp_drv);

  buildUI();
  Serial.println("[lvgl] UI built");
}

void loop() {
  lv_timer_handler();
  delay(5);
  static uint32_t last = 0;
  if (millis() - last > 5000) {
    last = millis();
    Serial.println("[lvgl] alive");
  }
}

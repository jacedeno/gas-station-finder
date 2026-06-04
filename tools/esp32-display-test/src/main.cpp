// SenseCAP Indicator — display "first light" bring-up.
//
// ST7701S 480x480 RGB565 panel. SPI init on GPIO41(SCK)/48(MOSI); the panel CS and
// RST hang off a PCA9535 I2C expander (same I2C bus as touch: SDA=39, SCL=40).
// Pin map + Arduino_GFX constructor values are from Seeed's Arduino tutorial.
//
// Cycles solid R/G/B, then draws colour bars + text, so the exact colours reported
// back tell us if the init + RGB order + timings are right.

#include <Arduino_GFX_Library.h>
#include <PCA95x5.h>
#include <Wire.h>

#define LCD_SCK 41
#define LCD_MOSI 48
#define LCD_BL 45
#define I2C_SDA 39
#define I2C_SCL 40

// PCA9535 expander: P04 = LCD CS, P05 = LCD RST (active low).
static PCA9535 ioex;

// 3-wire SPI for the ST7701 init. CS is GFX_NOT_DEFINED here because the expander
// holds it; the init burst runs with CS asserted low on P04.
static Arduino_DataBus *bus =
    new Arduino_SWSPI(GFX_NOT_DEFINED /* DC */, GFX_NOT_DEFINED /* CS */, LCD_SCK,
                      LCD_MOSI, GFX_NOT_DEFINED /* MISO */);

static Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
    18 /* DE */, 17 /* VSYNC */, 16 /* HSYNC */, 21 /* PCLK */,
    4 /* R0 */, 3 /* R1 */, 2 /* R2 */, 1 /* R3 */, 0 /* R4 */,
    10 /* G0 */, 9 /* G1 */, 8 /* G2 */, 7 /* G3 */, 6 /* G4 */, 5 /* G5 */,
    15 /* B0 */, 14 /* B1 */, 13 /* B2 */, 12 /* B3 */, 11 /* B4 */,
    1 /* hsync_polarity */, 10 /* hsync_front_porch */, 8 /* hsync_pulse_width */,
    50 /* hsync_back_porch */, 1 /* vsync_polarity */, 10 /* vsync_front_porch */,
    8 /* vsync_pulse_width */, 20 /* vsync_back_porch */,
    1 /* pclk_active_neg: latch on falling edge */,
    16000000 /* prefer_speed: 16 MHz PCLK (~56 Hz refresh, no flicker) */,
    false /* useBigEndian */, 0 /* de_idle_high */, 0 /* pclk_idle_high */,
    480 * 10 /* bounce_buffer_size_px: SRAM bounce buffer kills PSRAM striping */);

static Arduino_RGB_Display *gfx = new Arduino_RGB_Display(
    480 /* width */, 480 /* height */, rgbpanel, 0 /* rotation */, true /* auto_flush */,
    bus, GFX_NOT_DEFINED /* RST (handled via expander) */, st7701_type1_init_operations,
    sizeof(st7701_type1_init_operations));

static void expanderResetPanel() {
  Wire.begin(I2C_SDA, I2C_SCL, 400000);
  ioex.attach(Wire);
  ioex.polarity(PCA95x5::Polarity::ORIGINAL_ALL);
  ioex.direction(0x0000);  // all expander pins as outputs (display-only bring-up)

  // Reset pulse on P05, then assert CS low on P04 for the whole init.
  ioex.write(PCA95x5::Port::P05, PCA95x5::Level::H);
  delay(20);
  ioex.write(PCA95x5::Port::P05, PCA95x5::Level::L);
  delay(20);
  ioex.write(PCA95x5::Port::P05, PCA95x5::Level::H);
  delay(50);
  ioex.write(PCA95x5::Port::P04, PCA95x5::Level::L);  // CS asserted
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n# Display first-light bring-up");

  expanderResetPanel();

  if (!gfx->begin()) {
    Serial.println("[disp] gfx->begin() FAILED");
  } else {
    Serial.println("[disp] gfx->begin() ok");
  }

  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);  // backlight on
  Serial.println("[disp] backlight on");
}

void loop() {
  const uint16_t colors[] = {RGB565_RED, RGB565_GREEN, RGB565_BLUE, RGB565_WHITE};
  const char *names[] = {"RED", "GREEN", "BLUE", "WHITE"};
  for (int i = 0; i < 4; ++i) {
    gfx->fillScreen(colors[i]);
    Serial.printf("[disp] full screen: %s\n", names[i]);
    delay(1500);
  }

  // Colour bars + text so orientation and RGB order are obvious.
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillRect(0, 0, 160, 480, RGB565_RED);
  gfx->fillRect(160, 0, 160, 480, RGB565_GREEN);
  gfx->fillRect(320, 0, 160, 480, RGB565_BLUE);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setTextSize(4);
  gfx->setCursor(20, 220);
  gfx->println("INDICATOR");
  Serial.println("[disp] colour bars: left=RED mid=GREEN right=BLUE + text");
  delay(4000);
}

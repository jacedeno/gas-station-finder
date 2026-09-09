// LVGL 8.x config for the Indicator Fuel Finder app. Mirrors the verified config
// from tools/esp32-esplcd-lvgl. Anything not set here falls back to defaults.
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0  // verified on this panel: swap=1 -> stripes/noise

#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (64U * 1024U)

// Drive LVGL's tick from Arduino millis().
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

// Fonts used by the UI.
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

// The QR widget for the station-detail screen (CLAUDE.md §5).
#define LV_USE_QRCODE 1

#define LV_USE_LOG 0

#endif  // LV_CONF_H

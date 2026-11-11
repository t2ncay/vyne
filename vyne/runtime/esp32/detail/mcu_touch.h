// runtime/esp32/detail/mcu_touch.h
//
// Touchscreen bindings on top of TFT_eSPI's built-in touch support.
// TFT_eSPI handles the XPT2046/ADS7843 resistive controller (through
// `TOUCH_CS` in User_Setup.h) and several capacitive controllers, so this
// header stays controller-agnostic.
//
// This is the pen surface: a stylus on the panel reports through these
// functions. Pressure is only meaningful on a resistive panel; on a
// capacitive panel `touch_screen_pressure()` returns 1 on contact and 0
// otherwise.
//
// Calibration data is stored in NVS under namespace "vyne_touch" so it
// survives resets. Run esp32_touch_calibrate.vy once per physical panel.
#pragma once
#include "mcu_types.h"
#include <Preferences.h>
#include <TFT_eSPI.h>

#if !defined(VYNE_USE_DISPLAY)
// When the display feature is off, mcu_display.h is not in the include
// chain, so the panel object is defined here instead.
static TFT_eSPI g_vyne_tft;
static bool g_vyne_tft_inited = false;
#endif

static uint16_t g_vyne_touch_x = 0;
static uint16_t g_vyne_touch_y = 0;
static int32_t g_vyne_touch_pressure = 0;

static inline bool _vyne_touch_poll(void) {
    uint16_t x = 0;
    uint16_t y = 0;
    uint8_t touched = g_vyne_tft.getTouch(&x, &y, 600);
    if (touched) {
        g_vyne_touch_x = x;
        g_vyne_touch_y = y;
#if defined(TOUCH_CS)
        // Resistive controllers report a Z value proportional to force.
        g_vyne_touch_pressure = (int32_t)g_vyne_tft.getTouchRawZ();
#else
        g_vyne_touch_pressure = 1;
#endif
        return true;
    }
    g_vyne_touch_pressure = 0;
    return false;
}

static inline VyneValue esp32_touch_screen_init(void) {
    if (!g_vyne_tft_inited) {
        g_vyne_tft.begin();
        g_vyne_tft_inited = true;
    }

    Preferences prefs;
    if (prefs.begin("vyne_touch", true)) {
        uint16_t calData[5] = {0};
        if (prefs.getBytes("cal", calData, sizeof(calData)) ==
            sizeof(calData)) {
            g_vyne_tft.setTouch(calData);
        }
        prefs.end();
    }
    return vyne_null();
}

static inline VyneValue esp32_touch_screen_pressed(void) {
    return vyne_bool(_vyne_touch_poll());
}

static inline VyneValue esp32_touch_screen_x(void) {
    return vyne_int((int64_t)g_vyne_touch_x);
}

static inline VyneValue esp32_touch_screen_y(void) {
    return vyne_int((int64_t)g_vyne_touch_y);
}

static inline VyneValue esp32_touch_screen_pressure(void) {
    // Resistive: raw Z value, roughly 0..4095. Capacitive: 0 or 1.
    return vyne_int((int64_t)g_vyne_touch_pressure);
}

static inline VyneValue esp32_touch_screen_read(void) {
    if (!_vyne_touch_poll())
        return vyne_int(-1);
    // Pack x, y, pressure into a single Int64:
    //   bits 32..47 = x, bits 16..31 = y, bits 0..15 = pressure.
    int64_t packed = ((int64_t)g_vyne_touch_x << 32) |
                     ((int64_t)g_vyne_touch_y << 16) |
                     (int64_t)(g_vyne_touch_pressure & 0xFFFF);
    return vyne_int(packed);
}

static inline VyneValue esp32_touch_screen_wait(void) {
    while (!_vyne_touch_poll())
        delay(10);
    int64_t packed = ((int64_t)g_vyne_touch_x << 32) |
                     ((int64_t)g_vyne_touch_y << 16) |
                     (int64_t)(g_vyne_touch_pressure & 0xFFFF);
    return vyne_int(packed);
}

static inline VyneValue esp32_touch_screen_wait_release(void) {
    while (_vyne_touch_poll())
        delay(10);
    return vyne_null();
}

static inline VyneValue esp32_touch_screen_calibrate(void) {
    // Interactive: the panel draws crosshairs and the user taps each one.
    // TFT_eSPI fills a 5-element calibration tuple, which we persist to
    // NVS under the "vyne_touch" namespace.
    uint16_t calData[5] = {0};
    g_vyne_tft.fillScreen(TFT_BLACK);
    g_vyne_tft.calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 15);
    g_vyne_tft.setTouch(calData);

    Preferences prefs;
    if (prefs.begin("vyne_touch", false)) {
        prefs.putBytes("cal", calData, sizeof(calData));
        prefs.end();
    }
    return vyne_null();
}

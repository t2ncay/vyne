// runtime/esp32/detail/mcu_touch.h
//
// Resistive touchscreen bindings. Shared-bus panels use TFT_eSPI's
// TOUCH_CS driver; VYNE_TOUCH_XPT2046 selects an independent SPI bus
// for CYD wiring. Capacitive controllers need their own driver.
//
// This is the pen surface: contact and ADC pressure on a resistive panel.
//
// Calibration data is stored in NVS under namespace "vyne_touch" so it
// survives resets. Run esp32_touch_calibrate.vy once per physical panel.
//
// With neither TOUCH_CS nor VYNE_TOUCH_XPT2046, the surface compiles as
// inert stubs. Separate-SPI calibration is stored under vyne_touch/xpt_cal;
// the existing TFT_eSPI five-word blob remains under vyne_touch/cal.
#pragma once
#include "mcu_types.h"
#include "mcu_panel.h"

#if defined(VYNE_TOUCH_XPT2046) || defined(TOUCH_CS)

#include <Preferences.h>
#include <nvs.h>
#if defined(VYNE_TOUCH_XPT2046)
#include "mcu_xpt2046.h"
#include "mcu_touch_calibration.h"
static VyneTouchCalibration g_vyne_touch_calibration = {};
static inline bool _vyne_touch_load_xpt_calibration(const char* name, VyneTouchCalibration& out) {
    out = {};
    nvs_handle_t handle;
    if (nvs_open(name, NVS_READONLY, &handle) != ESP_OK)
        return false;
    VyneTouchCalibration calibration = {};
    size_t length = sizeof(calibration);
    esp_err_t result = nvs_get_blob(handle, "xpt_cal", &calibration, &length);
    nvs_close(handle);
    if (result != ESP_OK || length != sizeof(calibration) || !vyne_touch_calibration_valid(calibration))
        return false;
    out = calibration;
    return true;
}
#endif

#if !defined(VYNE_TOUCH_XPT2046)
static bool g_vyne_shared_touch_calibrated = false;
#endif

static inline VyneValue esp32_touch_screen_configured(void) {
    return vyne_bool(true);
}

static inline VyneValue esp32_touch_screen_calibrated(void) {
#if defined(VYNE_TOUCH_XPT2046)
    return vyne_bool(vyne_touch_calibration_matches(g_vyne_touch_calibration,
        g_vyne_tft.width(), g_vyne_tft.height(), g_vyne_tft.getRotation()));
#else
    return vyne_bool(g_vyne_shared_touch_calibrated);
#endif
}

static uint16_t g_vyne_touch_x = 0;
static uint16_t g_vyne_touch_y = 0;
static int32_t g_vyne_touch_pressure = 0;

static inline bool _vyne_touch_poll(void) {
    uint16_t x = 0;
    uint16_t y = 0;
#if defined(VYNE_TOUCH_XPT2046)
    VyneTouchRaw raw;
    if (_vyne_xpt_read_raw(raw)) {
        VyneTouchCalibration calibration = g_vyne_touch_calibration;
        if (!vyne_touch_calibration_valid(calibration)) {
            // Before calibration, provide bounded ADC-normalized estimates.
            // The init message makes their uncalibrated status explicit.
            calibration = {1, (uint16_t)g_vyne_tft.width(), (uint16_t)g_vyne_tft.height(),
                (uint8_t)g_vyne_tft.getRotation(), 0, 0, 0, 4095, 0, 4095};
        }
        if (vyne_touch_calibration_map(calibration, raw.x, raw.y,
                                      g_vyne_tft.getRotation(), x, y,
                                      VYNE_DISPLAY_MIRROR_X, VYNE_DISPLAY_MIRROR_Y)) {
            g_vyne_touch_x = x;
            g_vyne_touch_y = y;
            g_vyne_touch_pressure = raw.pressure;
            return true;
        }
    }
#else
    uint8_t touched = g_vyne_tft.getTouch(&x, &y, 600);
    if (touched) {
        g_vyne_touch_x = x;
        g_vyne_touch_y = y;
        // Resistive controllers report a Z value proportional to force.
        g_vyne_touch_pressure = (int32_t)g_vyne_tft.getTouchRawZ();
        return true;
    }
#endif
    g_vyne_touch_pressure = 0;
    return false;
}

static inline VyneValue esp32_touch_screen_init(void) {
    _vyne_panel_init();
#if defined(VYNE_TOUCH_XPT2046)
    _vyne_xpt_begin();
#endif

    // Missing calibration is the normal first-boot state. Preferences
    // logs nvs_open(NOT_FOUND) as an error; read the optional blob via
    // NVS directly so an uncalibrated panel does not emit a false error.
#if defined(VYNE_TOUCH_XPT2046)
    _vyne_touch_load_xpt_calibration("vyne_touch", g_vyne_touch_calibration);
    if (!vyne_touch_calibration_matches(g_vyne_touch_calibration,
        g_vyne_tft.width(), g_vyne_tft.height(), g_vyne_tft.getRotation()))
        g_vyne_touch_calibration = {};
#else
    g_vyne_shared_touch_calibrated = false;
    nvs_handle_t handle;
    if (nvs_open("vyne_touch", NVS_READONLY, &handle) == ESP_OK) {
        uint16_t calData[5] = {0};
        size_t length = sizeof(calData);
        if (nvs_get_blob(handle, "cal", calData, &length) == ESP_OK &&
            length == sizeof(calData)) {
            g_vyne_tft.setTouch(calData);
            g_vyne_shared_touch_calibrated = true;
        }
        nvs_close(handle);
    }
#endif
#if defined(VYNE_TOUCH_XPT2046)
    ::Serial.println(vyne_touch_calibration_valid(g_vyne_touch_calibration)
        ? "XPT2046 calibration: loaded"
        : "XPT2046 calibration: absent (coordinates approximate; run touch calibration)");
#endif
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
    // Resistive: raw Z estimate (two 12-bit ADC channels, up to 8190).
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
    _vyne_panel_init();
#if defined(VYNE_TOUCH_XPT2046)
    const int inset = 20;
    const int width = g_vyne_tft.width(), height = g_vyne_tft.height();
    const int targets[4][2] = {{inset, inset}, {width - 1 - inset, inset},
        {width - 1 - inset, height - 1 - inset}, {inset, height - 1 - inset}};
    int32_t samples[4][2] = {};
    for (int corner = 0; corner < 4; ++corner) {
        VyneTouchRaw raw;
        uint32_t start = millis();
        while (_vyne_xpt_read_raw(raw, 300)) {
            if ((uint32_t)(millis() - start) >= 15000) {
                ::Serial.println("touch calibration: FAIL (release timeout)");
                return vyne_bool(false);
            }
            delay(10);
        }
        g_vyne_tft.fillScreen(TFT_BLACK);
        int x = targets[corner][0], y = targets[corner][1];
        g_vyne_tft.drawLine(x - 10, y, x + 10, y, TFT_MAGENTA);
        g_vyne_tft.drawLine(x, y - 10, x, y + 10, TFT_MAGENTA);
        g_vyne_tft.setTextColor(TFT_WHITE, TFT_BLACK);
        g_vyne_tft.setTextSize(1);
        char instruction[64];
        snprintf(instruction, sizeof(instruction), "Hold target %d/4, then release", corner + 1);
        g_vyne_tft.drawString(instruction, 35, 45);
        ::Serial.printf("touch calibration: target %d/4 (hold briefly, then release)\n", corner + 1);
        start = millis();
        int count = 0;
        while (count < 8) {
            if ((uint32_t)(millis() - start) >= 30000) {
                ::Serial.println("touch calibration: FAIL (contact timeout)");
                return vyne_bool(false);
            }
            if (_vyne_xpt_read_raw(raw, 300)) {
                if (count && (abs((int)raw.x - samples[corner][0] / count) > 100 ||
                              abs((int)raw.y - samples[corner][1] / count) > 100)) {
                    count = 0;
                    samples[corner][0] = samples[corner][1] = 0;
                }
                samples[corner][0] += raw.x;
                samples[corner][1] += raw.y;
                ++count;
            } else {
                count = 0;
                samples[corner][0] = samples[corner][1] = 0;
            }
            delay(20);
        }
        samples[corner][0] /= 8;
        samples[corner][1] /= 8;
        ::Serial.printf("touch calibration: raw x=%ld y=%ld\n",
            (long)samples[corner][0], (long)samples[corner][1]);
    }
    VyneTouchCalibration calibration = {};
    if (!vyne_touch_calibration_fit(samples, width, height, inset,
                                   g_vyne_tft.getRotation(), calibration,
                                   VYNE_DISPLAY_MIRROR_X, VYNE_DISPLAY_MIRROR_Y)) {
        ::Serial.println("touch calibration: FAIL (invalid geometry)");
        return vyne_bool(false);
    }
    Preferences prefs;
    if (!prefs.begin("vyne_touch", false))
        return vyne_bool(false);
    bool stored = prefs.putBytes("xpt_cal", &calibration, sizeof(calibration)) == sizeof(calibration);
    prefs.end();
    if (stored)
        g_vyne_touch_calibration = calibration;
    return vyne_bool(stored);
#else
    // Interactive: the panel draws crosshairs and the user taps each one.
    // TFT_eSPI fills a 5-element calibration tuple, which we persist to
    // NVS under the "vyne_touch" namespace.
    uint16_t calData[5] = {0};
    g_vyne_tft.fillScreen(TFT_BLACK);
    g_vyne_tft.calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 15);
    g_vyne_tft.setTouch(calData);
    g_vyne_shared_touch_calibrated = true;

    Preferences prefs;
    if (!prefs.begin("vyne_touch", false))
        return vyne_bool(false);
    bool stored = prefs.putBytes("cal", calData, sizeof(calData)) == sizeof(calData);
    prefs.end();
    return vyne_bool(stored);
#endif
}

#else // !TOUCH_CS

// TFT_eSPI has no touch driver configured. Keep the Vyne surface
// linkable and return "no contact" so a sketch can detect the absence at
// runtime; the hardware can be enabled by defining TOUCH_CS in
// User_Setup.h and rebuilding.

static inline VyneValue esp32_touch_screen_init(void) {
    return vyne_null();
}
static inline VyneValue esp32_touch_screen_configured(void) {
    return vyne_bool(false);
}
static inline VyneValue esp32_touch_screen_calibrated(void) {
    return vyne_bool(false);
}
static inline VyneValue esp32_touch_screen_pressed(void) {
    return vyne_bool(false);
}
static inline VyneValue esp32_touch_screen_x(void) {
    return vyne_int(0);
}
static inline VyneValue esp32_touch_screen_y(void) {
    return vyne_int(0);
}
static inline VyneValue esp32_touch_screen_pressure(void) {
    return vyne_int(0);
}
static inline VyneValue esp32_touch_screen_read(void) {
    return vyne_int(-1);
}
static inline VyneValue esp32_touch_screen_wait(void) {
    return vyne_int(-1);
}
static inline VyneValue esp32_touch_screen_wait_release(void) {
    return vyne_null();
}
static inline VyneValue esp32_touch_screen_calibrate(void) {
    return vyne_bool(false);
}

#endif // TOUCH_CS

// runtime/esp32/detail/mcu_display.h
//
// Display bindings, backed by TFT_eSPI by default. TFT_eSPI is configured
// entirely by the user's `User_Setup.h` / `User_Setup_Select.h`; Vyne
// does not choose pins. Allocate the library through arduino-cli's
// library manager (or a vendored copy on the build path) before compiling
// a sketch that uses display.*.
//
// Swapping to LovyanGFX is a header-only change in this file plus one
// `using` alias at the top: the public surface below is compatible
// (LovyanGFX exposes init/setRotation/width/height/drawPixel/drawLine/
// fillRect/drawString with the same argument order).
//
// Colors are packed RGB565 integers in 0..65535. Use display_rgb(r,g,b)
// to build one from 8-bit components.
#pragma once
#include "mcu_types.h"
#include <TFT_eSPI.h>

static TFT_eSPI g_vyne_tft;
static bool g_vyne_tft_inited = false;

static inline VyneValue esp32_display_init(VyneValue rotation) {
    g_vyne_tft.init();
    g_vyne_tft_inited = true;
    g_vyne_tft.setRotation((uint8_t)(rotation.as.i64 & 0x3));
    g_vyne_tft.setTextColor(TFT_WHITE, TFT_BLACK);
    return vyne_null();
}

static inline VyneValue esp32_display_width(void) {
    return vyne_int((int64_t)g_vyne_tft.width());
}

static inline VyneValue esp32_display_height(void) {
    return vyne_int((int64_t)g_vyne_tft.height());
}

static inline VyneValue esp32_display_set_rotation(VyneValue r) {
    g_vyne_tft.setRotation((uint8_t)(r.as.i64 & 0x3));
    return vyne_null();
}

static inline VyneValue esp32_display_clear(VyneValue color) {
    g_vyne_tft.fillScreen((uint16_t)color.as.i64);
    return vyne_null();
}

static inline VyneValue
esp32_display_pixel(VyneValue x, VyneValue y, VyneValue color) {
    g_vyne_tft.drawPixel(
        (int32_t)x.as.i64,
        (int32_t)y.as.i64,
        (uint16_t)color.as.i64
    );
    return vyne_null();
}

static inline VyneValue esp32_display_line(
    VyneValue x0,
    VyneValue y0,
    VyneValue x1,
    VyneValue y1,
    VyneValue color
) {
    g_vyne_tft.drawLine(
        (int32_t)x0.as.i64,
        (int32_t)y0.as.i64,
        (int32_t)x1.as.i64,
        (int32_t)y1.as.i64,
        (uint16_t)color.as.i64
    );
    return vyne_null();
}

static inline VyneValue esp32_display_rect(
    VyneValue x,
    VyneValue y,
    VyneValue w,
    VyneValue h,
    VyneValue color
) {
    g_vyne_tft.drawRect(
        (int32_t)x.as.i64,
        (int32_t)y.as.i64,
        (int32_t)w.as.i64,
        (int32_t)h.as.i64,
        (uint16_t)color.as.i64
    );
    return vyne_null();
}

static inline VyneValue esp32_display_rect_filled(
    VyneValue x,
    VyneValue y,
    VyneValue w,
    VyneValue h,
    VyneValue color
) {
    g_vyne_tft.fillRect(
        (int32_t)x.as.i64,
        (int32_t)y.as.i64,
        (int32_t)w.as.i64,
        (int32_t)h.as.i64,
        (uint16_t)color.as.i64
    );
    return vyne_null();
}

static inline VyneValue
esp32_display_circle(VyneValue x, VyneValue y, VyneValue r, VyneValue color) {
    g_vyne_tft.drawCircle(
        (int32_t)x.as.i64,
        (int32_t)y.as.i64,
        (int32_t)r.as.i64,
        (uint16_t)color.as.i64
    );
    return vyne_null();
}

static inline VyneValue esp32_display_circle_filled(
    VyneValue x,
    VyneValue y,
    VyneValue r,
    VyneValue color
) {
    g_vyne_tft.fillCircle(
        (int32_t)x.as.i64,
        (int32_t)y.as.i64,
        (int32_t)r.as.i64,
        (uint16_t)color.as.i64
    );
    return vyne_null();
}

static inline VyneValue esp32_display_text(
    VyneValue x,
    VyneValue y,
    VyneValue text,
    VyneValue color,
    VyneValue size
) {
    const char* s = (text.type == V_STRING && text.as.str) ? text.as.str : "";
    int sz = (int)size.as.i64;
    if (sz < 1)
        sz = 1;
    if (sz > 7)
        sz = 7;
    g_vyne_tft.setTextColor((uint16_t)color.as.i64);
    g_vyne_tft.setTextSize((uint8_t)sz);
    g_vyne_tft.drawString(
        String(s),
        (int32_t)x.as.i64,
        (int32_t)y.as.i64
    );
    return vyne_null();
}

static inline VyneValue esp32_display_text_bg(
    VyneValue x,
    VyneValue y,
    VyneValue text,
    VyneValue fg,
    VyneValue bg,
    VyneValue size
) {
    const char* s = (text.type == V_STRING && text.as.str) ? text.as.str : "";
    int sz = (int)size.as.i64;
    if (sz < 1)
        sz = 1;
    if (sz > 7)
        sz = 7;
    g_vyne_tft.setTextColor((uint16_t)fg.as.i64, (uint16_t)bg.as.i64);
    g_vyne_tft.setTextSize((uint8_t)sz);
    g_vyne_tft.drawString(
        String(s),
        (int32_t)x.as.i64,
        (int32_t)y.as.i64
    );
    return vyne_null();
}

static inline VyneValue esp32_display_set_text_wrap(VyneValue wrap) {
    g_vyne_tft.setTextWrap(wrap.as.i64 != 0);
    return vyne_null();
}

static inline VyneValue esp32_display_backlight(VyneValue percent) {
    // Uses the TFT_BL pin from the user's TFT_eSPI configuration when it
    // is defined. 0 = off, 100 = full brightness (8-bit duty).
#if defined(TFT_BL)
    static bool s_ready = false;
    int pct = (int)percent.as.i64;
    if (pct < 0)
        pct = 0;
    if (pct > 100)
        pct = 100;
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    if (!s_ready) {
        ledcAttach(TFT_BL, 5000, 8);
        s_ready = true;
    }
    ledcWrite(TFT_BL, (uint32_t)((pct * 255) / 100));
#else
    if (!s_ready) {
        ledcSetup(0, 5000, 8);
        ledcAttachPin(TFT_BL, 0);
        s_ready = true;
    }
    ledcWrite(0, (uint32_t)((pct * 255) / 100));
#endif
#else
    (void)percent;
#endif
    return vyne_null();
}

static inline VyneValue esp32_display_push(void) {
    // TFT_eSPI draws directly to the panel. A sprite-based backend would
    // call pushSprite() here; keeping the symbol makes the Vyne source
    // portable across both.
    return vyne_null();
}

static inline VyneValue
esp32_display_rgb(VyneValue r, VyneValue g, VyneValue b) {
    // Pack 8-bit components into RGB565: RRRRRGGGGGGBBBBB.
    uint16_t rr = (uint16_t)(r.as.i64 & 0xF8);
    uint16_t gg = (uint16_t)(g.as.i64 & 0xFC);
    uint16_t bb = (uint16_t)(b.as.i64 & 0xF8);
    return vyne_int((int64_t)((rr << 8) | (gg << 3) | (bb >> 3)));
}

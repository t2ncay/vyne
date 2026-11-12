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
//
// Double buffering: create up to VYNE_MAX_SPRITES TFT_eSprite buffers,
// select one, draw with the normal display_* calls, then push it. This
// is what keeps a drawing app or oscilloscope from flickering. Select
// -1 to draw directly to the panel again.
#pragma once
#include "mcu_types.h"
#include <TFT_eSPI.h>
#include <new>

#define VYNE_MAX_SPRITES 4

static TFT_eSPI g_vyne_tft;
static bool g_vyne_tft_inited = false;

// Inherited drawing methods dispatch virtual pixel/line operations into
// the sprite. Non-virtual size/rotation and full-buffer clears need
// explicit sprite dispatch.
static TFT_eSPI* g_vyne_draw_target = &g_vyne_tft;
static TFT_eSprite* g_vyne_active_sprite = nullptr;
static TFT_eSprite* g_vyne_sprites[VYNE_MAX_SPRITES] = {nullptr};

static inline TFT_eSPI* _vyne_draw(void) {
    return g_vyne_draw_target;
}

static inline VyneValue esp32_display_init(VyneValue rotation) {
    g_vyne_active_sprite = nullptr;
    g_vyne_draw_target = &g_vyne_tft;
    g_vyne_tft.init();
    g_vyne_tft.setRotation((uint8_t)(rotation.as.i64 & 0x3));
    g_vyne_tft.setTextColor(TFT_WHITE, TFT_BLACK);
    g_vyne_tft_inited = true;
    return vyne_null();
}

static inline VyneValue esp32_display_width(void) {
    if (g_vyne_active_sprite)
        return vyne_int((int64_t)g_vyne_active_sprite->width());
    return vyne_int((int64_t)g_vyne_tft.width());
}

static inline VyneValue esp32_display_height(void) {
    if (g_vyne_active_sprite)
        return vyne_int((int64_t)g_vyne_active_sprite->height());
    return vyne_int((int64_t)g_vyne_tft.height());
}

static inline VyneValue esp32_display_set_rotation(VyneValue r) {
    uint8_t rot = (uint8_t)(r.as.i64 & 0x3);
    if (g_vyne_active_sprite)
        g_vyne_active_sprite->setRotation(rot);
    else
        g_vyne_tft.setRotation(rot);
    return vyne_null();
}

static inline VyneValue esp32_display_clear(VyneValue color) {
    if (g_vyne_active_sprite)
        g_vyne_active_sprite->fillSprite((uint16_t)color.as.i64);
    else
        g_vyne_tft.fillScreen((uint16_t)color.as.i64);
    return vyne_null();
}

static inline VyneValue
esp32_display_pixel(VyneValue x, VyneValue y, VyneValue color) {
    _vyne_draw()->drawPixel(
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
    _vyne_draw()->drawLine(
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
    _vyne_draw()->drawRect(
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
    _vyne_draw()->fillRect(
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
    _vyne_draw()->drawCircle(
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
    _vyne_draw()->fillCircle(
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
    TFT_eSPI* d = _vyne_draw();
    d->setTextColor((uint16_t)color.as.i64);
    d->setTextSize((uint8_t)sz);
    d->drawString(String(s), (int32_t)x.as.i64, (int32_t)y.as.i64);
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
    TFT_eSPI* d = _vyne_draw();
    d->setTextColor((uint16_t)fg.as.i64, (uint16_t)bg.as.i64);
    d->setTextSize((uint8_t)sz);
    d->drawString(String(s), (int32_t)x.as.i64, (int32_t)y.as.i64);
    return vyne_null();
}

static inline VyneValue esp32_display_set_text_wrap(VyneValue wrap) {
    _vyne_draw()->setTextWrap(wrap.as.i64 != 0);
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
    // With a sprite selected, commit it to the panel at the origin.
    // Direct-to-panel drawing needs no push.
    if (g_vyne_active_sprite)
        g_vyne_active_sprite->pushSprite(0, 0);
    return vyne_null();
}

// ---------------------------------------------------------------------
// Sprite / double-buffer surface.
//
//   handle := display_sprite_create(w, h);
//   display_sprite_select(handle);
//   ... draw with the normal display_* calls ...
//   display_sprite_push(handle, x, y);   // or display_push() at (0,0)
//   display_sprite_select(-1);           // back to the panel
//   display_sprite_delete(handle);
//
// Returns -1 when no slot is free or the sprite buffer cannot be
// allocated. A 16-bit 240x320 sprite needs about 150 KB of heap.
// ---------------------------------------------------------------------
static inline VyneValue
esp32_display_sprite_create(VyneValue width, VyneValue height) {
    // createSprite takes signed 16-bit dimensions. Validate before the
    // narrowing conversion so oversized Int64 inputs cannot wrap into
    // a different, apparently successful allocation.
    int64_t w = width.as.i64;
    int64_t h = height.as.i64;
    if (w <= 0 || h <= 0 || w > INT16_MAX || h > INT16_MAX)
        return vyne_int(-1);

    for (int i = 0; i < VYNE_MAX_SPRITES; ++i) {
        if (g_vyne_sprites[i])
            continue;
        TFT_eSprite* s = new (std::nothrow) TFT_eSprite(&g_vyne_tft);
        if (!s)
            return vyne_int(-1);
        // TFT_eSPI 2.5.43 does not initialize gfxFont in its constructor.
        // A static panel happens to be zero-initialized; a heap sprite
        // is not. Explicitly choose the built-in font before any text
        // operation, otherwise textWidth dereferences a garbage pointer.
        s->setTextFont(1);
        s->setColorDepth(16);
        if (s->createSprite((int16_t)w, (int16_t)h) == nullptr) {
            delete s;
            return vyne_int(-1);
        }
        s->setRotation(g_vyne_tft.getRotation());
        g_vyne_sprites[i] = s;
        return vyne_int(i);
    }
    return vyne_int(-1);
}

static inline VyneValue esp32_display_sprite_delete(VyneValue handle) {
    int i = (int)handle.as.i64;
    if (i < 0 || i >= VYNE_MAX_SPRITES || !g_vyne_sprites[i])
        return vyne_null();
    if (g_vyne_active_sprite == g_vyne_sprites[i]) {
        g_vyne_active_sprite = nullptr;
        g_vyne_draw_target = &g_vyne_tft;
    }
    g_vyne_sprites[i]->deleteSprite();
    delete g_vyne_sprites[i];
    g_vyne_sprites[i] = nullptr;
    return vyne_null();
}

static inline VyneValue esp32_display_sprite_select(VyneValue handle) {
    int i = (int)handle.as.i64;
    if (i < 0 || i >= VYNE_MAX_SPRITES || !g_vyne_sprites[i]) {
        g_vyne_active_sprite = nullptr;
        g_vyne_draw_target = &g_vyne_tft;
        return vyne_null();
    }
    g_vyne_active_sprite = g_vyne_sprites[i];
    g_vyne_draw_target = g_vyne_sprites[i];
    return vyne_null();
}

static inline VyneValue
esp32_display_sprite_push(VyneValue handle, VyneValue x, VyneValue y) {
    int i = (int)handle.as.i64;
    if (i < 0 || i >= VYNE_MAX_SPRITES || !g_vyne_sprites[i])
        return vyne_null();
    g_vyne_sprites[i]->pushSprite((int32_t)x.as.i64, (int32_t)y.as.i64);
    return vyne_null();
}

static inline VyneValue esp32_display_sprite_width(VyneValue handle) {
    int i = (int)handle.as.i64;
    if (i < 0 || i >= VYNE_MAX_SPRITES || !g_vyne_sprites[i])
        return vyne_int(0);
    return vyne_int((int64_t)g_vyne_sprites[i]->width());
}

static inline VyneValue esp32_display_sprite_height(VyneValue handle) {
    int i = (int)handle.as.i64;
    if (i < 0 || i >= VYNE_MAX_SPRITES || !g_vyne_sprites[i])
        return vyne_int(0);
    return vyne_int((int64_t)g_vyne_sprites[i]->height());
}

static inline VyneValue
esp32_display_sprite_fill(VyneValue handle, VyneValue color) {
    int i = (int)handle.as.i64;
    if (i < 0 || i >= VYNE_MAX_SPRITES || !g_vyne_sprites[i])
        return vyne_null();
    g_vyne_sprites[i]->fillSprite((uint16_t)color.as.i64);
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

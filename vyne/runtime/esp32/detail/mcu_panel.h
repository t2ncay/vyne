#pragma once
#include <Arduino.h>
#include "mcu_panel_orientation.h"
#ifndef VYNE_DISPLAY_MIRROR_X
#define VYNE_DISPLAY_MIRROR_X 0
#endif
#ifndef VYNE_DISPLAY_MIRROR_Y
#define VYNE_DISPLAY_MIRROR_Y 0
#endif
#if defined(ARDUINO_ARCH_ESP32) && !defined(ESP32)
#error "ESP32 platform define missing: use compiler.cpp.extra_flags instead of replacing build.extra_flags"
#endif
#include <TFT_eSPI.h>
class VynePanel : public TFT_eSPI {
public:
    void setRotation(uint8_t rotation) {
        TFT_eSPI::setRotation(rotation);
#if VYNE_DISPLAY_MIRROR_X || VYNE_DISPLAY_MIRROR_Y
#if !defined(ILI9341_DRIVER) && !defined(ILI9341_2_DRIVER) && !defined(ST7796_DRIVER)
#error "Profile-controlled mirrors currently support ILI9341 and ST7796"
#endif
        writecommand(0x36);
        writedata(vyne_panel_madctl(rotation, TFT_MAD_COLOR_ORDER,
                                   VYNE_DISPLAY_MIRROR_X, VYNE_DISPLAY_MIRROR_Y));
#endif
    }
};
using VyneSprite = TFT_eSprite;
using VyneDrawTarget = TFT_eSPI;

// Shared by display and touch-only sketches. Board-specific enables are
// configured by the build profile, never by guessing a GPIO in the runtime.
static VynePanel g_vyne_tft;
static bool g_vyne_tft_inited = false;

static inline void _vyne_panel_init(void) {
    if (g_vyne_tft_inited)
        return;
#if defined(VYNE_DISPLAY_ENABLE_PIN)
    pinMode(VYNE_DISPLAY_ENABLE_PIN, OUTPUT);
    digitalWrite(VYNE_DISPLAY_ENABLE_PIN, HIGH);
#endif
#if defined(TFT_BL) && defined(TFT_BACKLIGHT_ON)
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
#endif
    g_vyne_tft.init();
    g_vyne_tft.setTextFont(1);
    g_vyne_tft_inited = true;
}

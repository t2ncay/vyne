// Hardware regression for the runtime's sprite lifecycle and drawing target.
// Staged with the current runtime headers by scripts/test_esp32.py.
#define VYNE_USE_DISPLAY 1
#define VYNE_USE_TOUCH 1
#include "vyne_esp32.h"

static int failures = 0;

class CaptureStream : public Stream {
public:
    String text;
    size_t write(uint8_t c) override {
        text += (char)c;
        return 1;
    }
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override {}
};

static void check(bool ok, const char* name) {
    Serial.print(name);
    Serial.println(ok ? ": PASS" : ": FAIL");
    if (!ok)
        ++failures;
}

void vyne_setup() {
    delay(300);
    check(arduino_sqrt(vyne_int(16)).as.f64 == 4.0,
          "integer sqrt argument");
    check(arduino_cos(vyne_int(1)).as.f64 > 0.54 &&
          arduino_cos(vyne_int(1)).as.f64 < 0.541, "integer cosine argument");
    check(arduino_pow(vyne_int(2), vyne_float(10.0)).as.f64 == 1024.0,
          "mixed numeric power arguments");
    check(arduino_constrain(vyne_int(8), vyne_float(0.0), vyne_int(5)).as.f64 == 5.0,
          "mixed numeric constrain arguments");
    check(arduino_lerp(vyne_int(2), vyne_int(10), vyne_float(0.5)).as.f64 == 6.0,
          "mixed numeric lerp arguments");
    CaptureStream captured;
    _vyne_print_stream(captured, vyne_int(INT64_MAX));
    check(captured.text == "9223372036854775807", "stream Int64 maximum");
    captured.text = "";
    _vyne_print_stream(captured, vyne_int(INT64_MIN));
    check(captured.text == "-9223372036854775808", "stream Int64 minimum");
    Serial.print("int64 max = ");
    arduino_serial_println(vyne_int(INT64_MAX));
    Serial.print("int64 min = ");
    arduino_serial_println(vyne_int(INT64_MIN));
    esp32_display_init(vyne_int(1));
#if (defined(ILI9341_DRIVER) || defined(ILI9341_2_DRIVER)) && defined(TFT_RGB_ORDER) && TFT_RGB_ORDER == 1
    check(TFT_MAD_COLOR_ORDER == 0 && (TFT_MAD_MV | TFT_MAD_COLOR_ORDER) == 0x20,
          "ILI9341 RGB MADCTL configuration");
    for (int rotation = 0; rotation < 4; ++rotation) {
        esp32_display_set_rotation(vyne_int(rotation));
        check(g_vyne_tft.getRotation() == rotation, "panel rotation");
    }
    esp32_display_set_rotation(vyne_int(1));
#endif
#if defined(ST7796_DRIVER) && defined(TFT_RGB_ORDER) && TFT_RGB_ORDER == 1
    check(TFT_MAD_COLOR_ORDER == 0, "ST7796 RGB configuration");
    for (int rotation = 0; rotation < 4; ++rotation) {
        esp32_display_set_rotation(vyne_int(rotation));
        check(g_vyne_tft.getRotation() == rotation, "panel rotation with mirror profile");
    }
    esp32_display_set_rotation(vyne_int(1));
#endif
#if defined(VYNE_TOUCH_XPT2046)
    Preferences prefs;
    check(prefs.begin("vyne_touch_test", false), "test calibration namespace");
    VyneTouchCalibration calibration = {1, 480, 320, 1, 0, 2, 400, 3600, 500, 3500};
    prefs.putBytes("xpt_cal", &calibration, sizeof(calibration));
    prefs.end();
    VyneTouchCalibration loaded = {};
    check(_vyne_touch_load_xpt_calibration("vyne_touch_test", loaded) && loaded.reserved == 2 &&
          loaded.width == 480 && loaded.x0 == 400, "calibration NVS round trip");
    prefs.begin("vyne_touch_test", false);
    prefs.putBytes("xpt_cal", &calibration, sizeof(calibration) - 1);
    prefs.end();
    check(!_vyne_touch_load_xpt_calibration("vyne_touch_test", loaded), "truncated NVS calibration rejected");
    prefs.begin("vyne_touch_test", false);
    calibration.version = 2;
    prefs.putBytes("xpt_cal", &calibration, sizeof(calibration));
    prefs.end();
    check(!_vyne_touch_load_xpt_calibration("vyne_touch_test", loaded), "unknown calibration version rejected");
    prefs.begin("vyne_touch_test", false);
    prefs.clear();
    prefs.end();
#endif
#if defined(TFT_BL)
    check(digitalRead(TFT_BL) == TFT_BACKLIGHT_ON, "backlight enabled");
#endif
#if defined(VYNE_DISPLAY_ENABLE_PIN)
    check(digitalRead(VYNE_DISPLAY_ENABLE_PIN) == HIGH, "auxiliary panel enable");
#endif
    esp32_display_clear(vyne_int(TFT_BLACK));
    check(esp32_display_sprite_create(vyne_int(65537), vyne_int(8)).as.i64 == -1,
          "oversized width rejected");
    check(esp32_display_sprite_create(vyne_int(8), vyne_int(65537)).as.i64 == -1,
          "oversized height rejected");
    check(esp32_display_sprite_create(vyne_int(0), vyne_int(8)).as.i64 == -1,
          "empty sprite rejected");

    for (int pass = 0; pass < 8; ++pass) {
        VyneValue handles[VYNE_MAX_SPRITES];
        for (int slot = 0; slot < VYNE_MAX_SPRITES; ++slot) {
            handles[slot] = esp32_display_sprite_create(vyne_int(520), vyne_int(16));
            check(handles[slot].as.i64 == slot, "sprite slot allocation");
            if (handles[slot].as.i64 != slot)
                continue;
            esp32_display_sprite_select(handles[slot]);
            check(esp32_display_width().as.i64 == 520 &&
                  esp32_display_height().as.i64 == 16, "selected sprite dimensions");
            esp32_display_clear(vyne_int(TFT_RED));
            check(g_vyne_sprites[slot]->readPixel(519, 15) == TFT_RED,
                  "clear reaches beyond panel width");
            esp32_display_rect_filled(vyne_int(0), vyne_int(0), vyne_int(4),
                                      vyne_int(4), vyne_int(TFT_BLUE));
            check(g_vyne_sprites[slot]->readPixel(1, 1) == TFT_BLUE,
                  "drawing reaches sprite buffer");
            // The original display example panics here on heap-reused gfxFont.
            esp32_display_text(vyne_int(8), vyne_int(0),
                               vyne_string_static("sprite text"),
                               vyne_int(TFT_WHITE), vyne_int(1));
            esp32_display_text_bg(vyne_int(110), vyne_int(0),
                                  vyne_string_static("background"),
                                  vyne_int(TFT_WHITE), vyne_int(TFT_BLACK), vyne_int(1));
            esp32_display_push();
            esp32_display_sprite_push(handles[slot], vyne_int(0), vyne_int(slot * 16));
        }
        check(esp32_display_sprite_create(vyne_int(8), vyne_int(8)).as.i64 == -1,
              "full sprite table rejected");
        for (int slot = 0; slot < VYNE_MAX_SPRITES; ++slot) {
            esp32_display_sprite_select(handles[slot]);
            esp32_display_sprite_delete(handles[slot]);
            check(g_vyne_active_sprite == nullptr && _vyne_draw() == &g_vyne_tft,
                  "deleting selected sprite restores panel");
            check(esp32_display_sprite_width(handles[slot]).as.i64 == 0,
                  "deleted handle has no dimensions");
            esp32_display_sprite_delete(handles[slot]);
        }
    }
    Serial.println(failures ? "runtime: FAIL" : "runtime: PASS");
}

void vyne_loop() {
    delay(1);
}

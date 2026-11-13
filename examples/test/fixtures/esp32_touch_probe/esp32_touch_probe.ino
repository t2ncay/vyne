// Direct TFT_eSPI control sketch: isolates controller/wiring problems from
// Vyne lowering, touch calibration, and the runtime wrapper.
#include "detail/mcu_panel.h"
#if defined(VYNE_TOUCH_REFERENCE_XPT2046)
#include <XPT2046_Touchscreen.h>
#ifndef VYNE_TOUCH_SPI_PORT
#define VYNE_TOUCH_SPI_PORT VSPI
#endif
static SPIClass reference_spi(VYNE_TOUCH_SPI_PORT);
static XPT2046_Touchscreen reference_touch(VYNE_TOUCH_CS);
#endif
#if defined(VYNE_TOUCH_XPT2046)
#include "detail/mcu_xpt2046.h"
#endif

static VynePanel& panel = g_vyne_tft;

void setup() {
    Serial.begin(115200);
    delay(300);
#if defined(VYNE_DISPLAY_ENABLE_PIN)
    pinMode(VYNE_DISPLAY_ENABLE_PIN, OUTPUT);
    digitalWrite(VYNE_DISPLAY_ENABLE_PIN, HIGH);
#endif
    _vyne_panel_init();
    panel.setRotation(1);
    panel.fillScreen(TFT_BLACK);
    panel.setTextFont(1);
    panel.setTextColor(TFT_WHITE, TFT_BLACK);
    panel.drawString("Hold the touch panel for 15 seconds", 10, 10, 2);
#if defined(VYNE_TOUCH_REFERENCE_XPT2046)
    reference_spi.begin(VYNE_TOUCH_SCLK, VYNE_TOUCH_MISO, VYNE_TOUCH_MOSI, VYNE_TOUCH_CS);
    reference_touch.begin(reference_spi);
    reference_touch.setRotation(1);
    Serial.println("touch probe: reference XPT2046_Touchscreen library");
    for (int i = 0; i < 30; ++i) {
        TS_Point raw = reference_touch.getPoint();
        Serial.printf("raw touch: x=%d y=%d z=%d contact=%d\n", raw.x, raw.y, raw.z, raw.z >= 600);
        delay(500);
    }
    Serial.println("touch probe: DONE");
#elif defined(VYNE_TOUCH_XPT2046)
    for (int i = 0; i < 30; ++i) {
        VyneTouchRaw raw;
        bool contact = _vyne_xpt_read_raw(raw);
        Serial.printf("raw touch: x=%u y=%u z=%u contact=%d\n", raw.x, raw.y, raw.pressure, contact);
        delay(500);
    }
    Serial.println("touch probe: DONE");
#elif defined(TOUCH_CS)
    for (int i = 0; i < 30; ++i) {
        uint16_t x = 0, y = 0;
        panel.getTouchRaw(&x, &y);
        Serial.printf("raw touch: x=%u y=%u z=%u\n", x, y, panel.getTouchRawZ());
        delay(500);
    }
    Serial.println("touch probe: DONE");
#else
    Serial.println("touch probe: SKIP (TOUCH_CS not configured)");
#endif
}

void loop() {
    delay(1);
}

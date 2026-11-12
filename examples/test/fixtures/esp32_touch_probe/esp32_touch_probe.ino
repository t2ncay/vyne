// Direct TFT_eSPI control sketch: isolates controller/wiring problems from
// Vyne lowering, touch calibration, and the runtime wrapper.
#include <TFT_eSPI.h>

static TFT_eSPI panel;

void setup() {
    Serial.begin(115200);
    delay(300);
    panel.init();
    panel.setRotation(1);
    panel.fillScreen(TFT_BLACK);
    panel.setTextFont(1);
    panel.setTextColor(TFT_WHITE, TFT_BLACK);
    panel.drawString("Hold the touch panel for 15 seconds", 10, 10, 2);
#if defined(TOUCH_CS)
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

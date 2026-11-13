// Standalone TFT_eSPI diagnostic, launched by scripts/test_esp32_screen.py.
// Serial controls: r/g/b/w/k/c/t/d = patterns, a = automatic cycling.
#include "detail/mcu_panel.h"
static VynePanel& panel = g_vyne_tft;
static const char* names[] = {"RED", "GREEN", "BLUE", "WHITE", "BLACK", "GRID", "TEXT", "BOUNDS"};
static int stage = 0;
static bool automatic = true;
static uint32_t last_change = 0;

static void draw_stage(int next) {
    stage = next;
    const uint16_t colors[] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_WHITE, TFT_BLACK};
    if (stage < 5) {
        panel.fillScreen(colors[stage]);
    } else if (stage == 5) {
        panel.fillScreen(TFT_BLACK);
        for (int x = 0; x < panel.width(); x += 20)
            panel.drawFastVLine(x, 0, panel.height(), TFT_CYAN);
        for (int y = 0; y < panel.height(); y += 20)
            panel.drawFastHLine(0, y, panel.width(), TFT_CYAN);
        panel.drawRect(0, 0, panel.width(), panel.height(), TFT_WHITE);
        panel.drawLine(0, 0, panel.width() - 1, panel.height() - 1, TFT_RED);
        panel.drawLine(panel.width() - 1, 0, 0, panel.height() - 1, TFT_GREEN);
    } else if (stage == 6) {
        panel.fillScreen(TFT_BLACK);
        panel.setTextFont(1);
        panel.setTextSize(2);
        panel.setTextColor(TFT_WHITE, TFT_BLACK);
        panel.drawString("ESP32 SCREEN DEMO", 10, 15);
        panel.setTextColor(TFT_YELLOW, TFT_BLACK);
        panel.drawString("Can you see this?", 10, 50);
        panel.setTextSize(1);
        panel.setTextColor(TFT_GREEN, TFT_BLACK);
        char dimensions[48];
        snprintf(dimensions, sizeof(dimensions), "Configured size: %d x %d", panel.width(), panel.height());
        panel.drawString(dimensions, 10, 90);
        panel.drawString("Red / green / blue / white / black / grid", 10, 115);
    } else {
        panel.fillScreen(TFT_BLACK);
        panel.setTextFont(1);
        panel.setTextSize(1);
        panel.setTextColor(TFT_GREEN, TFT_BLACK);
        panel.drawRect(0, 0, 320, 240, TFT_GREEN);
        panel.drawString("GREEN: 320x240", 10, 30);
        panel.setTextColor(TFT_YELLOW, TFT_BLACK);
        panel.drawRect(0, 0, 480, 320, TFT_YELLOW);
        panel.drawString("YELLOW: 480x320", 10, 50);
        const int points[4][2] = {{20,20},{300,20},{300,220},{20,220}};
        for (int i = 0; i < 4; ++i) {
            panel.drawCircle(points[i][0], points[i][1], 8, TFT_WHITE);
            panel.setTextColor(TFT_WHITE, TFT_BLACK);
            char label[8];
            snprintf(label, sizeof(label), "%d", i + 1);
            panel.drawString(label, points[i][0] - 3, points[i][1] - 3);
        }
    }
    Serial.printf("SCREEN_STAGE:%s\n", names[stage]);
    last_change = millis();
}

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("SCREEN_BOOT");
#if defined(VYNE_DISPLAY_ENABLE_PIN)
    pinMode(VYNE_DISPLAY_ENABLE_PIN, OUTPUT);
    digitalWrite(VYNE_DISPLAY_ENABLE_PIN, HIGH);
    Serial.printf("SCREEN_ENABLE:pin=%d level=%d\n", VYNE_DISPLAY_ENABLE_PIN,
                  digitalRead(VYNE_DISPLAY_ENABLE_PIN));
#endif
    _vyne_panel_init();
    panel.setRotation(1);
    panel.setTextFont(1);
    // These are compiled configuration values, not a hardware ID/readback.
    setup_t settings;
    panel.getSetup(settings);
    Serial.println("SCREEN_BACKEND:TFT_eSPI");
    Serial.printf("SCREEN_CONFIG:driver=0x%04X width=%d height=%d\n",
                  settings.tft_driver, panel.width(), panel.height());
    Serial.printf("SCREEN_ORIENTATION:rotation=1 mirror_x=%d mirror_y=%d\n",
                  VYNE_DISPLAY_MIRROR_X, VYNE_DISPLAY_MIRROR_Y);
#if defined(TFT_RGB_ORDER)
    Serial.printf("SCREEN_COLOR_ORDER:%s\n", TFT_RGB_ORDER == 1 ? "RGB" : "BGR");
#endif
#if defined(ILI9341_DRIVER) || defined(ILI9341_2_DRIVER)
    Serial.printf("SCREEN_MADCTL:rotation1=0x%02X (configured)\n",
                  TFT_MAD_MV | TFT_MAD_COLOR_ORDER);
#endif
    Serial.printf("SCREEN_PINS:mosi=%d miso=%d sclk=%d cs=%d dc=%d rst=%d\n",
                  TFT_MOSI, TFT_MISO, TFT_SCLK, TFT_CS, TFT_DC, TFT_RST);
    Serial.printf("SCREEN_ID_D3:%02X %02X %02X %02X (raw readback)\n",
                  panel.readcommand8(0xD3, 0), panel.readcommand8(0xD3, 1),
                  panel.readcommand8(0xD3, 2), panel.readcommand8(0xD3, 3));
#if defined(TFT_BL) && defined(TFT_BACKLIGHT_ON)
    Serial.printf("SCREEN_BACKLIGHT:pin=%d active=%d\n", TFT_BL, TFT_BACKLIGHT_ON);
    Serial.printf("SCREEN_BACKLIGHT_LEVEL:%d\n", digitalRead(TFT_BL));
#else
    Serial.println("SCREEN_BACKLIGHT:NOT_CONFIGURED (confirm LED/BL wiring)");
#endif
    Serial.println("SCREEN_DEMO_READY");
    draw_stage(0);
}

void loop() {
    while (Serial.available()) {
        int command = Serial.read();
        const char* keys = "rgbwkctd";
        const char* match = command > 0 ? strchr(keys, command) : nullptr;
        if (match) {
            automatic = false;
            draw_stage((int)(match - keys));
        } else if (command == 'a') {
            automatic = true;
            draw_stage(0);
        }
    }
    if (automatic && (uint32_t)(millis() - last_change) >= 4000)
        draw_stage((stage + 1) % 8);
    delay(10);
}

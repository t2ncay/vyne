# plotter-esp32 — HW-950 measured owner revision: ST7796-compatible 240x320 RGB, landscape, corrected Y, shared-HSPI XPT2046
# One-time setup: vynec hardware setup
# Compile: vynec build
# Compile, upload, reset and start: vynec run
# Optional serial monitor: vynec run --monitor

ruleset { dynamic_casting };

use native esp32;

const HW950_BLACK :: Int64 := 0x0000;

fn hw950_draw_frame(width :: Int64, height :: Int64) {
    esp32.display_clear(HW950_BLACK);
    esp32.display_rect(0, 0, width, height, 65535);
    esp32.display_text(4, 4, "TL", 65535, 1);
    esp32.display_text(width - 24, 4, "TR", 65535, 1);
    esp32.display_text(4, height - 16, "BL", 65535, 1);
    esp32.display_text(width - 24, height - 16, "BR", 65535, 1);
    esp32.display_text(16, 32, "plotter-esp32", 65535, 2);
    esp32.display_text(16, 56, "Touch to draw; serial C calibrates", 65535, 1);
    esp32.display_rect_filled(16, 80, 40, 18, 63488);
    esp32.display_rect_filled(60, 80, 40, 18, 2016);
    esp32.display_rect_filled(104, 80, 40, 18, 31);
}

# No argument uses hardware.toml orientation. Explicit display_init(0..3)
# overrides that default, just as it does in examples/test/.
esp32.display_init();
esp32.touch_screen_init();
width :: Int64 := esp32.display_width();
height :: Int64 := esp32.display_height();
hw950_draw_frame(width, height);
esp32.serial_println("plotter-esp32: HW-950 ready");
esp32.serial_print("HW950_GEOMETRY rotation=");
esp32.serial_print(esp32.display_rotation());
esp32.serial_print(" width=");
esp32.serial_print(width);
esp32.serial_print(" height=");
esp32.serial_println(height);
esp32.serial_print("HW950_TOUCH configured=");
esp32.serial_print(esp32.touch_screen_configured());
esp32.serial_print(" calibrated=");
esp32.serial_println(esp32.touch_screen_calibrated());
esp32.serial_println("Touch polling stays active. Send C over serial to calibrate this orientation.");

hits :: Int64 := 0;
last_report :: Int64 := esp32.millis();
while true {
    @bounded<4096> region sample {
        if (esp32.serial_available() > 0) {
            command :: Int64 := esp32.serial_read();
            if (command == 99 || command == 67) {
                esp32.serial_println("HW950_CALIBRATION_BEGIN");
                calibrated :: Bool := esp32.touch_screen_calibrate();
                esp32.serial_print("HW950_CALIBRATION_RESULT ");
                esp32.serial_println(calibrated);
                hw950_draw_frame(width, height);
            }
        }
        if (esp32.touch_screen_pressed()) {
            x :: Int64 := esp32.touch_screen_x();
            y :: Int64 := esp32.touch_screen_y();
            esp32.display_circle_filled(x, y, 3, 2016);
            esp32.serial_print("HW950_CONTACT x=");
            esp32.serial_print(x);
            esp32.serial_print(" y=");
            esp32.serial_print(y);
            esp32.serial_print(" pressure=");
            esp32.serial_println(esp32.touch_screen_pressure());
            hits = hits + 1;
        }
        now :: Int64 := esp32.millis();
        if ((now - last_report) >= 2000) {
            esp32.serial_print("HW950_HEARTBEAT hits=");
            esp32.serial_print(hits);
            esp32.serial_print(" arena=");
            esp32.serial_println(esp32.arena_used());
            last_report = now;
        }
    };
    esp32.delay(20);
}





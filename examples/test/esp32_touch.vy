# esp32_touch.vy — touchscreen/stylus polling for 10 seconds.
#
# Requires: VYNE_USE_TOUCH; shared-bus TOUCH_CS or profile-configured XPT2046
# Hardware: ESP32-WROOM-32 + touch panel
# Flash:    vynec --preset hw950 --esp32-flash --esp32-port COM5 examples/test/esp32_touch.vy
# Expect:   green dots follow contact; "x=.. y=.. pressure=..", then "touch: DONE"
#
# Pressure is meaningful on a resistive panel. Calibrate before testing coordinates.

use native esp32;

esp32.serial_begin(115200);
esp32.delay(300);

esp32.serial_println("=== ESP32 touch ===");

esp32.display_init(1);
esp32.touch_screen_init();
esp32.display_clear(0x0000);
esp32.display_text(10, 10, "Touch: dots should follow", 0xFFFF, 1);
width :: Int64 := esp32.display_width();
height :: Int64 := esp32.display_height();

esp32.serial_println("touch the panel for 10 seconds");

start :: Int64 := esp32.millis();
now :: Int64 := start;
hits :: Int64 := 0;

while ((now - start) < 10000) {
    if (esp32.touch_screen_pressed()) {
        x :: Int64 := esp32.touch_screen_x();
        y :: Int64 := esp32.touch_screen_y();
        if (x >= 0 && x < width && y >= 0 && y < height) {
            esp32.display_circle_filled(x, y, 3, 0x07E0);
        } else {
            esp32.serial_println("touch: FAIL (coordinates out of bounds)");
        }
        esp32.serial_print("x=");
        esp32.serial_print(x);
        esp32.serial_print(" y=");
        esp32.serial_print(y);
        esp32.serial_print(" pressure=");
        esp32.serial_println(esp32.touch_screen_pressure());
        hits = hits + 1;
    }
    esp32.delay(20);
    now = esp32.millis();
}

esp32.serial_print("hits = ");
esp32.serial_println(hits);

if (hits > 0) {
    esp32.serial_println("touch: PASS");
} else {
    esp32.serial_println("touch: no contact");
}
esp32.serial_println("touch: DONE");

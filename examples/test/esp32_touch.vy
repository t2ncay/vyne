# esp32_touch.vy — touchscreen/stylus polling for 10 seconds.
#
# Requires: VYNE_USE_TOUCH; TFT_eSPI touch configured (TOUCH_CS or capacitive)
# Hardware: ESP32-WROOM-32 + touch panel
# Flash:    vynec --esp32 esp32 --esp32-flash --esp32-port COM3 examples/test/esp32_touch.vy
# Expect:   "x=.. y=.. pressure=.." on contact, then "touch: DONE"
#
# Pressure is meaningful on a resistive panel; on capacitive panels it is 1.

use native esp32;

esp32.serial_begin(115200);
esp32.delay(300);

esp32.serial_println("=== ESP32 touch ===");

esp32.display_init(1);
esp32.touch_screen_init();

esp32.serial_println("touch the panel for 10 seconds");

start :: Int64 := esp32.millis();
hits :: Int64 := 0;

while ((esp32.millis() - start) < 10000) {
    if (esp32.touch_screen_pressed()) {
        esp32.serial_print("x=");
        esp32.serial_print(esp32.touch_screen_x());
        esp32.serial_print(" y=");
        esp32.serial_print(esp32.touch_screen_y());
        esp32.serial_print(" pressure=");
        esp32.serial_println(esp32.touch_screen_pressure());
        hits = hits + 1;
    } else {
        esp32.delay(20);
    }
}

esp32.serial_print("hits = ");
esp32.serial_println(hits);

if (hits > 0) {
    esp32.serial_println("touch: PASS");
} else {
    esp32.serial_println("touch: no contact");
}
esp32.serial_println("touch: DONE");

# esp32_display.vy — TFT_eSPI drawing primitives and rotation cycle.
#
# Requires: VYNE_USE_DISPLAY; TFT_eSPI configured for the panel
# Hardware: ESP32-WROOM-32 + TFT_eSPI display
# Flash:    vynec --esp32 esp32 --esp32-flash --esp32-port COM3 examples/test/esp32_display.vy
# Expect:   drawing sequence on the panel, then "display test complete"
#
# Colors are packed RGB565 Int64 values; display_rgb(r,g,b) builds one.

use native esp32;

esp32.serial_begin(115200);
esp32.delay(300);

esp32.serial_println("=== ESP32 display ===");

esp32.display_init(1);

w :: Int64 := esp32.display_width();
h :: Int64 := esp32.display_height();
esp32.serial_print("size = ");
esp32.serial_print(w);
esp32.serial_print(" x ");
esp32.serial_println(h);

black :: Int64 := esp32.display_rgb(0, 0, 0);
red :: Int64 := esp32.display_rgb(255, 0, 0);
green :: Int64 := esp32.display_rgb(0, 255, 0);
blue :: Int64 := esp32.display_rgb(0, 0, 255);
white :: Int64 := esp32.display_rgb(255, 255, 255);
yellow :: Int64 := esp32.display_rgb(255, 255, 0);

esp32.display_clear(black);

through r :: 0..3 -> loop {
    esp32.display_set_rotation(r);
    esp32.display_clear(black);
    esp32.display_pixel(10, 10, white);
    esp32.display_line(0, 0, w - 1, h - 1, red);
    esp32.display_rect(20, 20, 80, 50, green);
    esp32.display_rect_filled(30, 30, 60, 30, blue);
    esp32.display_circle(w / 2, h / 2, 40, yellow);
    esp32.display_circle_filled(w / 2, h / 2, 20, red);
    esp32.display_text(10, 10, "Vyne ESP32", white, 2);
    esp32.display_text_bg(10, 40, "display OK", black, white, 2);
    esp32.display_set_text_wrap(true);
    esp32.display_push();
    esp32.delay(1000);
};

esp32.display_set_rotation(0);
esp32.display_clear(black);
esp32.display_text(10, 10, "display test complete", white, 2);
esp32.display_backlight(80);
esp32.display_push();

esp32.serial_println("display test complete");

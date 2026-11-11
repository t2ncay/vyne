# esp32_touch_calibrate.vy — interactive resistive-panel calibration.
#
# Requires: VYNE_USE_TOUCH; TFT_eSPI touch configured (TOUCH_CS)
# Hardware: ESP32-WROOM-32 + resistive touch panel
# Flash:    vynec --esp32 esp32 --esp32-flash --esp32-monitor --esp32-port COM3 examples/test/esp32_touch_calibrate.vy
# Expect:   crosshairs appear; tap each one. Calibration is stored in NVS
#           namespace "vyne_touch" and reloaded on the next boot.
#
# Run this once per physical panel. All other touch sketches use the
# stored calibration automatically.

use native esp32;

esp32.serial_begin(115200);
esp32.delay(300);

esp32.serial_println("=== ESP32 touch calibration ===");
esp32.serial_println("tap each crosshair when it appears");

esp32.display_init(1);
esp32.touch_screen_init();

esp32.delay(1000);
esp32.touch_screen_calibrate();

esp32.serial_println("calibration stored in NVS namespace vyne_touch");
esp32.serial_println("touch calibrate: DONE");

# esp32_power.vy — blink, deep sleep for 5 seconds, report the wake cause.
#
# Requires: VYNE_USE_POWER
# Hardware: ESP32-WROOM-32, LED on GPIO2 (optional)
# Flash:    vynec --esp32 esp32 --esp32-flash --esp32-monitor --esp32-port COM3 examples/test/esp32_power.vy
# Expect:   on first boot wake_cause = 0; after each 5 s sleep it repeats
#           with wake_cause = 4 (timer), then "power: DONE" before sleeping.

use native esp32;

esp32.serial_begin(115200);
esp32.delay(300);

esp32.serial_println("=== ESP32 power ===");

cause :: Int64 := esp32.power_wake_cause();
esp32.serial_print("wake_cause = ");
esp32.serial_println(cause);

esp32.pin_mode(2, 1);
through i :: 0..2 -> loop {
    esp32.digital_write(2, 1);
    esp32.delay(200);
    esp32.digital_write(2, 0);
    esp32.delay(200);
};

esp32.serial_println("power: DONE");
esp32.serial_println("deep sleeping for 5 s...");
esp32.delay(50);

esp32.power_wake_timer(5000000);
esp32.power_deep_sleep_us(5000000);

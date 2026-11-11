# esp32_realtime.vy — @realtime deadline telemetry on ESP32.
#
# Requires: VYNE_USE_REALTIME (automatic for @realtime regions)
# Hardware: ESP32-WROOM-32
# Flash:    vynec --esp32 esp32 --esp32-flash --esp32-port COM3 examples/test/esp32_realtime.vy
# Expect:   worst_ms well below 10, overruns = 0, "realtime: PASS"

use native esp32;

esp32.serial_begin(115200);
esp32.delay(300);

esp32.serial_println("=== ESP32 realtime ===");

@realtime<10> region tick {
    # ~5 ms body, inside the 10 ms deadline.
    esp32.delay(5);
    marker :: Int64 := esp32.millis();
};

worst :: Float64 := esp32.realtime_worst_ms();
overruns :: Int64 := esp32.realtime_overruns();

esp32.serial_print("worst_ms = ");
esp32.serial_println(worst);
esp32.serial_print("overruns = ");
esp32.serial_println(overruns);

esp32.realtime_reset();

if (overruns == 0 && worst < 10.0) {
    esp32.serial_println("realtime: PASS");
} else {
    esp32.serial_println("realtime: FAIL");
}

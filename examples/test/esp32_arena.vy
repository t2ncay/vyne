# esp32_arena.vy — checkpoint/rewind and region-drift proof on ESP32.
#
# Requires: none
# Hardware: ESP32-WROOM-32
# Flash:    vynec --esp32 esp32 --esp32-flash --esp32-port COM3 examples/test/esp32_arena.vy
# Expect:   drift = 0 B on every region and "arena: PASS"

use native esp32;

esp32.serial_begin(115200);
esp32.delay(300);

esp32.serial_println("=== ESP32 arena ===");

before_used :: Int64 := esp32.arena_used();
before_peak :: Int64 := esp32.arena_peak();
before_free :: Int64 := esp32.arena_free();

region build {
    values :: Array<Int64> := [];
    through i :: 0..31 -> loop {
        values.push(i);
    };
    esp32.serial_print("pushed = ");
    esp32.serial_print(values.size());
    esp32.serial_println("");
};

after_used :: Int64 := esp32.arena_used();
drift :: Int64 := after_used - before_used;

esp32.serial_print("arena_used   before = ");
esp32.serial_print(before_used);
esp32.serial_print(", after = ");
esp32.serial_print(after_used);
esp32.serial_println(" B");
esp32.serial_print("arena_peak   before = ");
esp32.serial_print(before_peak);
esp32.serial_println(" B");
esp32.serial_print("arena_free   before = ");
esp32.serial_print(before_free);
esp32.serial_println(" B");
esp32.serial_print("region drift        = ");
esp32.serial_print(drift);
esp32.serial_println(" B");

region second {
    scratch tmp :: Float64[8];
    through i :: 0..7 -> loop {
        tmp[i] = 1.0 * i;
    };
};

drift2 :: Int64 := esp32.arena_used() - after_used;
esp32.serial_print("second drift        = ");
esp32.serial_print(drift2);
esp32.serial_println(" B");

if (drift == 0 && drift2 == 0) {
    esp32.serial_println("arena: PASS");
} else {
    esp32.serial_println("arena: FAIL");
}

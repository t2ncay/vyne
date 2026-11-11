# esp32_i2c_scan.vy — I2C address sweep 1..126 on the default bus.
#
# Requires: nothing (a device on SDA/SCL produces hits)
# Hardware: ESP32-WROOM-32, default I2C pins (SDA=21, SCL=22)
# Flash:    vynec --esp32 esp32 --esp32-flash --esp32-port COM3 examples/test/esp32_i2c_scan.vy
# Expect:   "found 0x.." for each responding address, then "i2c scan: DONE"

use native esp32;

esp32.serial_begin(115200);
esp32.delay(300);

esp32.serial_println("=== ESP32 I2C scan ===");

esp32.i2c_begin();
esp32.i2c_set_clock(100000);
esp32.delay(50);

found :: Int64 := 0;

through addr :: 1..126 -> loop {
    status :: Int64 := esp32.i2c_write(addr, 0);
    if (status == 0) {
        esp32.serial_print("found 0x");
        esp32.serial_println(addr);
        found = found + 1;
    }
};

esp32.i2c_end();

esp32.serial_print("devices = ");
esp32.serial_println(found);
esp32.serial_println("i2c scan: DONE");

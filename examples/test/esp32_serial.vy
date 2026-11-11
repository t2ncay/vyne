# esp32_serial.vy — UART0 round-trip on ESP32.
#
# Requires: a serial host connected to UART0
# Hardware: ESP32-WROOM-32
# Flash:    vynec --esp32 esp32 --esp32-flash --esp32-port COM3 examples/test/esp32_serial.vy
# Expect:   echoes each received byte as "rx: <code>" for 5 seconds

use native esp32;

esp32.serial_begin(115200);
esp32.delay(300);

esp32.serial_println("=== ESP32 serial ===");
esp32.serial_println("type characters within 5 seconds");

start :: Int64 := esp32.millis();
now :: Int64 := start;
count :: Int64 := 0;

while ((now - start) < 5000) {
    if (esp32.serial_available() > 0) {
        ch :: Int64 := esp32.serial_read();
        esp32.serial_print("rx: ");
        esp32.serial_println(ch);
        count = count + 1;
    } else {
        esp32.delay(10);
    }
    now = esp32.millis();
}

esp32.serial_print("bytes received = ");
esp32.serial_println(count);
esp32.serial_println("serial: DONE");

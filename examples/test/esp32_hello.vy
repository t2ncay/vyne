# esp32_hello.vy — minimal ESP32 boot proof.
#
# Requires: none (UART0 only)
# Hardware: ESP32-WROOM-32 (any ESP32 board)
# Flash:    vynec --esp32 esp32 --esp32-flash --esp32-port COM3 examples/test/esp32_hello.vy
# Expect:   "=== Vyne ESP32 hello ===" then "hello: PASS"

use native esp32;

esp32.serial_begin(115200);
esp32.delay(300);

esp32.serial_println("=== Vyne ESP32 hello ===");
esp32.serial_println("hello: PASS");

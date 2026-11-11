# esp32_math.vy — math bindings against known values, with delta reporting.
#
# Requires: none
# Hardware: ESP32-WROOM-32
# Flash:    vynec --esp32 esp32 --esp32-flash --esp32-port COM3 examples/test/esp32_math.vy
# Expect:   "math: PASS"; every printed delta is 0 or a small rounding error

use native esp32;

esp32.serial_begin(115200);
esp32.delay(300);

esp32.serial_println("=== ESP32 math ===");

s :: Float64 := esp32.sin(0.0);
c :: Float64 := esp32.cos(0.0);
q :: Float64 := esp32.sqrt(16.0);
p :: Float64 := esp32.pow(2.0, 10.0);
f :: Float64 := esp32.fmod(7.0, 3.0);
a :: Float64 := esp32.atan2(0.0, 1.0);

esp32.serial_print("sin(0)        = ");
esp32.serial_println(s);
esp32.serial_print("cos(0)        = ");
esp32.serial_println(c);
esp32.serial_print("sqrt(16)      = ");
esp32.serial_println(q);
esp32.serial_print("pow(2,10)     = ");
esp32.serial_println(p);
esp32.serial_print("fmod(7,3)     = ");
esp32.serial_println(f);
esp32.serial_print("atan2(0,1)    = ");
esp32.serial_println(a);

esp32.serial_print("delta sin     = ");
esp32.serial_println(s - 0.0);
esp32.serial_print("delta cos     = ");
esp32.serial_println(c - 1.0);
esp32.serial_print("delta sqrt    = ");
esp32.serial_println(q - 4.0);
esp32.serial_print("delta pow     = ");
esp32.serial_println(p - 1024.0);
esp32.serial_print("delta fmod    = ");
esp32.serial_println(f - 1.0);
esp32.serial_print("delta atan2   = ");
esp32.serial_println(a - 0.0);

if (c == 1.0 && q == 4.0 && p == 1024.0 && f == 1.0) {
    esp32.serial_println("math: PASS");
} else {
    esp32.serial_println("math: FAIL");
}

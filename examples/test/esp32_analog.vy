# esp32_analog.vy — ADC read, voltage conversion, oversampling, resolution.
#
# Requires: an analog source on GPIO34 (or leave floating for noise)
# Hardware: ESP32-WROOM-32, GPIO34 is input-only ADC1
# Flash:    vynec --esp32 esp32 --esp32-flash --esp32-port COM3 examples/test/esp32_analog.vy
# Expect:   raw values in 0..4095, voltage in 0..3.3, "analog: PASS"

use native esp32;

esp32.serial_begin(115200);
esp32.delay(300);

esp32.serial_println("=== ESP32 analog ===");

raw12 :: Int64 := esp32.analog_read(34);
volts :: Float64 := esp32.analog_read_voltage(34);
avg :: Float64 := esp32.analog_read_oversampled(34, 8);

esp32.serial_print("raw (12-bit)  = ");
esp32.serial_println(raw12);
esp32.serial_print("voltage       = ");
esp32.serial_println(volts);
esp32.serial_print("oversampled 8 = ");
esp32.serial_println(avg);

esp32.analog_read_resolution(9);
raw9 :: Int64 := esp32.analog_read(34);
esp32.serial_print("raw (9-bit)   = ");
esp32.serial_println(raw9);

esp32.analog_read_resolution(12);
raw12b :: Int64 := esp32.analog_read(34);
esp32.serial_print("raw (12-bit)  = ");
esp32.serial_println(raw12b);

esp32.analog_reference(0);
esp32.analog_reference(1);
esp32.analog_reference(2);
esp32.analog_reference(0);

ok :: Bool := raw12 >= 0 && raw12 <= 4095 && raw9 >= 0 && raw9 <= 511 &&
              volts >= 0.0 && volts <= 3.3;
if (ok) {
    esp32.serial_println("analog: PASS");
} else {
    esp32.serial_println("analog: FAIL");
}

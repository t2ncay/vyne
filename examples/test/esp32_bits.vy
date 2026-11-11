# esp32_bits.vy — bit manipulation and CRC-8 on ESP32.
#
# Requires: none
# Hardware: ESP32-WROOM-32
# Flash:    vynec --esp32 esp32 --esp32-flash --esp32-port COM3 examples/test/esp32_bits.vy
# Expect:   "bits: PASS" and crc8(0x01, 0) = 49 (0x31)

use native esp32;

esp32.serial_begin(115200);
esp32.delay(300);

esp32.serial_println("=== ESP32 bits ===");

set_v :: Int64 := esp32.bit_set(0, 3);
clear_v :: Int64 := esp32.bit_clear(255, 0);
toggle_v :: Int64 := esp32.bit_toggle(0, 2);
read_v :: Int64 := esp32.bit_read(10, 1);
pop_v :: Int64 := esp32.popcount(61680);
crc_v :: Int64 := esp32.crc8(1, 0);
lo_v :: Int64 := esp32.low_byte(4660);
hi_v :: Int64 := esp32.high_byte(4660);
word_v :: Int64 := esp32.make_word(18, 52);

esp32.serial_print("bit_set(0,3)      = ");
esp32.serial_println(set_v);
esp32.serial_print("bit_clear(255,0)  = ");
esp32.serial_println(clear_v);
esp32.serial_print("bit_toggle(0,2)   = ");
esp32.serial_println(toggle_v);
esp32.serial_print("bit_read(10,1)    = ");
esp32.serial_println(read_v);
esp32.serial_print("popcount(0xF0F0)  = ");
esp32.serial_println(pop_v);
esp32.serial_print("crc8(0x01,0)      = ");
esp32.serial_println(crc_v);
esp32.serial_print("low_byte(0x1234)  = ");
esp32.serial_println(lo_v);
esp32.serial_print("high_byte(0x1234) = ");
esp32.serial_println(hi_v);
esp32.serial_print("make_word(18,52)  = ");
esp32.serial_println(word_v);

if (set_v == 8 && clear_v == 254 && toggle_v == 4 && read_v == 1 &&
    pop_v == 8 && crc_v == 49 && lo_v == 52 && hi_v == 18 && word_v == 4660) {
    esp32.serial_println("bits: PASS");
} else {
    esp32.serial_println("bits: FAIL");
}

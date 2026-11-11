# esp32_nvs.vy — write, read-back, erase, read-back-again.
#
# Requires: NVS partition (standard on every ESP32 Arduino build)
# Hardware: ESP32-WROOM-32
# Flash:    vynec --esp32 esp32 --esp32-flash --esp32-port COM3 examples/test/esp32_nvs.vy
# Expect:   "nvs: PASS" with counter=42 and erased default=-1

use native esp32;

esp32.serial_begin(115200);
esp32.delay(300);

esp32.serial_println("=== ESP32 NVS ===");

opened :: Bool := esp32.nvs_open("vyne_test");
esp32.nvs_erase_all();

esp32.nvs_set_int("counter", 42);
esp32.nvs_set_float("scale", 2.5);
esp32.nvs_set_string("name", "vyne");
esp32.nvs_commit();

counter :: Int64 := esp32.nvs_get_int("counter", -1);
scale :: Float64 := esp32.nvs_get_float("scale", -1.0);
has_name :: Bool := esp32.nvs_exists("name");
esp32.nvs_close();

esp32.serial_print("counter = ");
esp32.serial_println(counter);
esp32.serial_print("scale   = ");
esp32.serial_println(scale);
if (has_name) {
    esp32.serial_println("name key: present");
} else {
    esp32.serial_println("name key: missing");
}

# Erase the integer, confirm the default is returned.
esp32.nvs_open("vyne_test");
esp32.nvs_erase_key("counter");
after_erase :: Int64 := esp32.nvs_get_int("counter", -1);
esp32.nvs_close();

esp32.serial_print("after erase = ");
esp32.serial_println(after_erase);

if (opened && counter == 42 && has_name && after_erase == -1) {
    esp32.serial_println("nvs: PASS");
} else {
    esp32.serial_println("nvs: FAIL");
}

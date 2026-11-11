# esp32_wifi_connect.vy — connect using credentials stored in NVS.
#
# Requires: VYNE_USE_WIFI; NVS keys configured = 1, ssid, pass
# Hardware: ESP32-WROOM-32
# Flash:    vynec --esp32 esp32 --esp32-flash --esp32-port COM3 examples/test/esp32_wifi_connect.vy
# Expect:   with "configured" = 1: local IP printed and "wifi connect: PASS".
#           Without it: "wifi connect: SKIP (NVS not configured)".
#
# Provision the namespace once with any NVS sketch, for example:
#   nvs_open("vyne_wifi"); nvs_set_int("configured", 1);
#   nvs_set_string("ssid", "MyNetwork"); nvs_set_string("pass", "secret");
#   nvs_commit();

use native esp32;

esp32.serial_begin(115200);
esp32.delay(300);

esp32.serial_println("=== ESP32 WiFi connect ===");

esp32.nvs_open("vyne_wifi");
configured :: Int64 := esp32.nvs_get_int("configured", 0);
ssid :: String := esp32.nvs_get_string("ssid", "");
password :: String := esp32.nvs_get_string("pass", "");
esp32.nvs_close();

if (configured == 0) {
    esp32.serial_println("wifi connect: SKIP (NVS not configured)");
} else {
    esp32.wifi_mode(1);
    esp32.wifi_connect(ssid, password);

    start :: Int64 := esp32.millis();
    while ((esp32.wifi_status() != 3) && ((esp32.millis() - start) < 20000)) {
        esp32.delay(250);
    }

    ip :: String := esp32.wifi_local_ip();
    rssi :: Int64 := esp32.wifi_rssi();

    esp32.serial_print("status = ");
    esp32.serial_println(esp32.wifi_status());
    esp32.serial_print("ip     = ");
    esp32.serial_println(ip);
    esp32.serial_print("rssi   = ");
    esp32.serial_println(rssi);

    if (esp32.wifi_status() == 3) {
        esp32.serial_println("wifi connect: PASS");
    } else {
        esp32.serial_println("wifi connect: FAIL");
    }
}

# esp32_wifi_http.vy — JSON POST to an endpoint configured in NVS.
#
# Requires: VYNE_USE_WIFI; NVS keys configured = 1, ssid, pass, url
# Hardware: ESP32-WROOM-32
# Flash:    vynec --esp32 esp32 --esp32-flash --esp32-port COM3 examples/test/esp32_wifi_http.vy
# Expect:   with "configured" = 1: status 200 and the response body,
#           then "http: PASS". Without it: "http: SKIP (NVS not configured)".
#
# Example endpoint: http://httpbin.org/post

use native esp32;

esp32.serial_begin(115200);
esp32.delay(300);

esp32.serial_println("=== ESP32 WiFi HTTP ===");

esp32.nvs_open("vyne_wifi");
configured :: Int64 := esp32.nvs_get_int("configured", 0);
ssid :: String := esp32.nvs_get_string("ssid", "");
password :: String := esp32.nvs_get_string("pass", "");
url :: String := esp32.nvs_get_string("url", "http://httpbin.org/post");
esp32.nvs_close();

if (configured == 0) {
    esp32.serial_println("http: SKIP (NVS not configured)");
} else {
    esp32.wifi_mode(1);
    esp32.wifi_connect(ssid, password);

    start :: Int64 := esp32.millis();
    now :: Int64 := start;
    status :: Int64 := esp32.wifi_status();
    while ((status != 3) && ((now - start) < 20000)) {
        esp32.delay(250);
        status = esp32.wifi_status();
        now = esp32.millis();
    }

    if (status == 3) {
        body :: String := esp32.http_post_json(url, "[\"vyne\"]");
        http_status :: Int64 := esp32.http_status();

        esp32.serial_print("status = ");
        esp32.serial_println(http_status);
        esp32.serial_print("body   = ");
        esp32.serial_println(body);

        if (http_status == 200) {
            esp32.serial_println("http: PASS");
        } else {
            esp32.serial_println("http: FAIL");
        }
    } else {
        esp32.serial_println("http: FAIL (WiFi not connected)");
    }
}

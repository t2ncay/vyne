# esp32_wifi_radar.vy — scan WiFi and render the list on the TFT.
#
# Requires: VYNE_USE_WIFI + VYNE_USE_DISPLAY; TFT_eSPI configured
# Hardware: ESP32-WROOM-32 + a configured TFT panel; command selects the verified HW-950 setup
# Flash:    vynec --preset hw950 --esp32-flash --esp32-monitor --esp32-port COM5 examples/test/esp32_wifi_radar.vy
# Expect:   "Scanning WiFi...", then "Found N networks" and one row per
#           SSID on the panel; serial ends with "Scan complete."
#
# wifi_scan() returns strings because the MCU runtime has no Map yet:
#   "<ssid>\t<rssi>\t<channel>\t<secure>"
# The raw row is rendered for now; split on tabs later for a nicer layout.

use native esp32;

const ESP32_DISPLAY_BLACK :: Int64 := 0x0000;

esp32.serial_begin(115200);
esp32.display_init(1);           # 1 = landscape
esp32.display_clear(0x0000);     # black

esp32.display_text(10, 10, "Scanning WiFi...", 0xFFFF, 2);

# Scan.
networks :: Array := esp32.wifi_scan();
count :: Int64 := networks.size();

# Header line. `string(count)` and string concatenation both lower to
# runtime helpers (vyne_to_string / vyne_binop) supplied by the ESP32
# runtime.
esp32.display_clear(ESP32_DISPLAY_BLACK);
found_text :: String := "Found " + string(count) + " networks";
esp32.display_text(10, 10, found_text, 0x07E0, 2);

# Rows.
h :: Int64 := esp32.display_height();
y :: Int64 := 40;

through i :: 0..count-1 -> loop {
    if (y < h - 20) {
        net :: String := networks[i];
        esp32.display_text(10, y, net, 0xFFFF, 1);
        y = y + 15;
    }
};

esp32.serial_println("Scan complete.");

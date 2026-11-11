# esp32_wifi_scan.vy — scan for networks and print SSID/RSSI/channel/security.
#
# Requires: VYNE_USE_WIFI (automatic); WiFi antenna
# Hardware: ESP32-WROOM-32
# Flash:    vynec --esp32 esp32 --esp32-flash --esp32-port COM3 examples/test/esp32_wifi_scan.vy
# Expect:   one "<ssid>\t<rssi>\t<channel>\t<secure>" line per network,
#           then "wifi scan: DONE"
#
# Note: scan results come back as an Array of Strings because the MCU
# value type has no Map container yet. Split on tabs if you need fields.

use native esp32;

esp32.serial_begin(115200);
esp32.delay(300);

esp32.serial_println("=== ESP32 WiFi scan ===");

esp32.wifi_mode(1);
esp32.delay(200);

nets :: Array := esp32.wifi_scan();

esp32.serial_print("networks = ");
esp32.serial_println(nets.size());

through i :: 0..nets.size()-1 -> loop {
    esp32.serial_println(nets[i]);
};

esp32.serial_println("wifi scan: DONE");

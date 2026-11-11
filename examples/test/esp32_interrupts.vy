# esp32_interrupts.vy — GPIO edge counting through the counter-based API.
#
# Requires: a signal source on GPIO2 (button or jumper to 3V3)
# Hardware: ESP32-WROOM-32
# Flash:    vynec --esp32 esp32 --esp32-flash --esp32-port COM3 examples/test/esp32_interrupts.vy
# Expect:   prints the edge count after 5 seconds and "interrupts: DONE"

use native esp32;

esp32.serial_begin(115200);
esp32.delay(300);

esp32.serial_println("=== ESP32 interrupts ===");

esp32.pin_mode(2, 0);
esp32.attach_interrupt(2, 2);
esp32.interrupt_reset(2);

esp32.serial_println("counting rising edges on GPIO2 for 5 s");
esp32.delay(5000);

edges :: Int64 := esp32.interrupt_count(2);
fired :: Bool := esp32.interrupt_fired(2);

esp32.serial_print("edges = ");
esp32.serial_println(edges);
if (fired) {
    esp32.serial_println("edge seen: PASS");
} else {
    esp32.serial_println("edge seen: none (check wiring)");
}

esp32.detach_interrupt(2);
esp32.serial_println("interrupts: DONE");

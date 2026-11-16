# Physical lifecycle regression: existing radio ownership, repeated start/stop.
use native esp32;
esp32.serial_begin(115200);
if esp32.wifi_sniff_start(0) { esp32.serial_println("SNIFF_CYCLE_FAIL invalid channel"); esp32.delay(250); exit(1); }
esp32.wifi_mode(1);
if esp32.wifi_sniff_start(6) { esp32.serial_println("SNIFF_CYCLE_FAIL busy radio"); esp32.delay(250); exit(2); }
esp32.wifi_mode(0);
through cycle :: 1..3 -> loop {
    if !esp32.wifi_sniff_start(6) { esp32.serial_print("SNIFF_CYCLE_FAIL start error="); esp32.serial_println(esp32.wifi_sniff_error()); esp32.delay(250); exit(3); }
    esp32.delay(1000);
    if !esp32.wifi_sniff_sample() { esp32.serial_println("SNIFF_CYCLE_FAIL sample"); esp32.delay(250); exit(4); }
    # Arduino delay is RTOS tick-based; allow a small tick-alignment undershoot.
    if esp32.wifi_sniff_elapsed_us() < 990000 { esp32.serial_println("SNIFF_CYCLE_FAIL time"); esp32.delay(250); exit(5); }
    if !esp32.wifi_sniff_stop() { esp32.serial_print("SNIFF_CYCLE_FAIL stop error="); esp32.serial_println(esp32.wifi_sniff_error()); esp32.delay(250); exit(6); }
    if esp32.wifi_sniff_sample() { esp32.serial_println("SNIFF_CYCLE_FAIL stopped sample"); esp32.delay(250); exit(7); }
    esp32.delay(1000);
    esp32.serial_print("SNIFF_CYCLE cycle="); esp32.serial_print(cycle);
    esp32.serial_print(" heap_free="); esp32.serial_print(esp32.free_ram());
    esp32.serial_print(" arena="); esp32.serial_println(esp32.arena_used());
};
esp32.serial_println("SNIFF_CYCLE_PASS");

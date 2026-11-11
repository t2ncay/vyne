# esp32_smoke.vy — comprehensive ESP32 shared-surface smoke test.
#
# Requires: none (UART0 + one GPIO)
# Hardware: ESP32-WROOM-32, GPIO2 wired to an LED (optional)
# Flash:    vynec --esp32 esp32 --esp32-flash --esp32-port COM3 examples/test/esp32_smoke.vy
# Expect:   every line ends in "PASS" and the final line is "SMOKE: PASS"

use native esp32;

esp32.serial_begin(115200);
esp32.delay(300);

esp32.serial_println("=== ESP32 smoke ===");

# --- GPIO ---
esp32.pin_mode(2, 1);
esp32.digital_write(2, 1);
esp32.delay(50);
esp32.digital_write(2, 0);
gpio_ok :: Bool := esp32.digital_read(2) == 0;
if (gpio_ok) {
    esp32.serial_println("gpio: PASS");
} else {
    esp32.serial_println("gpio: FAIL");
}

# --- Math (exact values only) ---
s :: Float64 := esp32.sin(0.0);
c :: Float64 := esp32.cos(0.0);
q :: Float64 := esp32.sqrt(16.0);
if (s == 0.0) {
    esp32.serial_println("sin: PASS");
} else {
    esp32.serial_println("sin: FAIL");
}
if (c == 1.0) {
    esp32.serial_println("cos: PASS");
} else {
    esp32.serial_println("cos: FAIL");
}
if (q == 4.0) {
    esp32.serial_println("sqrt: PASS");
} else {
    esp32.serial_println("sqrt: FAIL");
}

# --- Arena / region drift ---
before :: Int64 := esp32.arena_used();

region probe {
    scratch buf :: Float64[16];
    through i :: 0..15 -> loop {
        buf[i] = esp32.sin(0.1 * i);
    };
    acc :: Float64 := buf[0];
};

after :: Int64 := esp32.arena_used();
drift :: Int64 := after - before;

esp32.serial_print("arena before = ");
esp32.serial_print(before);
esp32.serial_print(" B, after = ");
esp32.serial_print(after);
esp32.serial_print(" B, drift = ");
esp32.serial_print(drift);
esp32.serial_println(" B");

if (drift == 0) {
    esp32.serial_println("region drift: PASS");
} else {
    esp32.serial_println("region drift: FAIL");
}

esp32.serial_println("SMOKE: PASS");

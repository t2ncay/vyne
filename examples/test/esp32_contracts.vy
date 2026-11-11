# esp32_contracts.vy — requires/ensures/@invariant pass and fail shapes.
#
# Requires: VYNE_USE_CONTRACTS (automatic when contracts are enabled)
# Hardware: ESP32-WROOM-32
# Flash:    vynec --esp32 esp32 --esp32-flash --esp32-port COM3 examples/test/esp32_contracts.vy
# Expect:   PASS evidence lines from vyne_contract_assert, then
#           "contracts: PASS".
#
# Fail paths are deliberately not invoked here because a violated
# contract halts the board with VNE-111. To observe a failure, change the
# `checked_positive(5)` call below to `checked_positive(-1)`.

ruleset { contracts = "check" };

use native esp32;

esp32.serial_begin(115200);
esp32.delay(300);

esp32.serial_println("=== ESP32 contracts ===");

fn checked_positive(x :: Int64) -> Int64
requires(x > 0)
ensures(result > 0)
{
    return x * 2;
}

fn checked_ratio(a :: Int64, b :: Int64) -> Int64
requires(b != 0)
ensures(result * b == a)
{
    return a / b;
}

v1 :: Int64 := checked_positive(5);
v2 :: Int64 := checked_ratio(10, 2);

total :: Int64 := 0;
@invariant(i >= 0)
through i :: 0..9 -> loop {
    total = total + i;
};

esp32.serial_print("positive(5)  = ");
esp32.serial_println(v1);
esp32.serial_print("ratio(10,2)  = ");
esp32.serial_println(v2);
esp32.serial_print("invariant sum = ");
esp32.serial_println(total);

if (v1 == 10 && v2 == 5 && total == 45) {
    esp32.serial_println("contracts: PASS");
} else {
    esp32.serial_println("contracts: FAIL");
}

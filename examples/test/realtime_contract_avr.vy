# realtime_contract_avr.vy — AVR build check for @realtime + contracts.
#
#   ./vynec --arduino uno --arduino-flash examples/test/realtime_contract_avr.vy
#
# Build-only: the firmware must link the MCU realtime and contract
# runtimes (mcu_realtime.h / mcu_contracts.h). On real hardware the
# contract checks and the deadline watchdog run over Serial.

use native arduino;

fn positive(x :: Int64) -> Int64
requires(x > 0)
ensures(result > 0)
{
    return x * 2;
}

arduino.serial_begin(115200);
arduino.serial_print("=== AVR realtime + contracts ===\n");

@realtime<100> region tick {
    v :: Int64 := positive(5);
    arduino.serial_print("value = ");
    arduino.serial_print(v);
    arduino.serial_print("\n");
};

arduino.serial_print("overruns = ");
arduino.serial_print(arduino.realtime_overruns());
arduino.serial_print("\n");

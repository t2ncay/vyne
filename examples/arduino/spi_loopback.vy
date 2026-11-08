# spi_loopback.vy — SPI MOSI->MISO loopback test.
#
# Jumper D11 (MOSI) to D12 (MISO) on the Arduino Uno R3, flash, and open
# the serial monitor at 115200. Each transmitted byte should be echoed
# back on MISO and reported as `ok`.
#
# Uno SPI pinout: MOSI = D11, MISO = D12, SCK = D13, SS = D10.

use native arduino;
use external "vspi/vspi.vy";

arduino.serial_begin(115200);
arduino.delay(200);

arduino.serial_print("=== SPI loopback ===\n");
arduino.serial_print("Jumper D11 (MOSI) to D12 (MISO)\n\n");

vspi.begin();
vspi.set(1000000, 0);

through i :: 0..4 -> loop {
    sent :: Int64 := i * 37 + 1;
    got :: Int64 := vspi.transfer(sent);

    arduino.serial_print("sent ");
    arduino.serial_print(sent);
    arduino.serial_print("  got ");
    arduino.serial_print(got);

    if got == sent {
        arduino.serial_print("  ok\n");
    } else {
        arduino.serial_print("  mismatch\n");
    }
};

vspi.end_transaction();
vspi.end();
arduino.serial_print("\ndone.\n");

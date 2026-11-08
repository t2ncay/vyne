# vspi/Ops.vy - typed SPI master operations over the AVR `SPI` runtime.
#
# Transaction model:
#
#     vspi.begin();
#     vspi.set(1000000, 0);        # 1 MHz, SPI mode 0
#     b :: Int64 := vspi.transfer(0x9F);
#     vspi.end_transaction();
#     vspi.end();
#
# Every entry point validates its arguments before the native call and
# returns an Int64 status instead of halting:
#
#     0      success
#    -1      transfer failed (reserved; the AVR path does not fail)
#    -2      invalid clock (must be > 0)
#    -3      invalid data mode (0..3), byte (0..255), or word (0..65535)

use native arduino;

ruleset { dynamic_casting };

module vspi;

# Power the SPI peripheral and configure the SCK/MOSI/MISO pins.
fn :: vspi begin() -> Int64 {
    arduino.spi_begin();
    return 0;
}

# Release the SPI peripheral.
fn :: vspi end() -> Int64 {
    arduino.spi_end();
    return 0;
}

# Begin a transaction: clock in Hz, data mode 0..3, MSB first.
fn :: vspi set(clock_hz :: Int64, mode :: Int64) -> Int64 {
    if clock_hz <= 0 { return -2; }
    if mode < 0 || mode > 3 { return -3; }
    arduino.spi_set(clock_hz, mode);
    return 0;
}

# End the transaction opened by `set`.
fn :: vspi end_transaction() -> Int64 {
    arduino.spi_end_transaction();
    return 0;
}

# Full-duplex single-byte transfer; returns the byte shifted in on MISO.
fn :: vspi transfer(value :: Int64) -> Int64 {
    if value < 0 || value > 255 { return -3; }
    return arduino.spi_transfer(value);
}

# Full-duplex 16-bit transfer; returns the word shifted in on MISO.
fn :: vspi transfer16(value :: Int64) -> Int64 {
    if value < 0 || value > 65535 { return -3; }
    return arduino.spi_transfer16(value);
}

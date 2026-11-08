# vi2c/Ops.vy - typed I2C master operations over the AVR `Wire` runtime.
#
# Every entry point validates its arguments before the native call and
# returns an Int64 status instead of halting:
#
#     0      success (endTransmission ACK)
#     1..4   endTransmission failure (data too long, NACK address,
#            NACK data, other)
#    -1      read failed (no byte available / register select NACKed)
#    -2      invalid 7-bit device address (not in 0..127)
#    -3      invalid register or byte value (not in 0..255)
#
# Addresses are 7-bit (0x08..0x77 in practice); the 8-bit R/W bit is
# added by the Wire library, so pass 0x68 for an MPU-6050, not 0xD0.

use native arduino;

ruleset { dynamic_casting };

module vi2c;

# Start the TWI peripheral as a bus master.
fn :: vi2c begin() -> Int64 {
    arduino.i2c_begin();
    return 0;
}

# Release the TWI peripheral.
fn :: vi2c end() -> Int64 {
    arduino.i2c_end();
    return 0;
}

# Set the bus clock in Hz. 100000 = standard mode, 400000 = fast mode.
fn :: vi2c set_clock(hz :: Int64) -> Int64 {
    if hz <= 0 { return -2; }
    arduino.i2c_set_clock(hz);
    return 0;
}

# Write one byte to a device. Returns the endTransmission status.
fn :: vi2c write(addr :: Int64, value :: Int64) -> Int64 {
    if addr < 0 || addr > 127 { return -2; }
    if value < 0 || value > 255 { return -3; }
    return arduino.i2c_write(addr, value);
}

# Read one byte from a device. Returns -1 when nothing is available.
fn :: vi2c read(addr :: Int64) -> Int64 {
    if addr < 0 || addr > 127 { return -2; }
    return arduino.i2c_read(addr);
}

# Write one byte to a device register.
fn :: vi2c write_reg(addr :: Int64, reg :: Int64, value :: Int64) -> Int64 {
    if addr < 0 || addr > 127 { return -2; }
    if reg < 0 || reg > 255 { return -3; }
    if value < 0 || value > 255 { return -3; }
    return arduino.i2c_write_reg(addr, reg, value);
}

# Read one byte from a device register using a repeated start.
fn :: vi2c read_reg(addr :: Int64, reg :: Int64) -> Int64 {
    if addr < 0 || addr > 127 { return -2; }
    if reg < 0 || reg > 255 { return -3; }
    return arduino.i2c_read_reg(addr, reg);
}

# Bytes waiting in the Wire receive buffer.
fn :: vi2c available() -> Int64 {
    return arduino.i2c_available();
}

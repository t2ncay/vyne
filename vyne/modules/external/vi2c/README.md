# vi2c - typed I2C for the AVR target

`vi2c` is the typed wrapper over the AVR `Wire` bindings. It validates the
7-bit device address and register/value ranges before the native call and
returns a status code instead of silently truncating to 8 bits.

```vyne
use native arduino;
use external "vi2c/vi2c.vy";

vi2c.begin();
id :: Int64 := vi2c.read_reg(0x68, 0x75);   # MPU-6050 WHO_AM_I
vi2c.end();
```

## Requirements

- Target: Arduino Uno R3 (`vynec --arduino uno ...`). Desktop builds do not
  define `arduino_i2c_*`.
- The AVR build integration compiles the bundled `Wire` library and adds
  `vendor/arduino/libraries/Wire/src` to the include path. This happens
  automatically; no extra flag is needed.
- Wiring: SDA = A4, SCL = A5 on the Uno, with pull-ups (most breakout
  boards include them).

## Return codes

| Code | Meaning |
| ---: | :--- |
| `0` | Success (`Wire.endTransmission` ACK) |
| `1` | Data too long for the 32-byte TWI buffer |
| `2` | NACK on address |
| `3` | NACK on data |
| `4` | Other TWI error |
| `-1` | Read failed: no byte available or register select NACKed |
| `-2` | Invalid 7-bit device address (must be 0..127) |
| `-3` | Invalid register or byte value (must be 0..255) |

## API

| Function | Returns | Notes |
| :--- | :--- | :--- |
| `begin()` | `Int64` | Starts the bus master |
| `end()` | `Int64` | Releases the peripheral |
| `set_clock(hz)` | `Int64` | 100 kHz / 400 kHz typical |
| `write(addr, value)` | `Int64` | One data byte |
| `read(addr)` | `Int64` | One byte, or `-1` |
| `write_reg(addr, reg, value)` | `Int64` | Register write |
| `read_reg(addr, reg)` | `Int64` | Repeated-start register read |
| `available()` | `Int64` | Bytes in the receive buffer |

Addresses are **7-bit**: pass `0x68` for an MPU-6050, not the 8-bit `0xD0`.
The Wire library adds the R/W bit.

## Scope

This first slice exposes single-byte transfers, which covers identity
probes, configuration registers, and most sensor reads. Block transfers
(`write_bytes`, `read_bytes`) are a follow-up; they need an array-return
ABI on the MCU runtime before they can be typed cleanly.

The example `examples/arduino/i2c_imu.vy` reads an MPU-6050 `WHO_AM_I`
and prints it over serial.

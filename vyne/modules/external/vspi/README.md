# vspi - typed SPI for the AVR target

`vspi` is the typed wrapper over the AVR `SPI` bindings. It validates the
clock, data mode, and byte/word ranges before the native call, so a bad
constant produces a status code instead of a silently truncated transfer.

```vyne
use native arduino;
use external "vspi/vspi.vy";

vspi.begin();
vspi.set(1000000, 0);              # 1 MHz, mode 0
id :: Int64 := vspi.transfer(0x9F); # e.g. JEDEC ID on a flash chip
vspi.end_transaction();
vspi.end();
```

## Requirements

- Target: Arduino Uno R3 (`vynec --arduino uno ...`). Desktop builds do not
  define `arduino_spi_*`.
- The AVR build integration compiles the bundled `SPI` library and adds
  `vendor/arduino/libraries/SPI/src` to the include path automatically.
- Uno pinout: MOSI = D11, MISO = D12, SCK = D13, SS = D10 (drive SS
  manually for the device you are talking to).

## Return codes

| Code | Meaning |
| ---: | :--- |
| `0` | Success |
| `-1` | Transfer failed (reserved; the AVR path does not fail) |
| `-2` | Invalid clock (must be > 0) |
| `-3` | Invalid mode (0..3), byte (0..255), or word (0..65535) |

## API

| Function | Returns | Notes |
| :--- | :--- | :--- |
| `begin()` | `Int64` | Powers the peripheral |
| `end()` | `Int64` | Releases it |
| `set(clock_hz, mode)` | `Int64` | Begins a transaction (MSB first) |
| `end_transaction()` | `Int64` | Ends the transaction |
| `transfer(value)` | `Int64` | 8-bit full-duplex |
| `transfer16(value)` | `Int64` | 16-bit full-duplex |

## Loopback test

`examples/arduino/spi_loopback.vy` sends a pattern and compares the received
byte. Jumper **D11 (MOSI) to D12 (MISO)**, flash, and open the serial
monitor at 115200. With the jumper removed the received bytes are not a
reliable echo, which is expected.

## Scope

This first slice exposes byte and word transfers. Multi-byte block transfers
(`transfer_bytes`) need a buffer ABI on the MCU runtime and are a follow-up.

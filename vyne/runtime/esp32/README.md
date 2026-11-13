# Vyne ESP32 Runtime

> The C++ runtime that ships with every Vyne-generated ESP32 firmware.
> A peer of `vyne/runtime/arduino/`, sharing the same Vyne-level surface
> where the hardware semantics line up.

## Overview

`vyne_esp32.h` is the single translation unit the generated `.ino`
includes. It defines `setup()` / `loop()` and dispatches them into the
Vyne program's `vyne_setup()` / `vyne_loop()`, then pulls in the detail
headers the sketch actually uses.

Optional peripherals are gated by `VYNE_USE_<FEATURE>` defines emitted by
the compiler when a matching native call is lowered:

| Define               | Header            | Peripheral                          |
| :------------------- | :---------------- | :---------------------------------- |
| `VYNE_USE_I2C`       | `mcu_i2c.h`       | Wire                                |
| `VYNE_USE_SPI`       | `mcu_spi.h`       | SPI                                 |
| `VYNE_USE_REALTIME`  | `mcu_realtime.h`  | `@realtime` telemetry               |
| `VYNE_USE_CONTRACTS` | `mcu_contracts.h` | `requires` / `ensures` / `@invariant` |
| `VYNE_USE_NVS`       | `mcu_nvs.h`       | Preferences / NVS                   |
| `VYNE_USE_WIFI`      | `mcu_wifi.h`      | WiFi + HTTPClient                   |
| `VYNE_USE_DISPLAY`   | `mcu_display.h`   | TFT_eSPI                            |
| `VYNE_USE_TOUCH`     | `mcu_touch.h`     | TFT_eSPI or configurable-SPI XPT2046 |
| `VYNE_USE_IMU`       | `mcu_imu.h`       | MPU6050 over Wire                   |
| `VYNE_USE_POWER`     | `mcu_power.h`     | Deep/light sleep, reset             |

A sketch that never calls `esp32.wifi_*` never includes `<WiFi.h>` and
never pays the ~50 KB static heap the WiFi driver reserves on `begin()`.

## Building

The compiler delegates to `arduino-cli`; it does not reproduce the
xtensa toolchain invocation by hand:

```sh
vynec --esp32 esp32 --esp32-flash --esp32-port COM3 examples/test/esp32_hello.vy
```

Prerequisites:

1. `arduino-cli core install esp32:esp32`
2. TFT_eSPI plus a matching per-build profile or `User_Setup.h` for display/touch.
3. An MPU6050 on I2C if you use the IMU surface.

The build stages this directory next to the generated `.ino` inside a
throwaway `<base>_sketch/` directory, so no `-I` flags are needed. This
avoids cmd.exe quoting problems when the Vyne install path contains
spaces.

## Surface

`use native esp32;` exposes both surfaces:

- Shared names (`esp32.pin_mode`, `esp32.delay`, `esp32.serial_*`,
  `esp32.i2c_*`, `esp32.spi_*`, math, bits, memory, interrupts,
  realtime telemetry) map to the same `arduino_*` C symbols as AVR.
- ESP32-only names (`esp32.nvs_*`, `esp32.wifi_*`, `esp32.http_*`,
  `esp32.display_*`, `esp32.display_sprite_*`, `esp32.touch_screen_*`,
  `esp32.imu_*`,
  `esp32.power_*`, `esp32.dac_*`, `esp32.touch_pin_*`,
  `esp32.heap_largest_free`, `esp32.serial1_*`) map to `esp32_*` C
  symbols.

`use native arduino;` on the ESP32 target resolves against the shared
surface only. EEPROM names are deliberately absent: use NVS.

## Deviations from the AVR runtime

These are intentional and documented at the relevant header:

- **`mcu_interrupts.h` is adapted, not copied.** ESP32 has no
  `<avr/wdt.h>` and no Timer1; the implementation uses up to eight GPIO
  trampolines and the ESP-IDF Task Watchdog. The Vyne-level API
  (`attach_interrupt`, `interrupt_count`, `watchdog_*`) is unchanged.
- **`wifi_scan()` returns `Array` of `String`, not `Array<Map>`.**
  The MCU value union has no map member yet; each entry is
  `"<ssid>\t<rssi>\t<channel>\t<secure>"`.
- **`imu_read()` packs 7 x 9 bits.** The requested 7 x 10-bit layout is
  70 bits and cannot fit in an Int64; individual accessors remain the
  precise interface.
- **`analog_reference()` maps to ADC attenuation.** ESP32 has no
  selectable voltage reference, so modes 0/1/2 select 11 dB / 2.5 dB /
  0 dB attenuation.
- **`analog_write()` uses LEDC.** Arduino-ESP32 3.x
  `ledcAttach(pin, freq, bits)` is used; the 2.x channel API is retained
  behind a version check.
- **`timer_read()` returns `esp_timer_get_time()`.** A 64-bit
  microsecond monotonic clock, not a raw Timer1 counter.
- **The default arena is 64 KB, not 128 KB.** The ESP32 linker's
  `dram0_0_seg` cannot fit a 128 KB static array in a WiFi-enabled
  `esp32:esp32:esp32` image. Override with `-DVYNE_MCU_ARENA_SIZE`.
- **`mcu_operators.h` supplies boxed operators.** The compiler lowers
  dynamic arithmetic to `vyne_binop`, sizes to `vyne_get_sizeof`, and
  dynamic indexing to `vyne_index_get`; the shared `mcu_types.h` is
  minimal, so the ESP32 runtime provides them.
- **Touch compiles as inert stubs when neither `TOUCH_CS` nor `VYNE_TOUCH_XPT2046` is configured.**
  The library omits `getTouch`/`setTouch` in that configuration, so the
  Vyne surface stays linkable and reports "no contact" until the user
   configures a touch driver. The XPT2046 path supports its own SPI pins and bus.

## Display and touch setup

Use a profile matching the actual panel. For the owner's **HW-950** board,
[`profiles/hw950-screen-rgb.json`](profiles/hw950-screen-rgb.json) is the
visually tested configuration: ST7796-compatible initialization, explicitly
measured 240x320 dimensions (320x240 landscape), RGB order, and vertical
mirror correction. A driver define reports configuration, not detected IC
identity: raw display-ID reads on this board returned zero.

The profile uses display HSPI pins MOSI13/MISO12/SCLK14/CS15/DC2, no dedicated
reset pin, active-high backlight GPIO21 plus the owner-confirmed GPIO27
enable, and XPT2046 CS33/IRQ36 on the shared HSPI data/clock pins. Other CYD
variants can use different touch wiring; `cyd-ili9341-rgb.json` retains the
user-supplied ILI9341/separate-touch-bus reference configuration.

`--esp32-build-property <key=value>` forwards overrides to Arduino CLI.
Profiles use **`compiler.cpp.extra_flags`**, preserving the platform's
`build.extra_flags` (`ESP32`, USB, and core definitions). Replacing
`build.extra_flags` with only TFT defines previously removed `ESP32` and
caused incorrect platform code selection and a lit-but-blank screen.

See [hardware testing](HARDWARE_TESTING.md) for direct Vyne commands,
calibration, the screen demo, and the current COM5 results. The generic
external ILI9488 profile is not the HW-950 integrated-board pinout.

### Double buffering

`display_sprite_create(w, h)` returns a handle to an off-screen
`TFT_eSprite` (up to four). `display_sprite_select(handle)` routes the
normal `display_*` drawing calls into the sprite; `display_sprite_push`
blits it to the panel; `display_sprite_select(-1)` returns to direct
drawing. A 16-bit 240x320 sprite needs about 150 KB of heap, so create
sprites after WiFi is initialized and keep only what you need.

## Tests

`examples/test/esp32_*.vy` contains one sketch per area. Each begins
with a `Requires` / `Hardware` / `Flash` / `Expect` comment block. Start
with `esp32_hello.vy`, then `esp32_smoke.vy`; the rest are
peripheral-specific and can be skipped when the hardware is absent.

`python scripts/test_esp32.py` builds all examples and both native fixtures.
Add `--run` to flash the core suite plus the runtime regression on COM5,
or give explicit suffixes such as `--run display wifi_radar`.
The runner captures setup output after reset, checks failure diagnostics,
and writes per-test logs, `run.json`, and `results.json` outside the source
tree. No-contact/device/credential cases are reported as skips.

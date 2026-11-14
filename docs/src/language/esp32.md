---
id: esp32
title: ESP32 & Hardware
group: Language Reference
icon: fa-microchip
order: 16
description: ESP32 firmware, select-once hardware presets, display and touch, networking, storage, sensors, and runtime budgets.
---

# ESP32 & Hardware

The ESP32 backend emits an Arduino-ESP32 C++ sketch and builds it with
`arduino-cli`. Use `use native esp32;` for GPIO, serial, math, memory, buses,
interrupts, and realtime telemetry, plus ESP32-specific peripherals. Optional
headers are included only when a matching call is lowered; a program without
Wi-Fi calls does not include the Wi-Fi library.

## Select hardware once

```shell
vynec init screen_demo --name screen_demo --kind embedded --preset hw950 --port COM5 --no-git
cd screen_demo
vynec hardware setup
vynec hardware show
vynec build
vynec run
vynec run --monitor
```

`hardware setup` installs the selected core/libraries and requires `arduino-cli`.
`build` compiles without uploading. `run` builds, uploads, resets, and starts the
board; monitoring is opt-in. Replace the port with your connected device.
Artifacts live under `build/firmware/`.

Generic boards use `--board esp32`, `esp32s2`, `esp32s3`, `esp32c3`, or `esp32c6`.
They have no display by default. `--preset hw950` selects the measured integrated
display/touch configuration. Board-specific pins/peripherals still depend on
the selected chip and wiring.

```toml title="hardware.toml"
[board]
preset = "hw950"

[connection]
port = "COM5"
baud = 115200

[display]
rotation = 1
```

Precedence is **CLI overrides → hardware.toml → preset defaults**.
`vynec hardware list` lists bundled labels; `hardware show` prints resolved TOML.
Use `--hardware-set display.rotation=2` for a repeatable typed override. Changed
hardware definitions trigger a clean object rebuild to prevent stale firmware.
See [Projects & Tooling](page:projects) for validation and configuration details.

## Minimal serial program

```vyne title="hello-esp32.vy"
use native esp32;
esp32.serial_begin(115200);
esp32.serial_println("Hello from Vyne on ESP32");
esp32.serial_println(esp32.arena_used());
```

Inside a configured project, run `vynec run hello-esp32.vy --monitor`.
For standalone source, the lower-level workflow is:

```shell
vynec --esp32 esp32 --esp32-flash --esp32-port COM3 examples/test/esp32_hello.vy
```

## Runtime and memory

The generated sketch dispatches Arduino `setup()` / `loop()` into the Vyne
program. The default static arena is **64 KB**, with checkpoint/rewind semantics
shared with desktop. This leaves space for the original ESP32 linker layout;
Wi-Fi and display drivers consume additional heap outside the Vyne arena.
`Float64` remains double precision on ESP32, unlike AVR's 32-bit double.

`@bounded`, `@zeroed`, `@deterministic`, and `@realtime` use the MCU runtime;
contracts and telemetry have feature-gated headers. `scratch` uses stack space.
Avoid large scratch buffers and repeatedly growing arrays without a region.
Heap/sprite usage is separate from `esp32.arena_used()` and a region budget.
Desktop device/commit/streamed policies are not an ESP32 acceleration mechanism.

## Peripheral map

All names below are called as `esp32.<name>(...)`. Consult the linked example
for exact argument order and wiring; the runtime headers are the signature source.

| Area | Representative bindings | Example |
| :--- | :--- | :--- |
| GPIO and ADC | `pin_mode`, `digital_write`, `digital_read`, `analog_read`, `analog_write`, `analog_reference` | [analog](https://github.com/t2ncay/vyne/blob/master/examples/test/esp32_analog.vy) |
| DAC/capacitive touch | `dac_write`, `dac_write_mv`, `touch_pin_read` | [runtime map](https://github.com/t2ncay/vyne/blob/master/vyne/compiler/codegen/native_maps.h) |
| Serial | `serial_begin`, `serial_println`, `serial_available`, `serial_read`, `serial1_begin`, `serial1_read` | [serial](https://github.com/t2ncay/vyne/blob/master/examples/test/esp32_serial.vy) |
| Math/bits/memory | shared math, bit helpers, `arena_used`, `arena_peak`, `heap_largest_free` | [math](https://github.com/t2ncay/vyne/blob/master/examples/test/esp32_math.vy) |
| I2C/SPI | `i2c_begin`, `i2c_write`, shared register/transfer helpers | [I2C scan](https://github.com/t2ncay/vyne/blob/master/examples/test/esp32_i2c_scan.vy) |
| Interrupts/watchdog | `attach_interrupt`, `interrupt_count`, `interrupt_reset`, `watchdog_*` | [interrupts](https://github.com/t2ncay/vyne/blob/master/examples/test/esp32_interrupts.vy) |
| NVS storage | `nvs_open`, `nvs_set_int`, `nvs_get_int`, float/string variants, `nvs_exists`, `nvs_erase_key`, `nvs_close` | [NVS](https://github.com/t2ncay/vyne/blob/master/examples/test/esp32_nvs.vy) |
| Wi-Fi | `wifi_connect`, `wifi_status`, `wifi_local_ip`, `wifi_rssi`, `wifi_scan`, `wifi_disconnect` | [Wi-Fi scan](https://github.com/t2ncay/vyne/blob/master/examples/test/esp32_wifi_scan.vy) |
| HTTP | `http_get`, `http_post`, `http_post_json`, `http_status` | [HTTP](https://github.com/t2ncay/vyne/blob/master/examples/test/esp32_wifi_http.vy) |
| Display | `display_init`, `display_width`, `display_height`, `display_rotation`, drawing and sprite operations | [display](https://github.com/t2ncay/vyne/blob/master/examples/test/esp32_display.vy) |
| Screen touch | `touch_screen_init`, `touch_screen_pressed`, `touch_screen_x`, `touch_screen_y`, `touch_screen_calibrate` | [touch](https://github.com/t2ncay/vyne/blob/master/examples/test/esp32_touch.vy) |
| IMU | `imu_init`, `imu_ok`, individual acceleration/gyro/temperature accessors, `imu_read` | [MPU6050](https://github.com/t2ncay/vyne/blob/master/examples/test/esp32_imu.vy) |
| Power | `power_deep_sleep_us`, `power_light_sleep_us`, `power_wake_timer`, `power_wake_pin`, `power_wake_cause`, `power_restart` | [power](https://github.com/t2ncay/vyne/blob/master/examples/test/esp32_power.vy) |

`use native arduino;` on ESP32 exposes shared names, but not EEPROM: use NVS.
Interrupts are counter-based rather than arbitrary Vyne ISR callbacks. Timer
reads use a 64-bit microsecond clock. ADC reference selects attenuation; PWM uses
LEDC. `wifi_scan()` returns tab-separated strings (`ssid`, RSSI, channel, secure),
not maps. `imu_read()` packs seven 9-bit fields; individual accessors retain precision.

## HW-950 display and touch

The measured preset uses ST7796-compatible initialization, native **240×320 RGB**
geometry (**320×240 landscape**), vertical mirror correction, GPIO21 backlight,
GPIO27 panel enable, and XPT2046 touch sharing HSPI data/clock pins. This describes
the tested board configuration, not every CYD variant or detected panel identity.

`esp32.display_init()` uses resolved rotation; `display_init(0..3)` overrides it
explicitly. The initialized starter shows geometry/color markers and continuously
polls touch. `touch_screen_configured()` and `touch_screen_calibrated()` distinguish
driver configuration from saved calibration. Send **C** through the monitor for
explicit four-corner calibration; existing NVS calibration is otherwise preserved.
A missing touch configuration reports no contact.

### Sprites and live charts

`display_sprite_create(w,h)` returns an off-screen sprite handle (up to four).
Select it with `display_sprite_select(handle)`, draw using the normal display
calls, and push it with `display_sprite_push`. Select `-1` for direct panel drawing
and delete sprites when finished. A 16-bit 240×320 sprite needs about **150 KB**
of heap; allocate after Wi-Fi initialization and budget separately from the arena.

For a managed chart starter:

```shell
vynec init dashboard --name dashboard --kind embedded --preset hw950 --libraries vplot --port COM5
cd dashboard
vynec hardware setup
vynec run
```

[vplot](page:vplot) supplies colorful charts, bounded live-series storage, and
automatic timestamp/redraw management. The
[plotter project](https://github.com/t2ncay/vyne/tree/master/examples/projects/plotter-esp32)
includes the HW-950 dashboard and touch examples.

## Verification

```shell
# Host configuration tests; no board required
python scripts/test_hardware.py
# Compile fresh initialized projects; requires Arduino toolchains
python scripts/test_hardware.py --compile
# Board-specific acceptance; replace COM5 as appropriate
python scripts/test_hardware.py --compile --port COM5
python examples/projects/plotter-esp32/tests/run.py --esp32 --port COM5 --seconds 60
```

Per-peripheral sketches carry their hardware requirements in comments.
See [runtime details](https://github.com/t2ncay/vyne/blob/master/vyne/runtime/esp32/README.md)
and [hardware acceptance](https://github.com/t2ncay/vyne/blob/master/vyne/runtime/esp32/HARDWARE_TESTING.md).

## See also

- [Arduino & AVR](page:arduino)
- [Projects & Tooling](page:projects) and [CLI Reference](page:cli)
- [vplot](page:vplot) and [Memory & Regions](page:regions)

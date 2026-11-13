# plotter-esp32

An embedded Vyne project for **HW-950 measured owner revision: ST7796-compatible 240x320 RGB, landscape, corrected Y, shared-HSPI XPT2046**.

## One-time setup

Install `arduino-cli`, then run:

```sh
vynec hardware setup
vynec hardware show
```

Set your board's USB port in `[connection]` in `hardware.toml`.

## Build and run

```sh
vynec build          # compile firmware
vynec run            # compile, upload, reset and start
vynec run --monitor  # explicitly open serial monitoring after upload
```

Firmware, sketches and compiler intermediates are under `build/firmware/`.
The Uno starter blinks pin 13; generic ESP32 prints serial greetings; HW-950
initializes the measured display in landscape and continuously draws green
dots on touch contact. Corner labels and RGB bars make orientation/color
checks straightforward. Serial output reports rotation, dimensions, touch
configuration, calibration status, contacts and periodic memory heartbeats.

Send **C** through `vynec run --monitor` to explicitly start four-corner touch
calibration. Hold and release each magenta target. Existing calibration is
loaded from NVS; initialization does not erase or replace it automatically.

## Hardware configuration

`hardware.toml` is functional configuration. A preset supplies board, display,
touch, pins and orientation. JSON presets are bundled compiler data.
Precedence is **CLI overrides → TOML overrides → preset defaults**.

```sh
vynec hardware list
vynec hardware show --hardware-set display.rotation=1
vynec build --hardware-set display.rotation=1
vynec run --port COM5
```

Advanced overrides use `[display]` and `[touch]` tables in `hardware.toml`;
see the resolved report for the supported field names. Display dimensions
are native portrait dimensions; select landscape with rotation.
Legacy Uno board, pinout and memory-budget metadata are accepted.

`esp32.display_init()` uses the configured rotation. An explicit
`esp32.display_init(0..3)` overrides that default. The HW-950 preset uses the
same ST7796/HSPI/GPIO27-enable/Y-correction settings as the verified standalone
examples. `mirror_y = true` is a panel correction, not a request to display
upside-down text. Hardware-option changes invalidate Arduino's cached objects.

| Path | Contents |
| --- | --- |
| `src/main.vy` | board-appropriate bounded starter |
| `hardware.toml` | selected hardware and connection |
| `project.toml` | project target and toolchain |
| `build/firmware/` | generated files |

Documentation: <https://github.com/t2ncay/vyne>

License: AGPL-3.0-only


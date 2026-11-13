# {{name}}

An embedded Vyne project for **{{board_label}}**.

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
initializes the measured display and draws green dots on touch contact for
ten seconds. Calibrate the physical touchscreen for accurate coordinates.

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

| Path | Contents |
| --- | --- |
| `src/main.vy` | board-appropriate bounded starter |
| `hardware.toml` | selected hardware and connection |
| `project.toml` | project target and toolchain |
| `build/firmware/` | generated files |

Documentation: <https://github.com/t2ncay/vyne>
{{#if license_spdx}}
License: {{license_spdx}}
{{/if}}

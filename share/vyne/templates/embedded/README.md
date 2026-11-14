# {{name}}

An embedded Vyne project for **{{board_label}}**.

{{#if vplot_starter}}
## Your first chart

The generated program is ready to draw:

```vyne
use external "vplot/vplot.vy";
xs := [0, 1, 2, 3, 4, 5];
ys := [22.0, 23.4, 21.8, 24.1, 23.0, 25.2];
vplot.show_line(xs, ys, "{{name}}");
```

After one-time hardware setup, run `vynec run`. Display initialization, theme,
axis ranges, color, reusable buffering and temporary cleanup are automatic.
No display/surface setup is needed in application code.

For live values, create `chart := vplot.live_chart("Temperature")` and call
`vplot.push(chart, value)` whenever you obtain a sample. Timestamps and redraw
throttling are automatic; sampling cadence is yours. Close managed charts with
`vplot.close(chart)` when finished.
{{/if}}

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
{{#if hw950_touch}}
The HW-950 touch starter
initializes the measured display in landscape and continuously draws green
dots on touch contact. Corner labels and RGB bars make orientation/color
checks straightforward. Serial output reports rotation, dimensions, touch
configuration, calibration status, contacts and periodic memory heartbeats.

Send **C** through `vynec run --monitor` to explicitly start four-corner touch
calibration. Hold and release each magenta target. Existing calibration is
loaded from NVS; initialization does not erase or replace it automatically.
{{/if}}
{{#if uno}}
The Uno starter blinks pin 13 and prints serial greetings.
{{/if}}
{{#if generic_esp32}}
The generic ESP32 starter prints serial greetings. Configure a physical display
before using display chart helpers.
{{/if}}

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
{{#if license_spdx}}
License: {{license_spdx}}
{{/if}}

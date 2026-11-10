# rv_spc

An embedded Vyne project for the Arduino Uno R3. It blinks the
built-in LED five times inside a `@bounded<512>` region and reports
progress over serial at 9600 baud.

## Build

```bash
vynec --arduino uno src/main.vy
```

This emits the AVR firmware. Upload it with:

```bash
vynec --arduino uno --flash --port COM3 src/main.vy
```

Then open the serial monitor:

```bash
vynec --arduino uno --monitor --port COM3 src/main.vy
```

`hardware.toml` records the board, pinout, and the flash/SRAM budget
that `vynec verify` checks in a later milestone.

## Layout

| Path | Contents |
| --- | --- |
| `src/main.vy` | blinky demo and bounded region |
| `hardware.toml` | board, pinout, memory budget |
| `examples/` | serial hello and a bounded-array example |
| `project.toml` | target and toolchain configuration |

## Learn more

- Vyne repository: <https://github.com/t2ncay/vyne>
- Arduino runtime: `vyne/runtime/arduino/README.md`

- License: AGPL-3.0-only


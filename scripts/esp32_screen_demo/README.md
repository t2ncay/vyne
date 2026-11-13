# ESP32 screen diagnostic

`scripts/test_esp32_screen.py` compiles and flashes this standalone TFT_eSPI
sketch from your PC. The sketch runs on the classic ESP32 and cycles every
four seconds through red, green, blue, white, black, a grid, text, and bounds.
It repeats until reset or replaced with another firmware.

## Run on COM5

Requirements: Arduino CLI, `esp32:esp32`, TFT_eSPI, and Python with pyserial.
Close other serial monitors before starting.

```powershell
python scripts/test_esp32_screen.py --port COM5
```

The launcher shows serial messages live for 40 seconds. Watch the panel
during this window. In an interactive terminal it then asks what appeared
and whether the backlight glowed. Firmware execution and physical screen
results are recorded separately; serial markers do not prove visible pixels.
For unattended runs, use `--non-interactive`; the visual result stays unknown.

```powershell
# Hold a white screen instead of cycling
python scripts/test_esp32_screen.py --pattern white --seconds 15

# Build only, without changing board firmware
python scripts/test_esp32_screen.py --compile-only

# Retry at a lower SPI clock
python scripts/test_esp32_screen.py --spi-frequency 1000000

# Hold the numbered bounds markers
python scripts/test_esp32_screen.py --pattern bounds --seconds 15
```

The default profile is `vyne/runtime/esp32/profiles/hw950-screen-rgb.json`.
Its 320x240 landscape canvas, RGB colors, and orientation were confirmed
visually on the owner's HW-950 board. It uses ST7796-compatible initialization
with explicit 240x320 dimensions; it does not assert a detected controller ID.
The launcher disables `TOUCH_CS` and `VYNE_TOUCH_XPT2046` during screen-only
testing. The saved profile and installed library setup are not changed.

Flags are passed as `compiler.cpp.extra_flags` to preserve Arduino's ESP32
platform definitions. Overriding `build.extra_flags` with only TFT flags
removed the `ESP32` define and caused the original lit-but-blank behavior.

## Backlight

The owner's profile enables GPIO21 (active HIGH) and the confirmed GPIO27
auxiliary enable. Other profiles can leave backlight control unset; then
the firmware reports `SCREEN_BACKLIGHT:NOT_CONFIGURED`. Directly powered
backlights do not need a GPIO override.

Once you identify a GPIO-controlled backlight, pass its actual GPIO number
with `--backlight-pin` and its active level with `--backlight-active high`
or `low`. For example, replace `CONFIRMED_GPIO` below with the verified pin:

```text
python scripts/test_esp32_screen.py --backlight-pin CONFIRMED_GPIO --backlight-active high
```

The launcher rejects backlight pins that collide with the configured SPI
or display-control pins, flash pins, UART0, or input-only GPIOs. It uses a
digital on level for this diagnostic, not PWM. The demo must not be used to
guess the board's backlight wiring by trying arbitrary GPIOs.
`--enable-pin` can supply an independently confirmed auxiliary HIGH enable.

## Evidence and interpretation

- `SCREEN_CONFIG` and `SCREEN_PINS`: compiled driver/dimensions/wiring, not
  a detected hardware identity or panel readback.
- `SCREEN_ID_D3`: raw readback only. All-zero/all-high values are inconclusive.
- `SCREEN_DEMO_READY`: display initialization returned.
- `SCREEN_STAGE`: a pattern's drawing commands executed.
- Firmware PASS: ready plus the requested stage(s), without panic/failure.
- Visual `all`: owner saw the requested pattern(s) correctly.
- Visual `partial`: some visible graphics; inspect colors, clipping, or wiring.
- Visual `blank`: lit panel with no graphics; check driver and signal wiring.
- Visual `dark`: no glow; check backlight power/control and connections.
- Visual `unknown`: physical output has not been confirmed.

For a dark screen, shining a flashlight across it may reveal faint graphics
if drawing works but the backlight does not. A blank/dark result alone does
not establish that the panel is damaged.

Logs and `results.json` go to `<system temp>/opencode/vyne-esp32-screen`;
choose another directory with `--output`. `--build-timeout` controls the
per-build deadline. The script leaves the demo running on the board.

The sketch also accepts serial keys: `r/g/b/w/k/c/t` hold red/green/blue/
white/black/grid/text, `d` holds bounds, and `a` resumes automatic cycling. The launcher's
`--pattern` option uses the same protocol.

After this direct TFT_eSPI demo visibly works, compare with Vyne using the
same confirmed profile and `examples/test/esp32_display.vy`.

## Current COM5 observation

The owner confirmed correct colors, readable orientation, and all four
markers with the measured 320x240 canvas. A local photo showed that the
320x240 rectangle fits the panel; a 480x320 configuration wrapped an extra
line and put three calibration targets outside the visible area. The
profile retains the working RGB/initialization settings and corrects the
dimensions. The text stage has no intentional underline or outer border;
the grid/bounds stages have deliberate borders.

## Launcher checks

```powershell
python -m unittest discover -s scripts/tests -p "test_esp32_screen.py" -v
```

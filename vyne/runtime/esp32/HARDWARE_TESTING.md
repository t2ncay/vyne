# ESP32 hardware testing and the owner's HW-950 setup

## Requirements

- Build Vyne with `xmake -y`.
- Arduino CLI plus `esp32:esp32` (tested CLI 1.5.1 / core 3.3.12).
- TFT_eSPI (tested 2.5.43).
- Python 3 and `python -m pip install pyserial` for hardware runs.
- Close other COM5 serial monitors before uploading or capturing output.

The chip on COM5 is an ESP32-D0WD-V3, revision 3.1, with 4 MB flash.
Use `--esp32 esp32` / `esp32:esp32:esp32`. Other supported ESP32 aliases
are not physically validated by this session.

## The working HW-950 profile

Use [`profiles/hw950-screen-rgb.json`](profiles/hw950-screen-rgb.json).
The owner supplied local front/back photos and participated in the visual
and touch tests. The final profile is based on these observations, rather
than assuming the board's PCB marking uniquely determines its controller:

- **Measured panel:** 240x320 portrait / **320x240 landscape**.
- **Working initialization:** `ST7796_DRIVER` with explicit `TFT_WIDTH=240`
  and `TFT_HEIGHT=320`, RGB order, and logical vertical mirror correction.
- **RGB:** `TFT_RGB_ORDER=1` leaves the MADCTL BGR bit clear.
- **Orientation:** `VYNE_DISPLAY_MIRROR_Y=1` is re-applied after rotation.
- **Display SPI:** HSPI, MOSI13 / MISO12 / SCLK14 / CS15 / DC2 / RST=-1.
- **Power/control:** active-high GPIO21 backlight plus GPIO27 auxiliary enable,
  both explicitly confirmed safe by the owner and checked HIGH during tests.
- **Observed touch path:** XPT2046 on shared HSPI pins 13/12/14, CS33, IRQ36.

These are the tested settings for this board, not a universal HW-950/CYD
pinout. Raw display-ID reads returned zero, so the guide does not claim a
detected IC identity. The initially supplied ILI9341 / separate-touch-bus
settings remain as a reference in `cyd-ili9341-rgb.json`. The generic
external ILI9488 profile is not the integrated owner's board configuration.

### Why the original profiles failed

1. They replaced Arduino's **`build.extra_flags`** with TFT-only defines.
   This removed `ESP32` and other platform/USB/core settings. Libraries
   selected incorrect platform paths; Adafruit BusIO also failed with a
   missing `BitOrder` type. Profiles now use **`compiler.cpp.extra_flags`**,
   preserving the core's flags for both sketch and library compilation.
2. The generic external-panel pins and missing backlight configuration did
   not match the integrated board.
3. A 480x320 canvas could produce full color fills but wrapped a line across
   the actual 320x240 display and put calibration targets off screen. A
   front photo and numbered bounds markers resolved the physical size.
4. Text was vertically mirrored; profile-controlled MADCTL adjustment fixes
   it without changing the installed library and persists across rotations.

`mcu_panel.h` shares initialization between display and touch-only sketches.
It diagnoses a missing ESP32 platform define instead of silently rendering
through the wrong platform path.

## Direct Vyne commands

From the repository root, load the profile and flash in the same PowerShell
session (an unset `$screen` is not a valid `key=value` build property):

```powershell
$profile = Get-Content "vyne/runtime/esp32/profiles/hw950-screen-rgb.json" -Raw | ConvertFrom-Json
$screen = $profile.build_properties[0]

.\vynec.exe --esp32 esp32 --esp32-build-property "$screen" --esp32-flash --esp32-port COM5 examples/test/esp32_wifi_radar.vy
```

Firmware starts automatically after upload. Add `--esp32-monitor` to capture
serial output. RST starts another scan; the radar scans once per boot.

`--esp32-build-property` accepts separate and equals forms and can be repeated.
It retains Arduino CLI's property-override semantics. To add C++ defines
directly, append them to the same property string:

```powershell
$smallerArena = "$screen -DVYNE_MCU_ARENA_SIZE=32768"
.\vynec.exe --esp32 esp32 --esp32-build-property "$smallerArena" examples/test/esp32_hello.vy
```

Use C++ extra flags for library configuration. Do not replace the platform's
`build.extra_flags` with TFT-only flags: that was a reproduced failure here.

## Screen demo

```powershell
python scripts/test_esp32_screen.py --port COM5
python scripts/test_esp32_screen.py --pattern text --seconds 15
python scripts/test_esp32_screen.py --pattern bounds --seconds 15
```

The launcher defaults to the working HW-950 profile and cycles through
eight stages: red, green, blue, white, black, grid, text, and bounds. It logs
configuration, actual backlight levels, optional raw ID readback, and drawing
markers. It asks for physical observations in an interactive terminal;
`--non-interactive` leaves visual confirmation unknown. Serial completion
does not by itself validate visible pixels.

`--compile-only`, `--profile`, `--driver`, `--spi-frequency`, confirmed
`--backlight-pin` / `--backlight-active`, and confirmed `--enable-pin` support
diagnosis on other panels. Logs default to
`<system temp>/opencode/vyne-esp32-screen`. See the
[demo guide](../../../scripts/esp32_screen_demo/README.md).

## XPT2046 and calibration

`VYNE_TOUCH_XPT2046` selects the runtime's 12-bit ADC/SPI implementation.
Configure `VYNE_TOUCH_CS`, `VYNE_TOUCH_SCLK`, `VYNE_TOUCH_MOSI`,
`VYNE_TOUCH_MISO`, optional `VYNE_TOUCH_IRQ`, and `VYNE_TOUCH_SPI_PORT`.
The default custom port is VSPI; the owner's working profile selects HSPI.
Thus both separate-bus CYD variants and a shared bus can be represented
without assuming that TFT_eSPI's `TOUCH_CS` uses different data/clock pins.
The custom driver requires no additional Arduino library.

The driver discards the first conversion after switching channels, uses
median samples, powers down to re-enable PENIRQ, and rejects disconnected
all-zero/all-high responses. Raw pressure is derived from two 12-bit ADC
channels; it is not a calibrated physical force measurement.

```powershell
python scripts/test_esp32.py --only touch_calibrate --run touch_calibrate --seconds 100 --profile vyne/runtime/esp32/profiles/hw950-screen-rgb.json
python scripts/test_esp32.py --only touch --run touch --seconds 15 --profile vyne/runtime/esp32/profiles/hw950-screen-rgb.json
```

Hold each magenta crosshair briefly, release, and follow the next visible
target: TL, TR, BR, BL. Eight consecutive stable samples are collected at
each corner. Each contact wait is bounded to 30 seconds and release to 15
seconds. Wrong/mixed targets, degenerate spans, and inconsistent opposite
edges are rejected; the example checks the returned Bool before reporting
NVS success. A larger overall serial window may be needed for slow input.

Custom calibration uses a versioned `vyne_touch/xpt_cal` blob containing
dimensions, reference rotation, mirror flags, axis swap, and signed endpoints.
Loads reject incorrect lengths/versions/geometry and a different current
canvas. Mapping handles rotated/mirrored frames and clamps to pixel bounds.
Until calibrated, contact coordinates are explicitly reported as approximate.
Existing shared-bus TFT_eSPI calibration remains under `vyne_touch/cal`.
Missing first-boot calibration is not an NVS error.

The touch example draws green dots and reports coordinates/pressure at a
bounded rate. The owner confirmed the dots followed the finger correctly
after four-corner calibration and a subsequent flash/reset.

### Raw diagnostic / optional reference library

```powershell
python scripts/test_esp32.py --only touch_probe --run touch_probe --seconds 18 --profile vyne/runtime/esp32/profiles/hw950-screen-rgb.json
```

Hold the panel during the 15-second probe. Fixed zero coordinates and
`z=4095` do not establish contact, even if a library says `contact=1`.
The runner reports those as skipped/unverified rather than a false pass.

An optional comparison uses XPT2046_Touchscreen 1.4.0:

```powershell
arduino-cli lib install XPT2046_Touchscreen
python scripts/test_esp32.py --only touch_probe --run touch_probe --seconds 18 --profile vyne/runtime/esp32/profiles/hw950-screen-rgb.json --build-property "compiler.cpp.extra_flags=-DVYNE_TOUCH_REFERENCE_XPT2046"
```

The runner appends extra C/C++ compiler flags to profile flags, so this
comparison does not discard display/pin configuration. Other property
values retain last-value-wins semantics.

## Suite and evidence

```powershell
# Build all 21 examples and 2 native fixtures
python scripts/test_esp32.py --profile vyne/runtime/esp32/profiles/hw950-screen-rgb.json

# Core suite plus native runtime regression on COM5
python scripts/test_esp32.py --run --port COM5 --profile vyne/runtime/esp32/profiles/hw950-screen-rgb.json

python -m unittest discover -s scripts/tests -p "test_esp32_screen.py" -v
```

The pure C++ check at `scripts/tests/esp32_touch_calibration.cpp` covers
MADCTL rotation/mirror/RGB bits, calibration fit, signed/inverted axes,
rotation/mirror mapping, clamping, canvas compatibility, invalid versions,
missed targets, degenerate spans, and oversized inputs. Compile it with a
host C++17 compiler; its executable belongs in a temporary/build directory.

Hardware runs capture setup output after resetting the port, inject `V`
for serial echo, verify signed Int64 extrema and timer wake, and report
missing completion/panic/failure diagnostics. Build/upload/serial logs plus
`run.json` and `results.json` are stored outside the source tree. Use
`--only` for smaller batches and `--build-timeout` for slow cold toolchains.

### Final COM5 validation

All **23 sketches compile** (21 Vyne examples and two native fixtures).
The final non-interactive matrix was completed in two batches after an
interruption: **16 runtime passes, 4 prerequisite skips** across 20 selected
board runs. Calibration and interactive touch were separately verified.

| Area | Result / evidence |
| --- | --- |
| hello, smoke, bits, math | PASS; expected values and markers |
| analog | PASS; zero input read in range, not a calibrated-source test |
| arena | PASS; both region drifts 0 B, 64 KB arena |
| NVS | PASS; counter 42, scale 2.50, erased default -1 |
| contracts | PASS; positive paths/invariant sum 45, no negative-contract run |
| realtime | PASS; worst 4.92 ms, zero overruns |
| serial | PASS; injected V echoed as rx: 86 |
| power | PASS; initial cause 0, timer wake cause 4 |
| I2C scan | PASS; scan completed with zero devices |
| display / radar / WiFi scan | PASS; 320x240 display and 120x80 sprite, scan returned networks |
| native runtime fixture | PASS; Int64/mixed math, RGB/rotation/enables, 32 sprite lifecycles, full-buffer pixel checks, calibration NVS round trip and malformed-version/length rejection |
| screen diagnostic | Owner confirmed colors, orientation, coverage, and all four markers |
| four-corner calibration | PASS; final samples (681,3424), (521,482), (3226,553), (3203,3425) |
| interactive touch | PASS; calibration reloaded, 226 contacts in bounds, owner confirmed dot alignment |
| IMU / external interrupts | SKIP; no sensor / no external edges |
| WiFi connect / HTTP | SKIP; network credentials unset |
| raw touch probe | Compiled; early invalid responses correctly classified as unverified; positive contact verified through calibration/touch |

Also passed: 16 Python checks, the host C++ geometry/orientation suite,
`xmake -y`, and `git diff --check`. Earlier timed-out/missed-target calibration
attempts are diagnostic failures, not successful calibration evidence.

## WiFi/HTTP provisioning

Scanning/radar need no credentials. Connect/HTTP read the `vyne_wifi` NVS
namespace: Int64 `configured=1` and strings `ssid`, `pass`, and `url`.
Provision those locally with the existing NVS APIs. The URL must be a
reachable JSON POST endpoint returning HTTP 200.

```powershell
python scripts/test_esp32.py --only wifi_connect wifi_http --run wifi_connect wifi_http --seconds 45 --profile vyne/runtime/esp32/profiles/hw950-screen-rgb.json
```

Connection can take 20 seconds; allow time for HTTP afterward. These
credentialed positive paths remain untested. Keep credentials and raw
network logs outside version control.

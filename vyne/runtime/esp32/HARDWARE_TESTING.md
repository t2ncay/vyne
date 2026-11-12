# ESP32 hardware testing and HW-350 setup

## Requirements

- Build Vyne with `xmake -y`.
- Install `arduino-cli` and `esp32:esp32` (validated with CLI 1.5.1 and core 3.3.12).
- Install TFT_eSPI for display/touch/fixture builds (validated with 2.5.43).
- Python 3; install `pyserial` with `python -m pip install pyserial` for flashing/serial runs.
- Close other serial monitors before using COM5.

The examples target the classic ESP32. Other supported aliases are
`esp32s2`, `esp32s3`, `esp32c3`, and `esp32c6`; their hardware-dependent
surfaces and pin assignments differ and were not board-tested here.

## HW-350 ILI9488 SPI display

The COM5 board uses a 3.5-inch HW-350 display. Selecting ILI9341 caused a
blank/white screen; selecting ILI9488 produced a 480×320 landscape display,
and the owner confirmed the WiFi list rendered correctly.

The supplied profile encodes the wiring used for this session, not a
universal pinout for every board carrying the HW-350 label:

| Signal | ESP32 GPIO |
| --- | ---: |
| Display SCK | 18 |
| Display SDI / MOSI | 23 |
| Display CS | 15 |
| Display DC | 2 |
| Display RESET | 4 |
| Touch T_CLK | 18 |
| Touch T_DIN | 23 |
| Touch T_DO / MISO | 19 |
| Touch T_CS | 21 |

The profile sets ILI9488, a 20 MHz display SPI clock, a 2.5 MHz touch clock,
and the built-in GLCD/font 2/font 4/GFX font support. Adjust the flags to
match your actual connections and controller.

ILI9488 display SDO does not tristate when CS is inactive. Leave **display
SDO disconnected** when sharing MISO with touch; touch T_DO still connects
to GPIO19. The profile does not define a backlight-control pin: power the
panel backlight according to its hardware, or add the appropriate `TFT_BL`
and polarity configuration for your board.

### Build and flash with the profile

From the repository root:

```powershell
python scripts/test_esp32.py --only display wifi_radar --run display wifi_radar --port COM5 --profile vyne/runtime/esp32/profiles/hw350-ili9488-spi.json
```

For direct CLI use, read the same build property and pass it to Vyne:

```powershell
$profile = Get-Content vyne/runtime/esp32/profiles/hw350-ili9488-spi.json -Raw | ConvertFrom-Json
$tft = $profile.build_properties[0]
.\vynec.exe --esp32 esp32 --esp32-build-property $tft --esp32-flash --esp32-port COM5 examples/test/esp32_wifi_radar.vy
```

`--esp32-build-property` accepts both `--flag key=value` and
`--flag=key=value`. Repeat it for additional Arduino build properties:

```powershell
.\vynec.exe --esp32 esp32 --esp32-build-property $tft --esp32-build-property "compiler.cpp.extra_flags=-DVYNE_MCU_ARENA_SIZE=32768" examples/test/esp32_hello.vy
```

These flags apply to the generated sketch and library compilation. A
driver define in the `.ino` alone cannot reconfigure separately compiled
TFT_eSPI sources. The library's global `User_Setup.h` stays available for
builds that do not supply overrides.

## Repeatable suite

Compile all 21 Vyne examples and both native fixtures without flashing:

```powershell
python scripts/test_esp32.py --profile vyne/runtime/esp32/profiles/hw350-ili9488-spi.json
```

Compile everything and flash the eight core examples plus the native
runtime regression:

```powershell
python scripts/test_esp32.py --run --port COM5 --profile vyne/runtime/esp32/profiles/hw350-ili9488-spi.json
```

Run the session's non-interactive matrix (calibration is built but not
flashed by this command):

```powershell
python scripts/test_esp32.py --run analog arena bits contracts display hello i2c_scan imu interrupts math nvs power realtime serial smoke touch wifi_connect wifi_http wifi_radar wifi_scan runtime touch_probe --seconds 18 --profile vyne/runtime/esp32/profiles/hw350-ili9488-spi.json
```

Use `--only` to limit compilation, `--compiler` to choose the executable,
`--board` / `--port` for another target, and `--output` for a log directory.
The default output is `<system temp>/opencode/vyne-esp32-tests`.
`--build-timeout` defaults to 300 seconds per sketch; slow cold toolchains
can use a larger value. Timed-out compiler process trees are terminated.
Serial windows default to 12 seconds; power/longer interactive cases need
a larger `--seconds` value.

Each build/upload has its own log. Each flashed example has a serial log;
`run.json` records the target, compiler, properties, and selection, and
`results.json` records build/upload/runtime results and reasons. The
runner resets after opening the port to capture `setup()` output, injects
`V` into the serial echo test, checks Int64 extrema in the runtime fixture,
and verifies timer wake in the power test. A panic/FAIL or missing completion
marker fails the run even if an example printed a success line earlier.
Unconfigured credentials, absent IMU, missing interrupt input, and
no-contact touch cases are skips. Skips are not positive peripheral validation.

## Touch calibration and diagnosis

Calibrate a wired XPT2046-compatible resistive controller:

```powershell
python scripts/test_esp32.py --only touch_calibrate --run touch_calibrate --seconds 90 --profile vyne/runtime/esp32/profiles/hw350-ili9488-spi.json
python scripts/test_esp32.py --only touch --run touch --seconds 18 --profile vyne/runtime/esp32/profiles/hw350-ili9488-spi.json
```

Tap each crosshair until it advances. Successful calibration prints
`touch calibrate: DONE` and stores the tuple under `vyne_touch/cal` in NVS.
The runtime accepts only the correctly sized calibration blob. A missing
namespace/blob is a normal first-boot condition and no longer produces the
Preferences `nvs_open failed: NOT_FOUND` error message during touch init.

If calibration does not advance, run the direct TFT_eSPI control sketch:

```powershell
python scripts/test_esp32.py --only touch_probe --run touch_probe --seconds 18 --profile vyne/runtime/esp32/profiles/hw350-ili9488-spi.json
```

Hold the panel while it reports raw X/Y/Z values for 15 seconds. This
sketch bypasses Vyne lowering, the touch wrapper, and calibration. In this
session all 30 samples were `x=8191 y=8191 z=0`, consistent with an all-high
SPI response rather than a working touch controller. Check controller type,
T_CS/T_DO wiring, common ground, power, and shared-MISO contention. Capacitive
controllers require their own driver; defining `TOUCH_CS` does not add one.
Touch contact and stored-calibration reload remain unverified on this board.

## WiFi and HTTP

Scanning requires no credentials. Connect/HTTP examples read the
`vyne_wifi` NVS namespace: `configured` (Int64), `ssid`, `pass`, and `url`
(strings). Provision those keys with a local sketch using the existing
`nvs_open`, `nvs_set_int`, and `nvs_set_string` APIs, then close NVS.
Set `configured=1` after writing valid credentials. `url` should be a
reachable JSON POST endpoint that returns HTTP 200.

```powershell
python scripts/test_esp32.py --only wifi_connect wifi_http --run wifi_connect wifi_http --seconds 45 --profile vyne/runtime/esp32/profiles/hw350-ili9488-spi.json
```

The examples allow 20 seconds to connect; the longer serial window also
allows for HTTP. WiFi/HTTP positive paths were not tested because credentials
were not provisioned. Keep locally provisioned credentials and raw network
logs outside version control.

## COM5 validation results

All **21 Vyne examples plus 2 native fixtures compiled** with the HW-350
profile. The non-interactive board matrix flashed 22 sketches: **16 passed**
their runtime checks and **6 were skipped** for missing prerequisites/input.
The long matrix was completed in batches after outer execution timeouts;
per-test logs and results were retained. Calibration was separately flashed
and attempted with 90-second windows but did not complete.

| Area | Result | Evidence / coverage |
| --- | --- | --- |
| hello, smoke | PASS | Completion markers; GPIO/math checks; no panic |
| analog | PASS | 9/12-bit reads in range; input read zero, not a calibrated voltage-source test |
| arena | PASS | Both region drifts = 0 B; 64 KB arena |
| bits, math | PASS | Expected bit/CRC values and zero printed math deltas |
| NVS | PASS | Counter 42; scale 2.50; erased default -1 |
| contracts | PASS | Example positive paths and invariant sum 45; no negative-contract run |
| realtime | PASS | Worst 4.92 ms, zero overruns |
| serial | PASS | Injected byte `V` echoed as `rx: 86` |
| power | PASS | Wake cause 0 followed by timer wake cause 4 |
| I2C scan | PASS | Completed scan, zero responding devices |
| display, WiFi radar | PASS | 480×320, sprite 120×80, no crash; owner confirmed visible rendering |
| WiFi scan | PASS | Nearby networks returned; final scan saw 10 |
| native runtime fixture | PASS | Mixed/integer math, UART0/stream Int64 extrema, 32 sprite lifecycles, full-buffer pixel checks, invalid dimensions/handles |
| IMU | SKIP | MPU6050 not detected; readings unverified |
| interrupts | SKIP | Zero edges; external interrupt signal unverified |
| touch, direct touch probe | SKIP | No contact; direct raw controller response all-high |
| WiFi connect, HTTP | SKIP | `vyne_wifi/configured` unset |
| touch calibration | INCOMPLETE | Compiles/flashes; no completion within 90 seconds |

Additional checks: `xmake -y`, CLI missing/invalid build-property rejection,
repeatable build-property compilation, runner failure/skip/echo/wake
classification checks, process-tree timeout cleanup, and `git diff --check`.

## Runtime defects reproduced and fixed

1. **Sprite text crash:** `esp32_display.vy` rebooted with LoadProhibited.
   Decoded addresses resolved to `TFT_eSPI::textWidth` / `drawString`, called
   by sprite text. TFT_eSPI 2.5.43 leaves heap-object `gfxFont` uninitialized;
   explicitly selecting built-in font 1 removes the invalid pointer read.
2. **Sprite clear / dimensions:** clear uses `fillSprite` for the selected
   buffer. Int64 dimensions are checked before conversion to signed 16-bit
   library parameters, so 65537 cannot silently become a one-pixel dimension.
3. **Boxed numeric math:** Int64 arguments were read from the double union
   member. Integer sqrt/cosine failed on the board. Shared numeric conversion
   now feeds the boxed transcendental, mapping, and interpolation helpers.
4. **Int64 serial truncation:** INT64_MAX printed `-1` and INT64_MIN printed
   `0` due to a 32-bit `long` cast. UART0 and stream printing now use the
   core's `long long` overload and preserve both signed extrema.
5. **First-boot touch NVS error:** optional calibration reads now tolerate a
   missing namespace/blob using NVS directly, without the Preferences error
   log. Calibration writes retain the existing Preferences format.
6. **Panel configuration:** per-build Arduino properties make the correct
   ILI9488 driver/pins available without editing the installed library.

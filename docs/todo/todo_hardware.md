# Hardware Configuration & Embedded UX

Status: implemented and verified on 2026-11-14, including fresh-project
HW-950 upload/reset/start on COM5 and installed-distribution acceptance.

## Goal

Users select their board once, write Vyne, and run:

```sh
vynec build
vynec run
```

No manual JSON loading, long compiler-property strings, Python launchers, or
fixture files should be required for ordinary application development.

## Current gaps

- `vynec init` lacks ESP32 board selection.
- Generated `hardware.toml` is currently informational.
- Hardware presets require manual CLI flags.
- Board, display, touch, and port configuration are spread across multiple places.
- Generated firmware artifacts appear beside source files.

## P0 — Functional hardware configuration

- [x] Add ESP32 family and board/preset choices to interactive `vynec init`.
- [x] Add matching non-interactive init options.
- [x] Generate board-appropriate `.vy` starter code instead of Uno-only code.
- [x] Implement a typed `hardware.toml` reader.
- [x] Load bundled JSON presets internally and resolve their settings into hardware configuration.
- [x] Keep **TOML user-facing** and **JSON presets compiler-managed**.
- [x] Connect hardware configuration to project target selection and build options.
- [x] Preserve existing Arduino projects and their `hardware.toml` fields.

Suggested minimal configuration:

```toml
[board]
preset = "hw950"

[connection]
port = "COM5"
baud = 115200
```

Display, touch, pins, dimensions, and orientation come from the selected preset.
Advanced users can supply explicit overrides.

## P0 — Short build/run workflow

- [x] `vynec build`: compile firmware without flashing.
- [x] `vynec run`: compile, upload, reset, and start the configured board.
- [x] Keep serial monitoring explicitly selectable.
- [x] Resolve configuration precedence: CLI overrides → hardware.toml overrides → preset defaults.
- [x] Preserve Arduino's platform flags when adding hardware defines.
- [x] Store generated sketches and firmware in the build directory.
- [x] Show concise build/upload progress and actionable failures.

## P1 — Presets and toolchain setup

- [x] Bundle hardware presets with Vyne distributions.
- [x] Give presets clear board/revision/display labels.
- [x] Include generic ESP32 and the measured HW-950 configuration.
- [x] Validate incompatible drivers, dimensions, pins, and target families.
- [x] Provide a simple one-time core/library setup workflow.
- [x] Report resolved hardware settings through a compiler command.

## Tests and acceptance

- [x] Test interactive and non-interactive initialization.
- [x] Test TOML parsing, preset resolution, overrides, and invalid configurations.
- [x] Verify generated hardware configuration actually affects compilation.
- [x] Verify platform defines remain intact.
- [x] Verify backward-compatible Uno and desktop projects.
- [x] Test fresh-project build/run on COM5.
- [x] Confirm normal projects require no development fixtures.
- [x] Update init documentation, templates, and embedded tutorials.

**Definition of done:** A freshly initialized ESP32 project builds and runs with
short Vyne commands using its generated `hardware.toml`.

## Verification evidence

- `xmake -y`: compiler build passed.
- `python scripts/test_hardware.py --compile --port COM5`: fresh desktop,
  Uno, generic ESP32 and HW-950 projects under `examples/templates/_generated/`;
  hardware precedence, firmware output, preserved platform USB defines and
  physical HW-950 startup passed.
- The same acceptance command passed with `VYNEC` pointing to the binary
  installed by `xmake install -y --installdir=build/install-hardware vynec`.
- `vynec hardware setup` passed against the installed ESP32 core/libraries.
- `pwsh -NoProfile -File scripts/test_init.ps1`: all existing template checks
  passed, including ML, scientific, super-resolution, desktop and AVR.
- `python -m unittest discover -s scripts/tests -p "test_*.py"`: 16 existing
  hardware helper regression tests passed.
- Host C++ touch calibration regression passed (geometry, rotation and
  invalid-data checks).
- Recording-toolchain acceptance verified compile-only build, run/upload,
  explicit monitor selection, CLI port/baud and additive legacy macros.
- Interactive tests drive the real questionnaire's keyboard event handlers
  for every ESP32 board/preset, back/confirm, empty selection and cancellation;
  the non-TTY guard is tested separately.

Test environment: Arduino CLI 1.5.1, ESP32 core 3.3.12, AVR core 1.8.8,
TFT_eSPI 2.5.43, Windows/MinGW compiler build. The physical test verifies
upload/reset and serial startup; panel appearance and touch calibration
remain hardware-specific visual checks rather than automated assertions.

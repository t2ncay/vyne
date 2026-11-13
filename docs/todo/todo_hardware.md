# Hardware Configuration & Embedded UX

Status: planned follow-up for the next development session.

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

- [ ] Add ESP32 family and board/preset choices to interactive `vynec init`.
- [ ] Add matching non-interactive init options.
- [ ] Generate board-appropriate `.vy` starter code instead of Uno-only code.
- [ ] Implement a typed `hardware.toml` reader.
- [ ] Load bundled JSON presets internally and resolve their settings into hardware configuration.
- [ ] Keep **TOML user-facing** and **JSON presets compiler-managed**.
- [ ] Connect hardware configuration to project target selection and build options.
- [ ] Preserve existing Arduino projects and their `hardware.toml` fields.

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

- [ ] `vynec build`: compile firmware without flashing.
- [ ] `vynec run`: compile, upload, reset, and start the configured board.
- [ ] Keep serial monitoring explicitly selectable.
- [ ] Resolve configuration precedence: CLI overrides → hardware.toml overrides → preset defaults.
- [ ] Preserve Arduino's platform flags when adding hardware defines.
- [ ] Store generated sketches and firmware in the build directory.
- [ ] Show concise build/upload progress and actionable failures.

## P1 — Presets and toolchain setup

- [ ] Bundle hardware presets with Vyne distributions.
- [ ] Give presets clear board/revision/display labels.
- [ ] Include generic ESP32 and the measured HW-950 configuration.
- [ ] Validate incompatible drivers, dimensions, pins, and target families.
- [ ] Provide a simple one-time core/library setup workflow.
- [ ] Report resolved hardware settings through a compiler command.

## Tests and acceptance

- [ ] Test interactive and non-interactive initialization.
- [ ] Test TOML parsing, preset resolution, overrides, and invalid configurations.
- [ ] Verify generated hardware configuration actually affects compilation.
- [ ] Verify platform defines remain intact.
- [ ] Verify backward-compatible Uno and desktop projects.
- [ ] Test fresh-project build/run on COM5.
- [ ] Confirm normal projects require no development fixtures.
- [ ] Update init documentation, templates, and embedded tutorials.

**Definition of done:** A freshly initialized ESP32 project builds and runs with
short Vyne commands using its generated `hardware.toml`.

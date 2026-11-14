# Universal vplot — colorful charts and live updates

## Release scope

- [x] Shared portable C plotting geometry and surface contract.
- [x] Copied and borrowed numeric data, generation-checked handles and explicit ownership.
- [x] Line, scatter, signed bar, histogram and heatmap charts.
- [x] Themes, palettes, shared fonts, responsive axes/ticks/labels/legends/colorbars and viewports.
- [x] Fixed ranges, visible-window autoscale, hysteresis and finite-data/gap policies.
- [x] Fixed-capacity rings, allocation-free steady-state rendering and spike-preserving reduction.
- [x] RGB888 and borrowed vimage surfaces; explicit PNG/image conversion.
- [x] ESP32 direct/bounded band rendering using configured logical display geometry.
- [x] Comprehensive plotting tests and live demo under examples/projects/plotter-esp32.
- [x] Preserve the original touch demo and existing text plot helper.
- [x] Compiler native mapping, arity/target errors, native aliases, staging and installed data lookup.

Verification results and reproduction commands are maintained in the test project
README and test coverage catalog. SVG/window/terminal adapters, logarithmic
scales and richer font shaping are future surface/scale/font work.

## Acceptance evidence

- Windows source and installed compiler tests passed, including all five decoded
  PNG chart types, native aliases, legacy text helper and library initialization.
- Ubuntu/WSL ASan/UBSan native tests passed; C++17 core compatibility passed.
- 147523 core checks and 128 quantized pixel-parity cases passed.
- Physical device regression passed direct/buffered surfaces, rotations,
  viewports, every chart kind, capability errors and resource cleanup.
- Final 120-second COM5 capture: 751 frames, 512 retained samples, stable 33712
  native bytes / 8 allocation calls / 0 Vyne arena bytes, free heap 225828..225916.
- Owner confirmed the live display visually.

## Beginner UX follow-up

- [x] One-call quick display and PNG helpers for all five chart types.
- [x] Managed chart defaults, automatic colors/legends, show/save and owned-surface cleanup.
- [x] Configured display initialization that preserves existing application rotation.
- [x] Automatic 16/8/4 KiB/direct buffer fallback without user budgets.
- [x] Managed live rings, automatic timestamps, clock wrapping and redraw throttling.
- [x] Short user-facing live demo and retained explicit-surface advanced demo.
- [x] Plot-library selection generates a short HW-950 chart starter.
- [x] New managed clock/ownership/failure/fallback/export and beginner integration tests.

Follow-up acceptance: host/source and installed suites passed; Ubuntu sanitizer
tests passed; fresh generated starter uploaded without setup code and was
owner-confirmed on COM5. All quick display helpers, explicit-rotation retention,
cleanup and live throttling passed. The managed 60-second capture observed 385
frames, stable 33760 bytes / 8 allocations / 0 arena bytes / 225892 free heap.

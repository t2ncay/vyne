# Fresh-project hardware acceptance examples

Generate, inspect and test real projects here:

```powershell
vynec init examples/templates/_generated/hw950 --name hw950_demo --kind embedded --preset hw950 --port COM5 --no-git
cd examples/templates/_generated/hw950
vynec hardware show
vynec hardware setup
vynec build
vynec run
vynec run --monitor
```

The automated acceptance runner generates independent desktop, legacy Uno,
generic ESP32 and HW-950 projects under `_generated/`, validates configuration
and CLI precedence, and can build the starters using the real installed cores.

```powershell
python scripts/test_hardware.py
python scripts/test_hardware.py --compile
python scripts/test_hardware.py --compile --port COM5
```

The last command uploads the freshly generated HW-950 starter and checks its
geometry, active touch configuration and persistent heartbeats after reset,
then verifies configured-default versus explicit 0/1/2/3 rotation precedence.
It requires the matching physical board.
Generated projects and firmware are ignored by Git; compiler-managed JSON
presets are loaded from the Vyne installation, never from these examples.

For an operator-assisted display/touch check, upload the generated starter and
run from the repository root:

```powershell
python scripts/test_hw950_init.py --port COM5 --seconds 90 --require-touch
```

Touch corners and center; confirm upright corner labels, RGB bars and aligned
green dots. Add `--calibrate` to explicitly request four-corner calibration.
`--background --output <existing-directory>` detaches capture for interactive
checks and saves serial/results logs. Calibration is never erased automatically.

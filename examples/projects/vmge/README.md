# VMGE — Vyne Mini Game Engine

A compiled desktop map editor and playable-map runtime built on managed
`vraylib`. VMGE restores the original `map_builder.vy` → `tau_map.dat` →
`map_load_test.vy` workflow and extracts reusable map, rendering, collision,
asset, input and OBJ-export utilities into one project.

The project works with the existing compiler. The small library additions are
ordinary facade methods and one state query in the existing managed runtime;
they require no compiler/codegen changes or new native function registrations.

## Quick start

Install Vyne and the desktop raylib toolchain: GCC, CMake, make and the platform
graphics development dependencies described in the repository installation
guide. A working OpenGL context is required. Raylib is statically linked; the
first compilation builds its cached native libraries.

From the **repository root**, launch the editor:

```shell
vynec --run examples/projects/vmge/src/main.vy
```

The project subcommands discover `project.toml` from the working directory.
Inside **`examples/projects/vmge/`**, with `vynec` on PATH:

```shell
vynec build
vynec run
```

From the repository root, play the portable scene in PowerShell:

```powershell
$env:VYNE_VMGE_MODE = "play"
$env:VYNE_VMGE_ASSETS = "procedural"
$env:VYNE_VMGE_MAP = "examples/projects/vmge/maps/demo.dat"
vynec --run examples/projects/vmge/src/main.vy
```

POSIX equivalent:

```shell
VYNE_VMGE_MODE=play VYNE_VMGE_ASSETS=procedural \
  VYNE_VMGE_MAP=examples/projects/vmge/maps/demo.dat \
  vynec --run examples/projects/vmge/src/main.vy
```

Unset `VYNE_VMGE_MODE` or set it to `editor` to edit. Use a new writable map path
to start an empty map, then press **J** to save. Launch play mode with the same
path to explore it. Player spawn/reset is `(0, 1.8, 0)`, facing positive Z.
Leave that cell clear when designing a playable map.

The historical commands remain supported through thin launchers:

```shell
vynec --run examples/graphics/map_builder.vy
vynec --run examples/graphics/map_load_test.vy
```

## Controls

| Input | Editor | Play mode |
| --- | --- | --- |
| WASD / mouse | Fly horizontally / look | Walk / look |
| Shift | Ascend | Sprint |
| Ctrl | Descend | Crouch; standing is blocked under a low ceiling |
| Space | Place selected cube | Grounded jump |
| 1–8 | Select texture slot | — |
| Left / right click | Place / remove at the cursor, while mouse is captured | — |
| Backspace | Remove all cubes centered at the cursor | — |
| J | Save map | Reset player position and physics |
| K | Export OBJ geometry | — |
| Tab | Capture / release mouse | Capture / release mouse |
| F11 / Escape | Toggle fullscreen / exit | Toggle fullscreen / exit |

Placement is edge-triggered and occupied cells are rejected. Holding a mouse
button no longer adds one duplicate cube per frame. Capacity and write failures
are reported in the HUD; a failed write leaves the editing session available.
Exiting does not automatically save.

## Maps and texture compatibility

The historical format is retained:

```text
x,y,z,size,slot;
-2,1,4,2,0;
```

Each cube uses its **center**, one uniform side length and a zero-based texture
slot. Blank records and surrounding whitespace are allowed. Decimal/scientific
numeric syntax is accepted. Coordinates must be finite with absolute value at
most 1,000,000; size must be positive and at most 1,000,000; slots must be
integers from 0 to 7. Bad records produce a numbered diagnostic.

Imported records retain order, duplicates and numeric spellings, including
high-precision decimals. Saving normalizes record whitespace, not those numeric
values. Newly placed cubes use the editor's two-unit grid. The vertical grid
phase follows the first imported cube, allowing selection of both the original
integer-height maps and the demo's half-height centers. Empty maps start with
centers at `Y = 1 + 2n` so their first layer rests on the ground plane.

One shared palette follows the **original builder's** slot meanings:

| Key / stored slot | Original texture | Procedural substitute |
| --- | --- | --- |
| 1 / 0 | `Brick/Brick_16-512x512.png` | Maroon checker |
| 2 / 1 | `wall.jpeg` | Light-gray checker |
| 3 / 2 | `building.jpg` | Beige checker |
| 4 / 3 | `yusif.jpeg` | Purple checker |
| 5 / 4 | `asphalt_road_3.jpg` | Dark-gray checker |
| 6 / 5 | `Metal/Metal_18-512x512.png` | Blue checker |
| 7 / 6 | Same metal texture | Green checker |
| 8 / 7 | Same metal texture | Orange checker |

Play mode loads `Dirt/Dirt_20-512x512.png` separately for the ground when using
original media, resolving the historical disagreement about slot 2. Repeated
metal loads share a cached texture; each slot has a reusable cube model.

The committed `maps/demo.dat` exercises all eight slots. Original media under
`examples/assets/` and local maps remain ignored. `auto` uses the original media
when its directory is discovered, otherwise procedural textures. An explicitly
selected asset directory is checked during loading; failures are reported rather
than silently substituted. Optional original ambient audio is `akira.wav`.

**OBJ export** writes eight world-space vertices and six outward-facing quads
per cube, with groups containing cube index and slot. It exports geometry only,
without UVs, MTL files or copied textures. The default output is
`exported_map.obj` in the process working directory.

## Configuration

| Environment variable | Default | Purpose |
| --- | --- | --- |
| `VYNE_VMGE_MODE` | `editor` | `editor` or `play`; launchers select their own mode |
| `VYNE_VMGE_MAP` | `tau_map.dat` | Input/editor-save path, relative to process cwd or absolute |
| `VYNE_VMGE_EXPORT` | `exported_map.obj` | Editor OBJ output path |
| `VYNE_VMGE_ROOT` | Discovered | Project directory containing shaders and `project.toml` |
| `VYNE_VMGE_ASSETS` | `auto` | `auto`, `procedural`, or original media directory |
| `VYNE_VMGE_WIDTH`, `VYNE_VMGE_HEIGHT` | `1280`, `720` | Initial window dimensions |
| `VYNE_VMGE_WINDOWED` | `1` | Set to `0` for fullscreen |
| `VYNE_VMGE_FRAMES` | `0` | Unlimited interactive mode, or bounded frame count |
| `VYNE_VMGE_CAPACITY` | `16384` | Maximum records, from `1` through `65536` |
| `VYNE_VMGE_AUDIO` | `1` | Set to `0` to skip original ambient audio |
| `VYNE_VMGE_HIDDEN` | `0` | Hidden window and initially released cursor for acceptance |
| `VYNE_VMGE_AUTOTEST` | `0` | Script editor edits/save/export or viewer movement for acceptance |

Without an explicit map setting, the editor starts empty if `tau_map.dat` is
missing, and play mode falls back to the committed demo. An explicitly requested
missing viewer map is an error. Parent output directories must already exist.
The acceptance runner uses disposable outputs under repository `build/`.

Project discovery works from the repository/project directories and executable
locations under their build directories. For another executable layout, set
`VYNE_VMGE_ROOT` to an absolute project path. Explicit asset/map paths still
follow the process working directory; use absolute paths when relocating.

## Architecture and lifetimes

```text
src/main.vy                 editor/play selection and error reporting
src/engine/assets.vy       paths, options, original/procedural palette
src/engine/map.vy          fixed-capacity map store, validation and persistence
src/engine/render.vy       reusable models, map drawing, ray/grid cursor
src/engine/collision.vy    sliding, support, crouch, jump, ceiling and landing
src/engine/input.vy        shared window and keyboard/cursor controls
src/engine/export.vy       geometry-only OBJ output
src/editor/map_builder.vy  editing and HUD
src/game/map_load_test.vy  player and fog → VHS → scanline rendering
```

One `vraylib.App` owns resources, context, render passes, streamed music and
frame memory. Both applications close it in `finally`, including partial asset
loads and frame failures. Textures/models/shaders/targets/player state are
created before the frame loop. `screen_target()` follows window resizing;
`present()` fixes render-texture orientation. Shader uniforms are named and
cached: fog `cameraPos`, VHS `time`, scanline `renderSize`.

`VMGEMap` preallocates numeric storage, imported spellings and a shared count
cell **before managed frames begin**. `append`, `place` and `remove` reuse those
slots instead of allocating escaping rows or committing replacement arrays.
Only primitive numbers and static empty-string literals are assigned by edits.
HUD status is a numeric code; per-frame messages, vectors and serialization
builders are transient. Imported spellings are populated at load time and are
not replaced with frame-local strings.

Reusable map API: `vmge_map_create`, `vmge_map_parse`, `vmge_map_load`,
`vmge_map_save`, `vmge_map_export`, plus `VMGEMap.count()`, `value(index,field)`,
`append`, `place`, `occupied` and `remove`. Allocate/load before the loop and use
these methods rather than editing backing arrays. `append` preserves duplicates;
`place` rejects an occupied center. Direct programmatic non-grid Float64 appends
use Vyne's standard number-to-string formatting; imported values retain their
original precision.

Player motion uses clamped delta time, normalized diagonal input and axis-wise
collision substeps of at most 0.25 world units. Crouching preserves feet, standing
checks headroom, rising jumps stop at ceilings, descending motion lands on cube
tops, and leaving support starts falling. Jump impulse/gravity follow the migrated
bodycam example (`21.0` / `43.2` world units), allowing jumps onto two-unit cubes.
This is a cube-map runtime, with linear
map scans, rather than a general rigid-body physics system.

### Managed vraylib additions

- `App.texture(image)` uploads a checked same-session CPU image into an owned
  GPU texture. The source image can immediately close.
- `Camera.view()` returns a checked frame-local camera snapshot.
- `Camera.ray(screen_position)` returns a frame-local picking ray, requiring a
  live application window.
- `Texture.draw_region(source,destination,origin,rotation,tint)` draws scaled,
  cropped or rotated textures; it checks resource lifetime and drawing-surface
  state. Borrowed target views preserve logical top-left orientation and follow
  target resizing.
- `App.drawing()` reports whether a managed drawing surface is active, using
  the existing runtime `managed_state` entry point.

## Verification

From any directory, invoke the runner by its path:

```shell
# CPU map/physics/frame-lifetime checks, compile all entry points and project.
python examples/projects/vmge/tests/run.py
# Portable hidden-window GPU readback and editor → save/export → viewer flow.
python examples/projects/vmge/tests/run.py --graphics
# Original textures/dirt/audio and unchanged camera_test regression.
python examples/projects/vmge/tests/run.py --graphics --assets
# Also render a local historical map in both launchers, without modifying it.
python examples/projects/vmge/tests/run.py --graphics --assets --map tau_map.dat
# Linux virtual display; a real OpenGL context is still required.
xvfb-run -a python3 examples/projects/vmge/tests/run.py --graphics
```

`VYNEC` selects the compiler executable. The runner isolates interactive VMGE
environment settings and writes its own fixtures/binaries under
`build/vmge-tests/`. Graphics checks use 8 editor frames and 12 viewer frames;
original camera regression uses 30 frames. `--assets` requires the local media
and a working audio device.

Acceptance covers strict parsing, imported decimal precision, file round trips,
duplicate/capacity behavior, OBJ bounds/indexing/winding, sliding and sprint
tunneling, crouch/headroom, grounded jumps, ceiling hits, landing/support, 100
frames of edit/serialization churn, procedural texture upload, closed-resource
errors, camera rays, scaled pixels, borrowed-target orientation/resizing,
fog/VHS/scanline pixels, uniform caching, project build/run, relocated execution,
partial-load and nested-pass failures, and zero live native handles.

Normal runs print `VMGE_EDITOR_COMPLETE` or `VMGE_VIEWER_COMPLETE` with frame
count and `live_handles=0`. Entry-point errors exit nonzero and print
`VMGE_ERROR live_handles=0` after application cleanup.

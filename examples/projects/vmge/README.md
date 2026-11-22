# VMGE — Vyne Mini Game Engine

A compiled desktop **3D scene and hybrid-world editor** built on Vyne and managed
`vraylib`: node hierarchy, inspector, gizmos, map brushes, seeded terrain,
project assets and isolated Play/Stop. The shared **Scene / World / Shaders /
Rendering** workspace adds code-first GLSL authoring, typed controls, graph
generation and editable multipass effects. Inter provides interface typography;
JetBrains Mono provides source, gutter and diagnostic typography.

**Environment and camera looks:** a searchable Add Node picker creates Sky,
Directional Light and Point Light nodes. Active player cameras accept custom
post-processing pipelines, including the bundled editable VHS + tonemap example.
See [Sky, lighting and camera effects](ENVIRONMENT.md) for editor steps, shader
inputs, gameplay APIs, region policies, budgets and acceptance commands.

The original `map_builder.vy` → `tau_map.dat` → `map_load_test.vy` workflow remains
available. The studio adds a scene-owned resource with **4,096 map cells and
64×64 terrain quads**, independent of its 128-node scene hierarchy.

Build the compiler from this branch before using the studio: it includes focused
exception-lifetime fixes, atomic VFS saves and managed graphics/font helpers.
No new raylib function registrations or regenerated bindings are required.

## Quick start

Install Vyne and the desktop raylib toolchain: GCC, CMake, make and the platform
graphics development dependencies described in the repository installation
guide. A working OpenGL context is required. Raylib is statically linked; the
first compilation builds its cached native libraries.

From the **repository root**, launch the editor:

```shell
xmake build vynec
vynec --run examples/projects/vmge/src/main.vy
```

This launches **scene mode** by default. The project subcommands discover
`project.toml` from the working directory.
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

Set `VYNE_VMGE_MODE=editor` for the historical cube editor. Use a new writable map path
to start an empty map, then press **J** to save. Launch play mode with the same
path to explore it. Player spawn/reset is `(0, 1.8, 0)`, facing positive Z.
Leave that cell clear when designing a playable map.

The historical commands remain supported through thin launchers:

```shell
vynec --run examples/graphics/map_builder.vy
vynec --run examples/graphics/map_load_test.vy
```

## Studio: build a world

From the repository root, open the bundled island in PowerShell:

```powershell
$env:VYNE_VMGE_MODE = "scene"
$env:VYNE_VMGE_ASSETS = "procedural"
$env:VYNE_VMGE_SCENE = "$PWD/examples/projects/vmge/scenes/world-island.vscene"
.\vynec.exe --run examples/projects/vmge/src/main.vy
```

Use **Save As** before changing the bundled example. For a new proposed Save path,
set `VYNE_VMGE_SCENE` to an absent `.vscene` file. An absent path starts the demo;
an existing malformed scene produces a diagnostic.

1. **Shape terrain:** choose Hills/Island/Plateau/Flat, open Settings, enter seed,
   size, spacing, amplitude, frequency, octaves and persistence. Preview, then
   Apply or Cancel. A spawn-pad radius creates a central flat area. Raise, Lower,
   Flatten and Smooth sculpt the applied heightfield.
2. **Build maps:** choose Paint, Erase, Rectangle or Eyedropper. Use Surface
   placement or an XZ/XY/YZ construction plane, grid and elevation. Drag a stroke;
   release records one undo step. Escape restores its before-image.
3. **Add assets:** browse/search the cached Assets panel. Import self-contained
   GLB/images into project-owned storage, then Assign to mesh or New mesh node.
   Images have bounded thumbnail previews. External dependency-based models can
   be referenced explicitly; copy their complete dependency tree for portability.
4. **Play:** F6/Play copies authored nodes and world data into separate runtime
   owners. Click the viewport to capture the mouse; Escape releases it. Stop
   returns to authoring without keeping runtime edits.
5. **Save:** Ctrl+S saves the current document; Ctrl+Shift+S chooses another path.
   New/Open/Exit and titlebar close offer Save/Discard/Cancel when dirty. Failed
   saves leave the document and pending action available. Scene writes atomically
   replace the destination after the sibling file has successfully closed.

| Studio input | Action |
|---|---|
| Right drag / middle drag / wheel | Orbit / pan / zoom |
| Perspective, Top, Front, Side / numpad 5, 7, 1, 3 | View presets |
| T/R/S toolbar, Snap, F | Transform mode, snapping, focus node |
| Ctrl+Z / Ctrl+Y | Undo / redo; includes world strokes and terrain |
| Ctrl+N / Ctrl+O / Ctrl+S / Ctrl+Shift+S | New / Open / Save / Save As |
| Ctrl+D / Delete | Duplicate / delete selected subtree |
| Ctrl+I | Import DAT into map resource, preserving nodes and terrain |
| Ctrl+E / Ctrl+Shift+E | Export resource map cells as OBJ / DAT |
| B / P / N | Cycle brush / construction plane / plane–surface placement |
| + / − / Page Up / Page Down | Grid spacing / construction elevation |
| F6 | Play / Stop |

Fields support UTF-8 caret/selection, clipboard shortcuts, Home/End and Tab.
Hierarchy branches collapse and search; the inspector groups type-specific
properties. See [studio/document workflows](STUDIO.md), [UI and asset storage](UI.md),
[world resources](WORLD.md) and [terrain settings/exports](WORLD_OPTIONS.md).

## Shader Studio: write, inspect, and apply

From the repository root:

```powershell
$env:VYNE_VMGE_MODE = "shader"
$env:VYNE_VMGE_ASSETS = "procedural"
.\vynec.exe --run examples/projects/vmge/src/main.vy
```

1. Edit Fragment/Vertex source, then **Compile / Ctrl+Enter**. Syntax/link errors
   retain the last successful program. Root diagnostics navigate source; include
   diagnostics show cached file context; graph diagnostics select the node.
2. Declare a used uniform, Compile, then **Parameters → Expose / Schema → Expose
   uniform**. Define its default/range/group/units and Apply schema. Drag Values or
   right-click for precise entry; scalar/vector/matrix controls validate the real
   reflected type. Numeric edits do not require recompiling source.
3. Choose plane/cube/sphere/selected-model previews; freeze time, step, change
   resolution, capture a reference and inspect linear float pixels/differences.
4. Select a mesh in Scene, return to Shaders and **Apply**. Each node retains its
   own material values even when geometry/programs are shared. Undo, duplication,
   scene reopen and Play preserve those overrides.
5. **Save / Save As / Reload** preserve source and the authoritative material
   checkpoint, including sampler paths. Assigned scene publications are immutable;
   opening one creates an editable copy. Close/Open support Save/Discard/Cancel.
6. In Rendering, **Load bloom gallery**, choose pass shaders and predecessor
   inputs, bypass, inspect, change target format/size and reorder safely. The input
   is the Studio material preview. Save includes a shader-path ledger for reopen.

The gallery includes lit procedural material, HDR plasma/grid, exposure/tonemap,
bright extraction, separable blur and bloom composition. The typed graph generates
deterministic GLSL through the same document/compiler path. See the
[complete Shader Studio guide](SHADER_STUDIO.md), [UI/editor contract](SHADER_UI.md),
[scene/material integration](SHADER_HOST.md), [resource/evaluation APIs](SHADER_MODEL.md)
and [starter gallery](shaders/studio/GALLERY.md).

Float captures and a frozen evaluation API provide a future research bridge.
Current fitting uses explicit finite differences and `voptim` SGD for small scalar
parameter sets; it is not arbitrary GLSL autodiff and is not a GUI optimizer.

## Historical map editor/player controls

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
| `VYNE_VMGE_MODE` | `scene` | Shared `scene`, `world`, `shader`, `rendering`; historical `editor`, `play` |
| `VYNE_VMGE_SHADER_DOCUMENT` | `<project>/shaders/studio.vshader` | Editable shader and paired material checkpoint |
| `VYNE_VMGE_SHADER_GRAPH` | `<project>/shaders/studio.vgraph` | Typed graph document |
| `VYNE_VMGE_SHADER_PIPELINE` | `<project>/shaders/studio.vpipeline` | Pipeline and host shader-path ledger |
| `VYNE_VMGE_LAYOUT` | `<project>/shaders/studio.layout` | Saved tabs, splits, DPI and maximization |
| `VYNE_VMGE_SCENE` | `<project>/scene.vscene` | Studio document/proposed Save path |
| `VYNE_VMGE_WORLD_DEMO` | `0` | `1` generates a 1,024-cell island demo when no scene is loaded |
| `VYNE_VMGE_SPAWN_PAD` | `0` | `1` opts into generated-pad Play spawn; otherwise uses authored camera |
| `VYNE_VMGE_MAP` | `tau_map.dat` | Input/editor-save path, relative to process cwd or absolute |
| `VYNE_VMGE_EXPORT` | `exported_map.obj` | Editor OBJ output path |
| `VYNE_VMGE_ROOT` | Discovered | Project directory containing shaders, fonts and `project.toml` |
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
src/editor/scene_editor.vy studio orchestration, input capture and acceptance
src/editor/document.vy     Save/Discard/Cancel and document identity
src/editor/workspace.vy    tree, grouped inspector and cached asset browser
src/editor/viewport.vy     gizmos, orbit and orthographic view presets
src/editor/world_tools.vy  brush strokes, preview staging and terrain tools
src/editor/world_settings.vy direct numeric settings and pickers
src/ui/                   Inter theme, UTF-8 editing and project asset storage
src/editor/shader_workspace.vy source/graph/parameter/preview UI
src/editor/workspace_host.vy shared navigation, material publication and Rendering
src/shader/               documents, schemas, materials, graph, pipeline, evaluation
src/scene/                handles, transforms, IO, history, rendering and Play
src/engine/world*.vy       resource storage, exact IO/export and motion queries
src/engine/terrain*.vy     height sampling, sculpting and deterministic fBm
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

- Sized managed fonts, Unicode-aware measurement and bilinear atlas filtering.
- Matrix model drawing/picking/bounds, viewport rays and managed panel clips.
- Opt-in transient string errors: copy diagnostics into owned log bytes promptly;
  the static error ring retains only the latest 64 promotions, including rethrows.
- Deferred close requests let the studio finish Save/Discard/Cancel; default App
  close behavior and explicit stop/frame limits remain unchanged.
- Uncached compiler candidates return bounded driver diagnostics; reflection and
  typed uploads validate active uniforms. Per-slot material draws preserve shared
  model state. RGBA32F targets, exact replacement copies, readback and release-bank
  preflight support failure-safe pipelines without generated-binding changes.

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
# Also poison reclaimed arenas during the complete studio/legacy acceptance.
python examples/projects/vmge/tests/run.py --graphics --poison
# Focused exception/cleanup/atomic-save/font/close regression suite.
python vyne/runtime/tests/run_vraylib_blockers.py --font examples/projects/vmge/assets/fonts/Inter-Regular.ttf
# Native shader compile/reflection/material/float/GL-state/fault acceptance.
python vyne/runtime/tests/shader_run.py
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

The maintained runner includes **16 CPU fixtures and 9 graphics fixtures**, plus
five shader-core GPU sections and both 32-frame shader-host flows. Shader checks
cover real compilation/reflection, independent material pixels, parameter exposure,
immutable publications, typed graph and pipeline persistence, fractional-alpha/HDR
copies, include diagnostics, fitting rollback/checkpoint resume, Save As/Reload,
gallery reference pixels and native handle cleanup. Test documents/layouts are
isolated from project-owned user files.

Acceptance additionally covers deterministic terrain, exact triangle height/pick
agreement, 50° walkable slopes, spawn-pad policy, maximum resource persistence,
Float64 roundtrips, generation-safe copy/reuse, atomic subtree duplication,
world before-image rollback, Inter metrics and Unicode edits, owned binary imports,
thumbnail eviction, orthographic views, document recovery and extended studio flow.

### Memory policy contract

Session owners and boxed buffer headers are created before managed frames. Map,
terrain and history data use fixed numeric buffers; names/paths use reusable UTF-8
storage. Pool buffers are explicitly initialized. Rings are preacquired banks:
there are no later allocations that silently overwrite retained slots. No frame
string, resized array backing or transient descriptor is published into an owner.

Imports and generation validate separate staging stores, then copy payloads into
existing owners. `@speculative` does not roll back mutations to old buffers, and
aggregate `commit()` is not assumed to repair nested ownership. `@bounded` is an
exit accounting check, not a peak/admission limit; explicit sizes and input limits
are enforced separately. Tests track main/commit arenas and native handles.
Exception recovery restores checkpoint/persistent/zero-mode depths and protects
mutable cleanup owners across `longjmp`; remaining policy limitations are recorded
in `vyne/runtime/tests/REGION_THROW_AUDIT.md`.

### Current scale

This is a compact-world editor. Terrain rendering/picking is bounded linear work:
up to 8,192 triangle draws plus 4,096 cube draws, without chunk meshes, LOD or
streaming. Resource cells use solid palette colors; node meshes retain textures.
OBJ/DAT export includes map-resource cells only. Asset import supports self-contained
GLB/images up to 8 MiB; dependency rewriting, animation authoring and full rigid-body
physics are not implemented. Shader Studio targets desktop OpenGL 3.3. The shared
scene/pass program bank has 8 slots, node instances retain 16 parameter/orphan rows
and 16 sampler references, and the native shader supports 4 managed sampler bindings.
Hosted owner storage is bounded to 128 MiB; explicit target/asset budgets are
separate. Full docking, world streaming, compute shaders and arbitrary shader AD
remain future work. See the linked guides for precise limits.

Normal runs print `VMGE_SCENE_EDITOR_COMPLETE`, `VMGE_EDITOR_COMPLETE` or `VMGE_VIEWER_COMPLETE` with frame
count and `live_handles=0`. Entry-point errors exit nonzero and print
`VMGE_ERROR live_handles=0` after application cleanup.

# Hybrid map and terrain worlds

VMGE's scene viewport can render and play an ordinary node hierarchy together
with a **single, scene-owned world resource**. Map cells and terrain vertices are
resource geometry, not nodes: 4,096 cells still leave all 128 node slots available.
The resource is exposed as `scene.world`; selecting its surface uses selection
handle `-2`, with the picked cell index kept in the world tool state. A selected
map cell has a yellow outline.

## Try it

Launch the normal scene editor with `VYNE_VMGE_MODE=scene` and either:

* `VYNE_VMGE_SCENE=<project>/scenes/world-island.vscene` for a saved island,
  eight palette cells and a playable character/camera; or
* `VYNE_VMGE_WORLD_DEMO=1` for a generated seed-42 island over **1,024 map
  cells**, alongside the normal five demo nodes. Use this with AUTOTEST disabled.

The ordinary demo and existing scene-editor AUTOTEST retain five nodes and
version-1 output. No new command-line mode is required.

## Editor tools

The viewport overlay cycles through Select, Paint, Erase, Rectangle,
Eyedropper, Raise, Lower, Flatten and Smooth. **B** also cycles tools. Brushes
only consume left clicks below the overlay; the existing orbit controls remain
available. Numeric overlay labels use static glyphs rather than building strings.

* **P** / the XZ, XY, YZ button changes the construction plane. XZ uses Y
  elevation, XY uses Z elevation, and YZ uses X elevation.
* **+ / -** doubles/halves grid spacing, clamped to 0.125–1,024 units.
  **Page Up / Page Down** changes construction elevation by one grid step.
* **N** / Plane–Surface toggles surface construction. Paint offsets a hit by
  its normal and half a grid cell, then snaps into an adjacent cell. Empty-space
  construction uses the selected plane. Erase and eyedropper address the actual
  hit cell, including unsnapped legacy cells.
* **Palette** cycles eight resource colors. Eyedropper copies a cell's size,
  palette and Y elevation; picking terrain captures the flatten target height.
* Drag Paint/Erase to edit a stroke. Map drags interpolate skipped grid cells;
  a segment is explicitly limited to 128 grid steps. Rectangle fills on release
  in the construction plane; hold left Shift to erase the rectangle instead.
  A yellow wire box previews its extent.
* Radius and Power cycle bounded sculpt settings. Sculpting has linear radial
  falloff. Smooth reads an immutable numeric snapshot of the entire previous dab,
  so traversal order cannot feed modified samples back into the same dab.
* Hills, Island, Plateau and Flat are deterministic presets. Seed, Size and
  Height configure generation; Preview generates into a separate resource.
  Apply records one undo step; Cancel or Escape discards it. Changing settings
  while previewing takes effect on the next Preview.
* Escape cancels an active stroke using its real before-image. Releasing a
  modified stroke records **one** undo step; no-op paint records none. Capacity
  or brush validation failures restore the before-image and produce an editor
  diagnostic. Undo and redo include both scene nodes and the world resource.

Toolbar history/file/Play shortcuts are withheld while a world stroke or terrain
preview is active. Apply or cancel first. World geometry is authored in world
coordinates, independent of node hierarchy transforms.

## Explicit limits and storage

| Resource | Limit |
|---|---:|
| Scene nodes | 128, independent of cells |
| Map cells | 4,096 |
| Terrain quads per side | 1–64 (0 means absent) |
| Terrain samples | at most 65 × 65 |
| Rendered terrain triangles | at most 8,192 |
| Rectangle | at most 128 cells per axis; capacity preflight before mutation |
| Terrain spacing | 0.125–1,024 |
| Terrain amplitude / local sculpt height | 0–1,024 / −1,024–1,024 |
| Seed | integer 0–1,000,000 |
| Scene source | 1 MiB; 2,048 characters per record |

Each world owner preallocates 24,736 Float64 data elements and 4,256 numeric
work elements (231,936 payload bytes), four mutable Vector3 records, and ten
color records. It never grows those arrays. Data offsets are: header 0–15,
map records 16–20,495 (five numbers per cell), terrain 20,496–24,720 (fixed
row stride 65), followed by padding. Header element 8 retains generation
amplitude. Work elements 0–4,224 hold sculpt/import scratch; 4,225–4,233 hold
ray results; 4,234 is the current tool palette.

Authored, runtime, import staging and brush/preview staging each own their data.
The existing 17-slot history ring additionally acquires seventeen numeric world
snapshots at startup (3,364,096 payload bytes). Edits copy numbers into those
owners; they retain no edit-created arrays, strings or boxes. Rendering mutates
the four preacquired vertex records and reuses colors. Scalar ray queries produce
no per-cell or per-triangle collision/vertex records. Native draw-call arguments
are transient within managed frames.

The 64 MiB import/save regions bound **allocation volume**, not a claimed peak
memory cap. The maximum-resource test exercises serialization and parsing of
4,096 cells plus all 4,225 terrain samples within that explicit bound. A failed
import may alter staging, but never copies it into authoring. No speculative
region is used to pretend that preexisting mutations can be rolled back.

## Rendering and collision

Terrain is drawn with native `draw_triangle_3d`, with vertices a=(0,0), b=(1,0),
c=(0,1), d=(1,1), split into a/c/b and b/c/d. Height sampling and scalar ray
intersection use those exact two planar triangles, including the outer boundary;
they do not use bilinear interpolation. Ray normals point upward. Character
landing, floor support and terrain penetration use the same sampled surface.
Map cubes support side blocking, standing, landing and overhead collision.
Starting Play copies the resource into runtime; stopping clears runtime. Runtime
sculpting or cell edits do not affect authoring or authoring history.

### Current limitations

There are no chunk meshes, GPU height displacement, LOD, material textures for
resource cells, holes, overhangs or multiple world resources. Cells currently use
eight solid palette colors. Node mesh textures continue through the existing
asset bank. Rendering and picking scan the bounded resource; maximum grids are
intended for compact worlds rather than large streaming landscapes. Character
terrain support samples the body center and uses the existing 0.15-unit movement
tolerance; steep slopes are blocked, without a slope-angle/step controller.
Rectangles use the construction plane rather than following curved surfaces.
World resources are selected through the World tree item or the viewport. The
resource inspector shows capacity and terrain settings; geometry remains in world
coordinates without hierarchy transforms.
Generator and brush controls are session tools; applied generation parameters and
all resulting heights are persisted. Native calls still have bounded transient
argument overhead, even though resource geometry boxes are reused.

## Scene format compatibility

`VMGE_SCENE|1` remains accepted and is emitted when no world geometry is present.
The header and 21-field node layout are retained, while node numeric values use
roundtrip-safe 17-significant-digit formatting in both save entry points. Integer
properties still emit spellings such as `1.0`; precise fractions and subnormals may
emit scientific notation. Compatibility does not require byte-identical old numeric
spellings. `se_save` and `se_world_save_scene` publish with `vfs.write_atomic`, so a
failed file write or replacement does not truncate the existing saved scene.
`VMGE_SCENE|2` retains the same 21-field node records, plus:

```text
W|side|spacing|originX|originY|originZ|seed|preset|amplitude
C|centerX|centerY|centerZ|size|palette
H|vertexX|vertexZ|localHeight
```

There is exactly one W record, preceding C/H records. The earlier eight-field W
form (without amplitude) is also accepted, defaulting amplitude to zero. Every
terrain sample must appear exactly once. Duplicate/out-of-range vertices,
incomplete surfaces, capacity overflow, nonfinite properties, unsupported
versions and malformed metadata are rejected. Heights are authoritative after
sculpting; loading does not regenerate them. Legacy `.dat` imports replace only
the resource map, preserving ordinary nodes and any terrain. The editor import
wrapper raises legacy map capacity to 4,096 without changing workspace.vy.

## CPU API

Use `engine/terrain.vy` for generation, sampling and resource edits;
`engine/world_render.vy` adds drawing and scalar ray queries. Entry points include
`se_world_paint`, `se_world_erase`, `se_world_rectangle`, `se_world_import`,
`se_terrain_generate`, `se_terrain_height`, `se_terrain_sculpt`, `se_world_ray`.
Allocate with `se_world_create` before managed frames. Generate into a staging
owner, validate, then `se_world_copy` to publish. Scene IO's atomic entry points
are `se_import_text(destination, staging, text)` and `se_load` with distinct owners.
The lower-level `se_parse` deliberately mutates its staging argument.

## Test-runner integration (main agent)

No changes to main.vy are needed: the scene editor already loads the theme before
frames, calls the world input/panel/draw paths, imports map resources through
`se_editor_commit`, and renders resource copies during Play.

Add **world** and **world_scene** to the CPU test-name tuple in `tests/run.py`;
add **world_render** to its graphics-only tuple. Compile/run them exactly like
the existing scene tests, with expected markers:

* `tests/world.vy` → `VMGE_WORLD_OK`: seed profiles, exact interpolation and
  normals, cube occlusion, sculpt kernels, 4,096-cell capacity and atomic fills.
* `tests/world_scene.vy` → `VMGE_WORLD_SCENE_OK`: interpolated strokes, normal
  construction, legacy import above 128 cells, preview/cancel/apply, undo/redo,
  atomic failed imports, v1/v2 roundtrip, maximum resource persistence, real
  rollback, repeated Play, exact character landing and zero commit drift.
* `tests/world_render.vy` → `VMGE_WORLD_RENDER_OK`: hidden-window maximum-grid
  draw/pixel/pick agreement, fixed owner sizes, balanced passes and native cleanup.

For runner-owned file fixtures, set
`VYNE_VMGE_WORLD_TEST_SCENE=str(BUILD / "world-roundtrip.vscene")` in the test
environment. `world_scene` writes that file and its `.bad` companion. Its default
is the development-only `build/world-dev-roundtrip.vscene` under the repository
working directory.

The CPU tests link vraylib for value records and a windowless managed session;
they do not open a window. The existing five-node AUTOTEST assertions and its
version-1 saved-header check do not need modification. All development outputs
are uniquely prefixed `build/world-dev-*`; local native builds use
`VYNE_RAYLIB_CACHE=<repo>/build/world-dev-raylib` to avoid the shared cache lock.

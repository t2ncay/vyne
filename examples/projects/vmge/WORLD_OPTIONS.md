# Direct world controls and procedural terrain extension

This extends `WORLD.md`. The studio integrates these direct controls, extended
resource IO and slope-aware Play movement.

## Convenient direct controls

Click the current tool to choose any of nine labeled tools directly. Click the
preset to choose Hills, Island, Plateau or Flat. **Settings...** opens a proper
window-sized modal containing labeled numeric inputs and visible current values:

| Parameter | Range / meaning |
|---|---|
| Seed | Integer 0–1,000,000 |
| Side | Integer 1–64 terrain quads per axis |
| Height | 0–1,024 world units |
| Spacing | 0.125–1,024 world units per terrain sample |
| Frequency | 0.001–2 per world unit |
| Octaves | Integer 1–8; frequency doubles each octave |
| Persistence | 0–1; weight multiplier for each finer octave |
| Spawn pad | 0 disables; otherwise radius ≥ 1.5 spacings, ≤ 1,024 |
| Radius | Sculpt radius 0.125–1,024 world units |
| Power | 0–1,024 per dab; flatten/smooth blend clamps to one |
| Map grid | 0.125–1,024, independent of terrain spacing |
| Elevation | Construction plane −100,000–100,000 |
| Flatten Y | Absolute world Y −1,024–1,024 |

Click a field, type or paste a number, use Ctrl+A, selection, cursor keys,
Backspace/Delete, Enter and Tab/Shift+Tab. These use the existing text-edit helpers
with a **separate world-owned SEUI**, not the scene inspector's editing state.
Enter commits a field to a draft. **Apply settings** validates all parameters and
publishes them together; invalid values leave the current tools unchanged.
Cancel/Escape discard the draft. Applying settings or selecting a new preset
discards a stale preview; use Preview again, then Apply terrain to create one undo
step. The world itself is never mutated by numeric field editing.

The compact overlay retains current tool/preset, palette swatch, cell count,
grid/elevation, seed, side/height, radius/power and Ready/Stroke/Preview readouts.
Controls remain within the same 114-pixel input-exclusion band. Buttons fit narrow
viewports; the full modal provides readable values without squeezing the fields
into that overlay.

The editor preallocates 128 Int64 text elements, 256 paste elements, 64 editing
flags, 32 metrics, 128 unused compatibility elements, 32 draft Float64 elements,
160 numeric-format scratch elements and five modal-state integers. No retained
array grows. Unfocused numeric labels use static digit strings. Focused text is
read transiently from the owned buffer. Exact initial edit values avoid the
runtime's six-significant-digit default Float64 string conversion.

## Deterministic smooth procedural terrain

`engine/terrain_procedural.vy` implements version-1 smooth value-noise fBm, using
the existing bounded integer hash and smoothstep-interpolated lattice values.
Frequency uses **world units**, not grid indices. Each octave doubles frequency;
persistence controls its amplitude, and the sum is normalized. The same seed,
parameters, origin and grid produce identical Float64 samples.

Hills use fBm, Island applies radial coast falloff, Plateau has a bounded flat
top, and Flat is zero. An optional central spawn pad is flat within its requested
radius and smoothly blends into surrounding terrain over a transition annulus.
Its minimum radius guarantees complete central flat triangles. Pad construction
happens in preview, not during Play, and preserves map cells.

`se_terrain_generate_procedural(world, side, spacing, amplitude, seed, preset,
frequency, octaves, persistence, pad_radius)` validates before mutation. World
tools use it for new previews. `se_terrain_generate` remains the unchanged legacy
generator for existing callers. Do not call that legacy function on a modern
resource and then retain its G metadata: clear header 9–15 when deliberately
switching generator implementations.

## Versioned storage and exact numeric exports

The existing W, C, H and 21-field node records are unchanged. The extended writer
uses scene version 2 and adds one optional record:

```text
G|1|frequency|octaves|persistence|padRadius|padWorldX|padWorldZ
```

Numeric header elements 9–15 hold these values, so existing numeric scene copies
and history snapshots already include them. Stored heights remain authoritative
after sculpting. The extended reader accepts existing v1/v2 scenes without G,
rejects duplicate/unsupported/invalid G records, and publishes only after both
geometry and generator metadata validate in real staging.

Use the opt-in entry points from `engine/world_resource_io.vy`:

* `se_world_scene_text` / `se_world_parse_scene` (the latter mutates staging);
* `se_world_import_scene(destination, staging, text)` (atomic publication);
* `se_world_load_scene` / `se_world_save_scene` (disk).

Ordinary node-only scenes retain the v1 header and 21-field layout, but their
numeric spellings now use the exact 17-digit Float64 formatter too. This is record
compatibility, not an unchanged byte-for-byte serialization contract. Integer
properties retain spellings such as `1.0`; other values may use scientific notation.
The studio uses the extended entry points for world scenes to retain G metadata.
`engine/world_numbers.vy` expands
the binary rational with bounded base-10^7 limbs in existing scratch, then rounds
to 17 significant decimal digits, ties to even. This roundtrips numerical values,
including subnormals, without a runtime binding change. Signed zero is canonical.

Both `se_save` and `se_world_save_scene` use `vfs.write_atomic`: serialization
finishes before a private sibling is written and atomically replaces the target.
Write/close/replacement failures are surfaced to the caller, and do not truncate
the existing destination.

Explicit map export APIs in `engine/world_export.vy`:

* `se_world_map_text` / `se_world_save_map(world, path)` write legacy `.dat`;
* `se_world_map_obj` / `se_world_export_obj(world, path)` write geometry-only OBJ.

DAT preserves numeric centers, sizes and palette indices; it does not preserve
original lexical spellings. OBJ has eight vertices, six outward-facing quads and
one palette-named group per cube, in world coordinates. These APIs export the
**map resource only**; terrain and node meshes are explicitly omitted.

Import and OBJ export use explicit 128 MiB allocation-volume bounds; scene and
DAT saves use 64 MiB. Those are volume bounds, not peak-memory claims. Maximum
4,096-cell / 64-side resources are tested inside those bounds. Export is an
explicit operation, not a frame-loop action.

## Explicit slope limit and optional spawn hook

`engine/world_motion.vy` selects the exact triangle used by render/height queries
and computes its upward normal Y. The walking limit is **50 degrees**:
`normalY >= cos(50°) = 0.6427876096865394`. The query rejects grounded uphill/level
travel on steeper triangles, permits downhill escape, and permits airborne travel
above the surface.

`engine/world_character_motion.vy` provides the integrated per-axis query:

```text
se_world_motion_blocked(scene, cache, previous, candidate,
                        bodyHeight, excludedCharacter, grounded)
```

On success it stores accepted camera Y in `scene.world.work[4238]`. Grounded bodies
follow exact sampled support on walkable terrain, including rises greater than
the old 0.15-unit proxy. A displacement/angle bound prevents jumping an intervening
height discontinuity. Map and ordinary node solid collisions are still checked
against the vertically adjusted body. It does not introduce sliding or a general
step/climb controller. Airborne penetration retains a separate tolerance; that is
not the slope-angle rule.

`se_world_spawn_point(world, bodyHeight)` returns an optional generated-pad camera
position in work elements 4235–4237. It neither modifies authoring nor regenerates
terrain. Main can expose this as an explicit Play spawn option. It does not test
other node/map obstacles placed over the pad.

## Integration reference

1. **Before global edit/shortcut/input processing**, cache
   `captured = se_world_tools_modal_open(world_tools)`. If captured, call
   `se_world_tools_modal_input(world_tools)` and suppress global keyboard/mouse
   editing, selection, orbit, splitters, file/Play shortcuts and interactive
   background panels for that entire frame. Keep the cached flag even if Escape
   closes the modal, so Escape does not also quit the editor.
2. Use `se_world_tools_busy(world_tools)` for toolbar/history/file/Play guards:
   it includes strokes, previews and modals. Existing world input returns early
   while a modal is open. Continue excluding the 114-pixel overlay for viewport
   picking/painting. Modal-owned input must not be processed twice.
3. **After all background panels and their clips have ended**, and before
   `app.end()`, call `se_world_tools_modal_draw(app, world_tools)`. This draws the
   full-window modal and consumes picker/settings publication events. Existing
   `se_world_tools_panel` opens it but deliberately does not draw it in the
   viewport clip.
4. Route resource Open/Save/string imports through the extended IO entry points
   above, including startup loading. After successful loading, cancel the tools
   modal and call `se_world_tools_load_options(world_tools, authored.world)`.
   Existing map import behavior remains resource-only. New options are persisted
   when Preview is applied, not when only the settings dialog is applied.
5. In Play's horizontal substeps, replace the two old `se_body_blocked` movement
   probes with `se_world_motion_blocked`. Use this order:

   ```text
   candidate.y = previous.y
   X from = previous
   X to = (candidate.x, previous.y, previous.z)
   blocked X: candidate.x = previous.x
   accepted X: candidate.y = runtime.world.work[4238]

   Z from = (candidate.x, candidate.y, previous.z)
   Z to = (candidate.x, candidate.y, candidate.z)
   blocked Z: candidate.z = previous.z
   accepted Z: candidate.y = runtime.world.work[4238]
   player.camera.set_position(candidate)
   ```

   Pass current `player.grounded()`, body height and character handle. The helper
   is already imported through world_tools.vy; Play modules can import it directly.
   Keep the existing crouch/headroom and vertical gravity/landing logic.
6. Optionally, after `se_play_start` has copied runtime and established the camera,
   use `se_world_spawn_point(runtime.world, player.height())`, set the camera from
   work[4235..4237], and clear velocity. Gate it behind an explicit setting; do not
   override an authored camera by default. Export menu actions can directly call
   `se_world_save_map` and `se_world_export_obj` with their chosen paths.

## Precise performance characteristics

Geometry storage and 128-node capacity remain unchanged. At side 64 the renderer
issues **8,192 native triangle calls per frame**, plus one cube call per map cell
(at most 4,096). It reuses four vertex records and ten colors; native argument
overhead is transient, not free. Drawing is O(side² + cells), and scalar picking
tests both triangles of every quad plus all map cells with the same complexity.
Each movement probe checks up to all map cells and 128 ordinary nodes, plus O(1)
terrain sampling. Each sculpt dab snapshots 4,225 numbers and scans active samples.
Generation does at most (4,225 + 1) × 8 octave evaluations (including the central
pad target), each with four lattice hashes. Node exclusion tests additionally walk
bounded ancestor chains, up to 128 hops per tested node.
No batching, culling, spatial acceleration, chunk meshes, LOD or streaming was
added. These are explicitly bounded compact worlds, not a scalable world-streaming
renderer. The maximum-grid graphics test proves storage/lifetime behavior and
visible triangles, not an FPS guarantee for arbitrary scenes.

## Unique tests and runner additions

Add CPU tests:

* `tests/world_options.vy` → `VMGE_WORLD_OPTIONS_OK`: atomic numeric edits/cancel,
  independent spacing, fBm parameter effects/determinism, exact Float64 payload
  roundtrip, pad normals/spawn, version compatibility and atomic metadata failures,
  disk persistence, slope motion with ordinary obstacles, bounded repeated edits
  and maximum extended resources.
* `tests/world_exports.vy` → `VMGE_WORLD_EXPORTS_OK`: legacy DAT roundtrip,
  exact numbers including subnormals, OBJ group/index/bounds/outward-winding checks,
  disk exports and maximum-cell exports.

Add graphics-only `tests/world_settings_render.vy` →
`VMGE_WORLD_SETTINGS_RENDER_OK`: settings/tool/preset modal rendering, screenshots,
fixed owner storage, balanced passes and closed native handles. Its screenshots
hold each view for two frames before capture to account for desktop back-buffer
presentation.

Runner environment paths:

```python
env["VYNE_VMGE_WORLD_OPTIONS_SCENE"] = str(BUILD / "world-options.vscene")
env["VYNE_VMGE_WORLD_EXPORT_PREFIX"] = str(BUILD / "world-exports")
env["VYNE_VMGE_WORLD_CONTROLS_PREFIX"] = str(BUILD / "world-controls")
```

Development binaries/artifacts use `build/world-dev-*` and the separate
`build/world-dev-raylib` cache. This pass does not edit scene_editor.vy, UI files,
Play/IO modules, runtime bindings, main.vy or tests/run.py.

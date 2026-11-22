# Scene / World / Shader Studio host

`src/editor/scene_editor.vy` hosts the studio in the existing application and
session. `src/main.vy` dispatches `VYNE_VMGE_MODE=scene` (default), `world`,
`shader` and `rendering` to this shared host: `se_scene_editor()` starts in Scene,
and `se_scene_editor_workspace(1/2/3)` starts World/Shaders/Rendering respectively.
Historical `editor` and `play` modes remain separate map applications. The project
runner includes shader and host authoring checks. For launch, file selection and
end-to-end user workflows, see [Shader Studio](SHADER_STUDIO.md).

## Working workflow

1. Select a mesh in Scene. Its inspector has a **Shader** row: open the current
   material in Studio, or open the editable document when no shader is assigned.
   **Clear** returns that node to its ordinary material and records undo.
2. `.vshader` and host `.vmaterial` assets have **Open in Shader Studio**. Replacing a dirty editable
   document requires Save / Discard / Cancel. Loading synchronizes both editors
   and saved reset buffers. Publications and other source resources load into a
    separate editable project copy (default `shaders/studio.vshader`, configurable
    at startup or through Save As). Opening a selected
   material intentionally restores its instance values after the new document
   has been loaded and its material reseeded. Saving the copy never changes the
   publication that existing scenes/undo refer to.
3. Compile source or generate GLSL with the existing studio. Apply is consumed
   once, validates the selected authored mesh, and publishes a full, immutable
   `.vshader` sidecar before assigning a separate `MaterialInstance`. Apply during
   Play is rejected. Cached geometry is shared; uniforms are reset for every draw
   through `ss_material_draw`, material slot **0**.
4. For samplers, define scalar sampler2D metadata in the shader resource. The
   host's **Sampler: name** button cycles the target parameter. In Scene's asset
   browser, select a texture and use **Bind studio sampler**, then Apply. Import
   into the project first for a project-relative reference. The host caches
   texture tokens separately from their persistent resource paths/IDs. Sampler-only
   edits are dirty. Studio Save atomically commits a `.vmaterial` checkpoint with
   the full shader, material and R table; reopen resolves its saved texture paths.
   Reset restores the saved schema, material values, bindings and resource table.
   Reset of an imported/publication copy retains its unsaved-copy marker until
   that copy has actually been committed to the editable project path.
5. Scene Save/Open and Play preserve instance values, orphan metadata, shader
   references and texture references. Undo/redo and duplication copy values,
   not shared shader parameters. Save the scene after applying a material.

Rendering is a real ordered pass-list workspace: enable/bypass, add passes,
select a compiled pass shader, choose predecessor/external inputs, change divisor,
format and extent, remove/reorder passes, capture, inspect output, save pipeline, and compare
linear reference pixels. It uses the existing pipeline engine and shared studio
preview controls, with its own Rendering title. Shaders retains source/vertex,
graph, parameters and diagnostics from the UI contract.

World controls occupy the left tools area rather than the first 114 viewport
pixels. Scene/World share orientation controls, camera tools, splitters and a
distance-adaptive restrained grid. Maximize/F11 expands the viewport; inspector
and drawer widths use shared logical/DPI values. Legacy autotests keep their
original viewport/input geometry.

## Frame and shutdown ordering

- `se_workspace_host` calls `ss_workspace_create` exactly once, preframe.
- All document, material, source-history and registry storage is preallocated.
  Native wrappers and temporary decoded strings never enter a retained registry.
  Host state is explicitly constructed with `ss_ints`, including modal state.
- Active Shader/Rendering input and `ss_workspace_frame` run before `app.begin`.
  Studio drawing runs inside the shared drawing surface. Navigation is dispatched
  once when input is available; tab changes affect input on the next frame.
- Scene edit dispatch is nested behind the active-workspace gate: Vyne boolean
  operators must not be used to short-circuit effectful input/save calls.
- Host/scene/world dialogs freeze background input for the entire captured frame.
  Background scene passes are suspended in Shader/Rendering workspaces.
- Window close and Exit preserve studio and scene Save / Discard / Cancel
  decisions. Save failures retain the pending action and dirty state. Studio Save
   writes shader/material, graph and pipeline; their individual studio saves also
   work. Layout failures are checked before committing editable source/material,
   retain the pending Close action, and do not silently stop the application.
- Layout flushes on workspace changes, splitter release and shutdown. Pipeline
  targets, editable/pass programs and all eight published documents are explicitly
  closed/collected before the shared App closes its geometry/textures/window.

## Persistence and compatibility

Ordinary scenes continue to write v1, world scenes v2. Material/resource scenes
write **`VMGE_SCENE|3`** with unchanged 21-field node records and world W/C/H/G
records. The world-resource entry points preserve G alongside the new records.

```text
M|node-slot|document-id|instance-id|UTF8-hex-shader-reference
P|node-slot|P|stable-parameter-id|GL-type|array-length|width|active|exact-values...
P|node-slot|T|stable-parameter-id|texture-resource-id
R|texture-resource-id|UTF8-hex-texture-reference
```

Instance ID is node slot + 1; the shader path is the authoritative resource
reference. R IDs occupy 100..163. Only live mesh nodes accept M/P records. Load
validates candidate staging before replacing the authored scene. Numeric material
values use the engine's exact binary-rational encoding. No shader/texture native
token or App session is written to disk. Texture IDs must resolve through R after
reopen. History additionally snapshots all material values and resource paths.

Published resources normally live at `shaders/studio/host-<content-hash>.vshader`.
Existing matching resources are reused; collisions are checked against the full
resource text, and fresh registry loads verify the filename's content hash.
Source saves to a publication path are rejected unless the original text is
identical. Include-backed Apply verifies the compiled dependency hashes and freezes
expanded GLSL into the sidecar (64 KiB per expanded stage maximum). Changing an
authored include cannot change that publication after reopen. The editable document
keeps its authored include references. Sidecars are atomically published before
the scene reference, and never overwritten by later studio edits.

Editable `studio.vmaterial` uses this host checkpoint format:

```text
VMGE_STUDIO_MATERIAL|1
S|UTF8-hex-full-vshader-text
VMGE_MATERIAL|1|document-id|instance-id
P|...exact material rows...
T|parameter-id|resource-id
R|resource-id|UTF8-hex-resource-path
```

The checkpoint is the authoritative completed source/material Save commit and
is atomically written as one file (2 MiB maximum). A failure between the editable
`.vshader` write and this commit preserves the old checkpoint and dirty state;
reopen loads the old complete checkpoint rather than a mismatched pair. Native
tokens/sessions are never encoded. This host checkpoint embeds the ordinary
material format; it is loaded by `se_host_load_material`, not the standalone
engine `ss_material_load` entry point.

## Explicit ownership limits

| Owner | Bound |
| --- | --- |
| Authored / runtime / staging scene | 128 preallocated instances each, allocated with `ss_material_sized(...,16,16)` |
| Material snapshot / assignment | 16 parameter/orphan rows, 64 components per row, 16 sampler bindings |
| Undo history | 17 fixed snapshots, with separate metadata/path/resource sections avoiding power-of-two padding |
| Shared Rendering registry | 10 fixed entries: eight preallocated bank documents (IDs 3..10), editable 1, identity 2; no eviction |
| Studio documents | 1 editable, 1 identity pass, 1 noncompiled publication staging document |
| Shader text | Engine 64 KiB per stage; engine schema/reflection limits remain in force |
| Scene paths / texture references | 255 UTF8 bytes; shader-program canonical cache paths 2048 bytes |
| Texture resource table / native cache | 64 each; 4096 per axis; 128 MiB estimated RGBA8 uploads total |
| Hosted pipeline | 16 passes; 512 per target axis; 64 MiB aggregate color+depth target budget |
| Reference samples | Preallocated 512×512×4 Float64 components (8 MiB) |
| Scene text / individual record | 16 MiB / 2048 characters |
| Editable checkpoint | 2 MiB disk text, saved material/schema/resource snapshots preallocated |
| Whole preframe editor owner storage | Enforced 128 MiB main-arena delta; recorded portable authoring fixture 133,974,016 bytes / 127.768 MiB; startup path/bookkeeping can add a small amount |

The registry deliberately reports capacity instead of silently replacing a
program that existing nodes/history use. Reopening the editor starts a fresh
registry. GPU and imported asset budgets above are separate from owner-array
storage and the existing scene geometry/palette cache.
The host draws overrides only at material slot 0. Its 16 stored sampler bindings
do not raise the native limit of four managed scalar sampler2D bindings per program.

## Host APIs / UI coordination

- `se_host_save_editable(host) -> Bool`: checked source/material/layout commit.
- `se_host_save(host) -> Bool`: checked source/material/layout plus graph/pipeline.
- `se_host_reset(host) -> Bool`: saved material/schema/resource restoration.
- `se_host_material_dirty(host) -> Bool`: revision-aware semantic material/R check.
- `se_host_finish(app,host) -> Bool`: staged replacement, fresh material seeding,
  editable-copy path and optional atomic checkpoint load.
- `se_host_resolve_editable(host,assets) -> Bool`: resolve persisted R references;
  also runs after Reset before the next preview.
- `se_material_copy(to,from)` delegates to capacity-aware `ss_material_copy`.
- `se_host_navigation_ready` includes cached modal ownership and exact-entry
  **field ID 0**, including the frame in which it closes.
- Pending exact-entry text participates in Close dirty decisions. Cancel retains
  it; `se_host_commit_parameter` dispatches through `ss_workspace_commit_field`
  for numeric, range, group, units, defaults and path field IDs. Invalid text
  leaves the pending action intact; host-polled input is claimed before UI update.
- `se_host_pipeline_prepare/close` adapt the engine's sparse fixed target arrays
  to compact live-token native release banks, with candidate-first replacement.

Hosted UI action slots are **42 hosted / 43 Save / 46 Reset / 47 Save As / 53 Reload**;
**55** is whole-frame capture. 44/45 remain exact-entry shape guards. The host uses
the published commit/capture/claim and reset/save-as/reload completion APIs;
coordinate polling is removed. Save As stages the new source/material identity,
backs up an existing destination, commits both files before publishing live paths,
and restores destination files and path focus on failure. Save As requires a
mutable `.vshader` destination; its original empty include roots are pinned before
the filename changes. Confirmed Reload restores the current resource checkpoint
without a second host dirty prompt. Graph individual Save remains the UI owner's
API; Rendering Save persists its shader-path ledger. Main's host reads
`VYNE_VMGE_SHADER_DOCUMENT`, `VYNE_VMGE_LAYOUT`, `VYNE_VMGE_SHADER_GRAPH` and
`VYNE_VMGE_SHADER_PIPELINE`; defaults are `<project>/shaders/studio` with
`.vshader`, `.layout`, `.vgraph` and `.vpipeline` extensions. Explicit relative
overrides follow the process working directory. Internal configurable paths are
`editable_path`, `graph_path`, `pipeline_path`, and `publish_directory`.

### Rendering library and transactional pass editing

`se_host_bind_registry` runs preframe and shares the same bank array with the UI.
Its parallel IDs are `[3,4,5,6,7,8,9,10,1,2]`; callers look up IDs rather than array
positions. No extra `SSDocument` storage is constructed. Gallery load preflights
remaining bank capacity, stages programs into unused slots, remaps gallery IDs
201..205, validates the whole candidate, then publishes paths/count/descriptors.
Failure closes staged programs and preserves the existing pipeline and history.
Bloom defaults to 512×512; its five color+depth targets use about 25 MiB.
Rendering's external input is the Studio material preview. It is not a full-scene
postprocess hook. The eight bank slots are shared with scene material resources.

- `se_host_pipeline_edit(host,kind,pass_id,value)` performs candidate rollback for
  shader/input/divisor/format/extent/toggle/remove/reorder edits.
- Shader cycling includes loaded pipeline-compatible programs with an active
  scalar `inputTexture`. Two-input programs automatically expose `inputTexture1`;
  absent second inputs are valid only for one-input shaders.
- Inputs cycle through external and prior pass IDs. Referenced removals and
  dependency-breaking reorders are rejected; reroute consumers first.
- Extents cycle relative / 128 / 256 / 512. Divisor edits select relative sizing.
- `se_host_pipeline_load/save` preserve descriptors plus shader paths in an atomic
  `.vpipeline.host` checkpoint (`VMGE_RENDER_PIPELINE|1`, embedded pipeline text,
  `D|saved-id|UTF8-hex-path` rows). Reopen loads/reuses slots and remaps IDs even
  when scene materials already occupy the bank. The plain `.vpipeline` is also
  written for engine tools. The host checkpoint is authoritative on reopen.
- Gallery blur `texelStep` uses the actual input extent after bypass/divisor changes.
  Geometry draws supply the actual `cameraPosition` context without changing the
  authored material values. Shader compilation and resource loads happen outside
  active camera/target/drawing passes.
- Rendering's **Load bloom gallery** action dispatches through
  `se_host_pipeline_actions` to `se_host_pipeline_load` with
  `<project>/shaders/studio/bloom.vpipeline`. **Open plasma seed** and
  **Open lit material** open editable preview documents through `se_host_open`.
  Includes stay in the UI's read-only source drawer; external-open requests report
  explicit status when no OS launcher is configured, never treating includes as
  shader resources.

## Verification

Run from the repository root with the isolated native cache:

```powershell
$env:VYNE_RAYLIB_CACHE="build/shader-host-dev-cache"
$env:VYNE_POISON_ARENA="1"
$env:VYNE_VMGE_ASSETS="procedural"
.\vynec.exe --compile -o build/shader-host-dev-material examples/projects/vmge/tests/material_scene.vy
.\build\shader-host-dev-material.exe
.\vynec.exe --compile -o build/shader-host-dev examples/projects/vmge/tests/shader_host.vy
$env:VYNE_VMGE_SHADER_AUTOTEST="1"
$env:VYNE_VMGE_HIDDEN="1"
$env:VYNE_VMGE_SCENE="build/shader-host-dev-absent.vscene"
$env:VYNE_VMGE_SHADER_PREFIX="build/shader-host-dev-review"
.\build\shader-host-dev.exe
```

`VYNE_VMGE_SHADER_AUTOTEST=1` runs **32 frames**. Do not combine it with the
ordinary `VYNE_VMGE_AUTOTEST` flow. `VYNE_VMGE_SHADER_PREFIX` overrides the default
`build/shader-host-dev-flow` artifact prefix. Publications are isolated beneath
that prefix's `-published` directory; layout, scene, pipeline and screenshots
also stay beneath `build/` by default.
Sampler fixtures are generated 16×16 PNGs at `<prefix>-texture-a.png` and
`<prefix>-texture-b.png`; image owners are closed immediately after export.
Neither shader-host fixture depends on ignored `examples/assets/` images or an
asset download. Set `VYNE_VMGE_ASSETS=procedural` for the same palette branch in CI.
Fixture source/material/layout/pipeline saves use only their prefix paths.

Markers:

- `VMGE_MATERIAL_SCENE_OK`
- `VMGE_SHADER_HOST_COMPILE_OK`
- `VMGE_SHADER_HOST_PIXELS_OK`
- `VMGE_SHADER_HOST_MATERIAL_OK`
- `VMGE_SHADER_HOST_IMMUTABLE_OK`
- `VMGE_SHADER_HOST_INCLUDE_OK`
- `VMGE_SHADER_HOST_RESEED_OK`
- `VMGE_SHADER_HOST_INPUT_OK`
- `VMGE_SHADER_HOST_EDITABLE_MATERIAL_OK`
- `VMGE_SHADER_HOST_PIPELINE_OK`
- `VMGE_SHADER_HOST_REGISTRY_OK`
- `VMGE_SHADER_HOST_RESOURCES_OK`
- `VMGE_SHADER_HOST_CLOSE_OK`
- `VMGE_SHADER_HOST_LAYOUT_FAILURE_OK`
- `VMGE_SHADER_HOST_AUTOTEST_OK`
- `VMGE_SHADER_HOST_MEMORY_OK`
- `VMGE_SCENE_EDITOR_COMPLETE` with `frames=32`, `live_handles=0`

The GPU run edits through the code editor, rejects a bad real compiler candidate,
recovers, draws two nodes sharing a cached model into a float target and checks
0.25/0.75 pixels, roundtrips scene/history, captures/reopens a real pipeline,
isolates a modal, resizes, exercises Play, resolves imported sampler references,
compiles all eight published copies, rejects the ninth, opens/edits/saves an
assigned publication while preserving the original scene, reseeds matching-ID
document B, freezes include content and checks fresh-reopen pixels after an
include mutation, roundtrips sampler-only Save/Reset, isolates exact-entry ID 0,
and tests source/layout Save failures and Discard/Cancel. Expected `commit_drift=0`,
zero editor errors and zero live handles. Main-arena source-owner bookkeeping
is explicitly bounded to 4096 bytes; measured 2944 bytes in the poisoned run.

Screenshots: `build/shader-host-dev-flow-rendering.png`, `-scene.png`, `-shader.png`.
With the review prefix above, use `build/shader-host-dev-review-{rendering,scene,shader}.png`.

### Final runner authoring fixture

```powershell
$env:VYNE_RAYLIB_CACHE="build/shader-host-dev-cache"
$env:VYNE_POISON_ARENA="1"
$env:VYNE_VMGE_ASSETS="procedural"
$env:VYNE_VMGE_SHADER_AUTOTEST="1"
$env:VYNE_VMGE_SHADER_AUTHORING_AUTOTEST="1"
$env:VYNE_VMGE_HIDDEN="1"
$env:VYNE_VMGE_SCENE="build/shader-host-dev-authoring-absent.vscene"
$env:VYNE_VMGE_SHADER_PREFIX="build/shader-host-dev-authoring"
.\vynec.exe --compile -o build/shader-host-dev-authoring examples/projects/vmge/tests/shader_host_authoring.vy
.\build\shader-host-dev-authoring.exe
```

This separate bounded **32-frame** fixture tests scalar/schema/default/path field
dispatch, expose/metadata Apply, Save As success/failure with identity/checkpoint/
focus preservation, explicit Reload discard acknowledgment, Reset acknowledgment,
schema capture with no text field, ten shared documents, real bloom and edited-pass
pixels, input/format/divisor/extent transactions, valid and rejected reorder/remove,
shader-path ledger reopen, lit-material Apply/Play and capacity preflight at all
eight occupied bank slots. It constructs no gallery document owners.

Markers: `VMGE_SHADER_HOST_AUTHORING_OK`, `VMGE_SHADER_HOST_SAVE_AS_RELOAD_OK`,
`VMGE_SHADER_HOST_RENDERING_GALLERY_OK`, `VMGE_SHADER_HOST_LIT_APPLY_OK`,
`VMGE_SHADER_HOST_RENDER_BANK_OK`, `VMGE_SHADER_HOST_AUTHORING_AUTOTEST_OK`,
`VMGE_SHADER_HOST_MEMORY_OK`, and completion with `frames=32`, `live_handles=0`.
Recorded portable preframe **133,974,016 bytes**, `commit_drift=0`, `errors=0`,
source-owner bookkeeping **2752 bytes**; the portable normal-host fixture uses
**133,973,568 preframe bytes** and **2944 bookkeeping bytes**.
These are recorded fixture measurements; startup path/bookkeeping changes may add
a small amount. The live 128 MiB constructor guard is authoritative.
Clear `VYNE_VMGE_SHADER_AUTHORING_AUTOTEST` to run the original shader-host fixture;
that fixture also passes with **2944** bookkeeping bytes and zero drift/handles.

Legacy verification uses the same executable after clearing shader autotest:
`VYNE_VMGE_AUTOTEST=1`, `VYNE_VMGE_FRAMES=16`, an isolated `VYNE_VMGE_SCENE` path.
Expected five nodes, Edited Crate at X=3, one Play cycle, `commit_drift=0`, zero
handles. Add `VYNE_VMGE_STUDIO_AUTOTEST=1` and an isolated
`VYNE_VMGE_STUDIO_PREFIX` for `VMGE_STUDIO_AUTOTEST_OK` and the existing G/modal/
save/export workflow. Both legacy flows passed with arena poisoning.

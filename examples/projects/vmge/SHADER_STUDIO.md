# VMGE Shader Studio

Shader Studio is the integrated, code-first shader workspace in VMGE. Write
GLSL, compile against the live driver, expose typed controls, preview on geometry,
and Apply independent material instances to scene meshes. A typed graph can
generate GLSL, and Rendering runs an editable ordered multipass pipeline.

## Launch and choose your files

Use a current `vynec` and the desktop raylib toolchain described in the repository
installation guide. Graphics require a live **OpenGL 3.3** context; shader/runtime
operations run on the application's **main thread**. The first compilation may
build the cached native libraries.

From the repository root in PowerShell:

```powershell
$env:VYNE_VMGE_MODE = "shader"
$env:VYNE_VMGE_ASSETS = "procedural"
.\vynec.exe --run examples/projects/vmge/src/main.vy
```

Or, from `examples/projects/vmge/` with `vynec` on PATH, set the same environment
variables and run `vynec run`. Project discovery supplies the fonts and assets;
set `VYNE_VMGE_ROOT` to an absolute project directory for a relocated executable.
Procedural assets need no external texture download.

| `VYNE_VMGE_MODE` | Entry workspace/application |
| --- | --- |
| `scene` (default) | Scene in the shared studio |
| `world` | World in the shared studio |
| `shader` | Shaders in the shared studio |
| `rendering` | Rendering in the shared studio |
| `editor`, `play` | Historical map editor and map player |

Scene, World, Shaders and Rendering share one window, App and session. Use the
top navigation to switch; Scene's Play/Stop is separate from historical `play` mode.

These overrides are read by the integrated host at startup. Defaults are relative
to the discovered project; explicit relative paths follow the process working
directory. Prefer absolute overrides, with **existing writable parent directories**.

| Environment variable | Default under `<project>/` |
| --- | --- |
| `VYNE_VMGE_SHADER_DOCUMENT` | `shaders/studio.vshader` |
| `VYNE_VMGE_LAYOUT` | `shaders/studio.layout` |
| `VYNE_VMGE_SHADER_GRAPH` | `shaders/studio.vgraph` |
| `VYNE_VMGE_SHADER_PIPELINE` | `shaders/studio.vpipeline` |
| `VYNE_VMGE_SCENE` | `scene.vscene` |

For example, before launching from the repository root:

```powershell
$project = "$PWD/examples/projects/vmge"
$env:VYNE_VMGE_SHADER_DOCUMENT = "$project/shaders/my-surface.vshader"
$env:VYNE_VMGE_LAYOUT = "$project/shaders/my-studio.layout"
$env:VYNE_VMGE_SHADER_GRAPH = "$project/shaders/my-surface.vgraph"
$env:VYNE_VMGE_SHADER_PIPELINE = "$project/shaders/my-render.vpipeline"
```

An absent shader path starts the gain shader with a proposed Save path. An absent
scene path starts the demo. Saved shader/material checkpoints and saved Rendering
ledgers are restored by the host. Clear overrides when returning to defaults.

## Create a GLSL uniform, expose it, and edit pixels

1. Start with an **absent `VYNE_VMGE_SHADER_DOCUMENT` path** so the starter has
   the `gain` schema used below, then open **Shaders**. Use **Save As**
   (Ctrl+Shift+S) to choose a new mutable
   `.vshader` path, such as `<project>/shaders/my-surface.vshader`. Existing targets
   require **Replace** confirmation. This also creates its `.vmaterial` checkpoint.
2. In **Fragment**, select the source (Ctrl+A) and paste this complete starter.
   Leave **Vertex** empty to use the standard GLSL 330 vertex stage.

   ```glsl
   #version 330
   in vec2 fragTexCoord;
   out vec4 finalColor;
   uniform float gain;
   uniform vec3 tint;
   void main() {
       finalColor = vec4(tint * gain, 1.0);
   }
   ```

3. Click **Compile** or press Ctrl+Enter. This builds a real candidate program.
   `tint` must actually contribute to the output: optimized-out declarations do
   not appear in active reflection. A black preview before exposure is expected
   because the new uniform has no authored values yet.
4. In the **Parameters** inspector, switch from **Values** to **Expose / Schema**.
   Find `tint` (`vec3`, one element) and click **Expose uniform**.
5. Keep its assigned stable ID and reflected shape. Set Minimum `0`, Maximum `1`,
   Group `Surface`, Units `linear RGB`, and Default components `0.2`, `0.6`, `1`.
   Click each field to edit, then Enter to stage it; scroll to the defaults if
   necessary. **Continuous** enables continuous values; **Fit** is evaluation API
   metadata, not a GUI optimization command.
6. Click **Apply schema**. The whole draft is validated, compatible values are
   migrated, and bindings are compiled. Return to **Values**: `tint` now has three
   controls. Drag a component, or right-click it for exact numeric entry; Enter
   commits and Escape cancels. Set `gain` to `1` to inspect the tint directly.
7. Click **Save** / Ctrl+S to checkpoint source, schema and material values. To
   assign the result, select an authored mesh in **Scene**, return to **Shaders**,
   then click **Apply**. Save the scene separately in Scene.

**Apply schema** edits the shader's control schema; toolbar **Apply** publishes a
material assignment to the selected scene mesh. Numeric value changes update
preview uniforms without a source rebuild, but require Apply to update the scene.
Schema errors leave the previous schema, values and program usable.

Use a parameter's **Schema** action to edit ranges, defaults, group, units and
flags or adopt a changed reflected shape. Stable IDs preserve compatible overrides;
missing/type-changed rows remain as orphans. A new range must contain retained
current values as well as defaults. Integer controls require exact Int32 values,
booleans exactly `0` or `1`; finite continuous values are bounded to ±1,000,000
and the declared range.

## Gallery: apply a lit material to a scene mesh

1. In **Scene**, select a mesh in the hierarchy or viewport. Use a cube or another
   normal-bearing mesh; select an authored node while Play is stopped.
2. In the **Assets** browser, browse/search `shaders/studio/`, select
   [`lit-material.vshader`](shaders/studio/lit-material.vshader), and click
   **Open in Shader Studio**. Resolve any Save / Discard / Cancel prompt.
   Open the `.vshader` resource, whose fragment, custom vertex and schema are
   embedded; the `.fs`/`.vs` companions are readable authoring copies.
3. In **Shaders**, click **Compile**. Choose Cube, Sphere or **Selected model** in
   preview controls. Adjust `baseColor`, `accentColor`, `contrast`, pattern or rim
   values; `normalView = 1` displays encoded world normals.
4. Click **Apply**, then return to Scene to inspect the selected node. The host
   supplies the actual `cameraPosition` for geometry draws. Save the scene.
5. To give another mesh different values, select it in Scene, return via the
   **Shaders navigation tab**, edit Values, and Apply again. Each node owns its
   overrides. Duplication, undo/redo, scene reopen and Play copy instance values;
   changing one assignment does not mutate another node's material.

The Scene inspector's **Shader** row reopens that node's assigned material and
restores its instance values into an editable copy. Use it when editing an existing
assignment. **Clear** restores the node's ordinary material and records undo.
Apply rejects non-mesh selections and Play-time authoring. Geometry is cached and
shared; the host overrides **material slot 0** only.

Other ready-to-edit resources include procedural
[`plasma-grid.vshader`](shaders/studio/plasma-grid.vshader), exposure tonemap,
bright extraction, horizontal/vertical blur and bloom composite. See the
[gallery index](shaders/studio/GALLERY.md) for parameter IDs and developer examples.
Exposed `time` and `resolution` are manually controlled Values; automatic preview
context only updates unexposed compatible uniforms. Plasma's exposed `time` can
be edited directly (default period: 8 seconds).

### Bind a texture sampler

Compile a used scalar `uniform sampler2D`, expose it through Expose / Schema,
and select it with the host's **Sampler: name** button. In Scene's asset browser,
select a texture and click **Bind studio sampler**, then Apply. Import external
images first for project-relative references. Save checkpoints sampler-only edits;
reopen/Reset resolve saved texture paths. Every active material sampler must be
resolved before drawing. Sampler arrays and non-2D sampler types are unsupported.

## Rendering: load and edit real bloom

1. Open **Rendering** and click **Load bloom gallery**. The deferred host action
   `se_host_pipeline_actions` calls `se_host_pipeline_load` on
   [`shaders/studio/bloom.vpipeline`](shaders/studio/bloom.vpipeline): it loads and
   compiles five actual pass programs, remaps gallery IDs into the shared bank,
   validates the candidate, and enables the pipeline at 512×512.
2. The external input is the **Studio material preview**, not a live full-scene
   postprocess. **Open plasma seed** or **Open lit material** opens that editable
   preview document in **Shaders**. Compile and edit its Values to supply a visible
   input; plasma intensity above `1` is useful for HDR bloom. These actions can
   prompt about unsaved document changes. Return through the **Rendering** tab.
3. Inspect the ordered passes below. **Capture** toggles intermediate capture;
   **Inspect** selects a real output in the drawer. Try bypassing blur/composite
   to see the effect, then re-enable them.

   ```text
   external -> 10 extract -> 20 horizontal blur -> 30 vertical blur
   external + 30 -> 40 composite -> 50 exposure/tonemap
   ```

4. **Shader:** cycles loaded compatible programs. **New:** chooses the program
   for **Add pass**. **In0/In1** cycle external input (`-1`) and earlier pass IDs;
   `-2` means absent second input. Every enabled pass requires an active scalar
   `inputTexture`; two-input programs also require active scalar `inputTexture1`.
   The host supplies/clears these bindings. A supplied second input on a one-input
   shader, missing required input, feedback or forward dependency is rejected.
5. **Div /** cycles 1, 2, 4, 8, 16 and selects relative sizing. **Size** cycles
   fixed 128, 256, 512 and relative (`0`); formats switch RGBA8/RGBA32F. **Bypass**
   aliases input0. **Up/Down/Remove** validate dependencies: reroute consumers
   before removing their producer or moving it after them. Failed edits retain
   the previous descriptors. Gallery blur pitch follows the actual input extent.
6. Save the editable shader first, then use Rendering **Save**. This writes the
   plain pipeline and authoritative `.vpipeline.host` shader-path ledger. Reopen
   loads/reuses pass programs and remaps IDs even if scene materials occupy slots.

For a custom pass, use an empty Vertex stage and a fragment shader such as:

```glsl
#version 330
in vec2 fragTexCoord;
out vec4 finalColor;
uniform sampler2D inputTexture;
uniform float gain;
void main() {
    vec4 source = texture(inputTexture, fragTexCoord);
    finalColor = vec4(source.rgb * gain, source.a);
}
```

Compile, expose `gain` if it has no schema, edit its Values, and Save. In Rendering,
select **Editable #1** with New/Shader and wire its input.
Numeric controls belong in the shader schema; pipeline-controlled input samplers
do not need exposure. Editable #1 follows the current Studio document; preserve
separate pass resources through saved shaders and the host pipeline ledger.
Gallery loading is transactional: insufficient capacity or compilation/validation
failure preserves the current pipeline. Five new bloom programs need five free
slots in the eight-slot shared scene/pass bank.

## Graph, diagnostics and comparison

- **Graph:** add constants, exposed vec4 parameters, arithmetic/mix or UV nodes.
  Drag titles to move; click output then input to connect; wheel over constants
  to edit; middle-drag pans. **Generate GLSL** validates types/cycles and replaces
  fragment source, starting fresh source history. Compile afterward. **Save graph**
  persists graph data. GLSL edits are independent; there is no GLSL-to-graph import.
- **Diagnostics:** root rows/gutter markers jump to Fragment or Vertex. Include
  errors open a cached read-only **Source view** with Previous/Next context;
  graph errors select the stable node. **Open externally** reports status when
  no OS launcher is configured. Failed builds retain the last successful program
  for preview; Apply requires a current successful compile, including dependencies.
- **Reference:** hold the camera/values fixed, choose a resolution, and **Freeze
  time** before **Capture reference**. Step advances the automatic clock by 1/60 s.
  Compare Live / Reference / Difference x4; **Measure difference** reports peak
  absolute and RMS error across RGBA. Click the image for top-left-origin linear
  float RGBA. Use gamma `1` for linear gallery comparisons. Changing output size
  invalidates the reference. Readback happens on request, not every preview frame.
- **Editing/layout:** source supports UTF-8 selection, clipboard, indentation,
  Ctrl+F/F3 find and Ctrl+Z/Y history. Values have separate Undo value/Redo value
  and per-parameter reset. Drag splitters; F11 maximizes preview. DPI-aware layout
  persists on tab changes, splitter release and shutdown.

## Save, reopen and immutable assignments

The editable project shader state is user-editable. **Save** in Shaders commits
the `.vshader` and corresponding `.vmaterial` checkpoint plus layout. Graph and
Rendering have individual saves; the host's combined Save/close path also saves
graph and pipeline. **Save As** changes editable shader identity only after the
source/material transaction succeeds and pins prior relative include origins.
**Reload** restores the current disk checkpoint, with explicit Discard / Reload
confirmation for local changes. Host **Reset** restores saved source, schema,
material values, bindings and resource table; it differs from a single parameter's
reset to defaults. Save failures preserve dirty state and pending actions.

| Resource | Persistence contract |
| --- | --- |
| `.vshader` | `VMGE_SHADER|1`: source, stage origins and stable typed schema/values |
| Editable `.vmaterial` | `VMGE_STUDIO_MATERIAL|1`: authoritative atomic completed source/material checkpoint, embedding shader text, `VMGE_MATERIAL|1` overrides and texture resource paths |
| Material-bearing `.vscene` | `VMGE_SCENE|3`: node shader references, exact instance/orphan values and texture R table; ordinary scenes remain v1, world scenes v2 |
| `.vgraph`, `.vpipeline` | Distinct version-1 graph/pass resources |
| `.vpipeline.host` | `VMGE_RENDER_PIPELINE|1`: authoritative host reopen checkpoint with descriptors and shader-path ledger |

Text fields use UTF-8 hex; writers preserve Float64 values using exact binary
rational encoding. Native tokens/session IDs never persist. The host checkpoint
loader reads `.vmaterial`; it is not interchangeable with standalone
`ss_material_load`. If a source write succeeds but its material commit fails,
reopen uses the older complete checkpoint rather than mixing snapshots.

Apply publishes immutable `shaders/studio/host-<content-hash>.vshader` resources
before assigning a node. Include-backed publications freeze expanded GLSL;
editing includes later cannot change saved scenes or undo. Opening a gallery or
publication loads a separate editable project copy (default `shaders/studio.vshader`,
or the configured/Save As path). Saving that copy never overwrites the publication.
Save the scene after Apply and keep its referenced resources with the project.

## Current bounds and evaluation scope

| Area | Hosted limit |
| --- | --- |
| Mesh binding | Material slot **0**; no per-slot authoring UI |
| Shared shader bank | **8** cached scene/pass programs, plus editable #1 and identity #2; no eviction of programs used by nodes/history; restart for a fresh bank |
| Per-node material | **16 total parameter/orphan rows**, 64 components per row; 16 stored sampler bindings |
| Native sampler binding | **4** managed scalar sampler2D uniforms per program, regardless of stored binding capacity |
| Shader / graph | 64 KiB per source stage; 64 schema parameters; first 64 unexposed reflection rows; 64 stable graph nodes |
| Source / value history | **17 source snapshots per stage** (16 undo steps); **8** selective numeric transactions |
| Rendering / reference | 16 passes; 512 per target axis; 64 MiB aggregate color+depth targets; 512×512 RGBA reference bank (8 MiB Float64) |
| Texture cache | 64 resources/tokens, 4096 per axis, 128 MiB estimated RGBA8 uploads |
| Preframe editor owners | Enforced **128 MiB** main-arena delta (134,217,728 bytes); separate from GPU/asset budgets |

The recorded portable authoring fixture measured **133,974,016 bytes**
(127.768 MiB) preframe. This is a fixture measurement, not an exact allocation
promise for every startup path; current path/header bookkeeping can add a small
amount, and the enforced 128 MiB bound remains authoritative. The poisoned host
fixtures are bounded to **32 frames** and require `commit_drift=0`, `errors=0`,
and `live_handles=0` on completion. Their setup/markers are in
[SHADER_HOST.md](SHADER_HOST.md#verification); the integrated project runner
includes shader model/UI/gallery and host authoring checks.

Developer evaluation is available in [`src/shader/evaluation.vy`](src/shader/evaluation.vy)
and exercised by [`tests/shader_evaluation.vy`](tests/shader_evaluation.vy).
Evaluation buffers are allocated preframe, up to 256×256 RGBA.
`ss_evaluation_freeze` snapshots source/schema/material and camera/time/resolution;
the synchronous objective must render that same frozen context. Steps select up
to 16 fit-marked scalar continuous parameters, estimate central finite differences
(one-sided at bounds), use actual `voptim.sgd_step`, and clamp to ranges.
Nonfinite/throwing steps roll back; authored values are not mutated. GPU checkpoint
resume requires recompilation, texture resolution and matching-resource rebind.
This is an **API/test workflow**, not a GUI optimizer. There is **no GPU autodiff
or arbitrary GLSL differentiation**. Typed graphs and frozen evaluation provide
a basis for future differentiation work without changing today's contract.

## Developer references

- [UI, authoring APIs and input ownership](SHADER_UI.md)
- [Shared host, publication/checkpoints and verification](SHADER_HOST.md)
- [Document, material, graph, pipeline and evaluation APIs](SHADER_MODEL.md)
- [Gallery resources and standalone rendering examples](shaders/studio/GALLERY.md)
- [Managed graphics runtime contract](../../../vyne/modules/external/vraylib/SHADER_STUDIO.md)

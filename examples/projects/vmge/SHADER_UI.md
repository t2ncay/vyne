# VMGE Shader Studio UI

The studio is a code-first native workspace: fragment/vertex source on the left,
a material preview in the center, typed parameters or preview controls on the
right, and a resizable diagnostics drawer below. Inter is used for navigation;
the source, gutter, values, and build output use authentic static JetBrains Mono
2.304. Its provenance and OFL license are bundled in `assets/fonts/`.

## Host integration

The main application already hosts this workspace through
`src/editor/workspace_host.vy` and `src/editor/scene_editor.vy`. Launch with
`VYNE_VMGE_MODE=shader` or `rendering`, or switch through the shared navigation.
See [Shader Studio](SHADER_STUDIO.md) for launch and authoring workflows and
[SHADER_HOST.md](SHADER_HOST.md) for the implemented host lifecycle.

For another host, include `src/editor/shader_workspace.vy` and create exactly once
**before the first frame**, inside its long-lived session region:

```vy
shader_ui :: SSWorkspace := ss_workspace_create(app,project);
```

The workspace uses the existing App. It never opens or closes a window, changes
mouse capture, or calls `app.frame`, `app.begin/end`, or `app.close`. Font IDs,
cameras, three preview meshes, targets, and programs belong to the shared App.
Source buffers, history, diagnostics, reference samples, and control state are
preallocated. Keep the workspace's region alive until the shared App closes.

Exact hosted API:

| API | Timing / purpose |
| --- | --- |
| `ss_workspace_create(app,project) -> SSWorkspace` | Preframe allocation, optional resource load, initial compile |
| `ss_workspace_frame(app,workspace,bounds,dt) -> Bool` | Once inside an active `app.frame`, **before** `app.begin`; updates input and renders offscreen preview |
| `ss_workspace_update(app,workspace,bounds,dt) -> Bool` | Input/time/layout half of `frame` |
| `ss_workspace_preview(app,workspace) -> Bool` | Offscreen render half of `frame`; requires an active frame |
| `ss_workspace_draw(app,workspace,bounds) -> Bool` | Inside the host's drawing surface; draws source, preview, inspector, drawer, actions and status |
| `ss_workspace_navigation(workspace.layout,bounds) -> Int64` | Inside host drawing surface; draw **once** for all workspaces |
| `ss_workspace_selected(workspace,model) -> Bool` | Supplies a borrowed model from the same session for the Selected model preview |
| `ss_workspace_apply(workspace) -> Bool` | Applies current compiled parameters and raises a host action edge |
| `ss_workspace_take_apply(workspace) -> Bool` | Consume that edge once; host records scene material assignment using `workspace.document` and `workspace.material` |
| `ss_workspace_save_layout(workspace) -> Bool` | Flush pending layout on a host tab switch or host shutdown |
| `ss_workspace_sync_source(workspace) -> Bool` | Synchronize editor caches/history after a host load or source replacement |
| `ss_workspace_parameter_set(workspace,id,component,value) -> Bool` | Atomic document/material edit through the engine's typed compatible migration API |
| `ss_workspace_capture_reference(workspace,output) -> Bool` | Exact GPU copy and retained CPU reference samples; requires an active frame |
| `ss_workspace_take_diagnostic_open(workspace,path_buffer) -> Bool` | Optional external-editor request from the drawer's Open externally button; line/column remain in `state[48]` / `state[49]` |
| `ss_workspace_commit_field(workspace) -> Bool` | Commit the active value, schema-draft or Save As field; shared host field dispatcher |
| `ss_workspace_input_captured(workspace) -> Bool` | Whole-frame capture, including a field that just closed, schema drafts, and document dialogs |
| `ss_workspace_claim_input(workspace) -> Bool` | Host input owner claims the frame after polling a field; next workspace update skips duplicate polling |
| `ss_workspace_expose_begin(workspace,reflection_row) -> Bool` | Open a typed draft for an unrepresented reflected uniform |
| `ss_workspace_schema_begin(workspace,param_id) -> Bool` | Edit metadata/adopt the current reflected shape for an existing stable ID |
| `ss_workspace_schema_apply(app,workspace) -> Bool` | Validate/publish the draft atomically, then compile its bindings |
| `ss_workspace_parameter_reset(workspace,param_id) -> Bool` | Reset one parameter to defaults as one numeric undo transaction |
| `ss_workspace_parameter_undo(workspace,redo) -> Bool` | Undo/redo one selective parameter transaction |
| `ss_workspace_save_as_begin(workspace) -> Bool` | Open the path dialog; Ctrl+Shift+S |
| `ss_workspace_save_as_path(workspace) -> String` | Transient getter for the staged host Save As destination |
| `ss_workspace_save_as_complete(workspace,success) -> Bool` | Host acknowledges its completed source/material checkpoint transaction |
| `ss_workspace_reload(workspace,discard_local) -> Bool` | Explicit Reload, with local-conflict confirmation or a hosted request |
| `ss_workspace_reload_complete(workspace,success) -> Bool` | Host acknowledges source/material restoration |
| `ss_workspace_include_view(workspace,path,line,column) -> Bool` | Read requested include context into existing owned diagnostic slots |

`bounds` is the full physical UI rectangle including the shared navigation row.
Region IDs in `ss_layout_region(layout,bounds,id)` are navigation (0), actions (1),
source (2), preview (3), inspector (4), drawer (5), status (6), and three splitters
(7–9). The host can use those same regions for Scene, World and Rendering panels.
Navigation IDs are **Scene 0 / World 1 / Shaders 2 / Rendering 3**.

Minimal custom-host loop ordering (the integrated host also gates modals,
dispatches Rendering and handles document transactions):

```vy
while app.frame() {
    bounds :: vraylib.Rectangle := se_rect(0.0,0.0,float64(app.width()),float64(app.height()));
    if shader_ui.layout.state[0] == 2 {
        ss_workspace_frame(app,shader_ui,bounds,vraylib.get_frame_time());
    }
    app.begin();
    active :: Int64 := ss_workspace_navigation(shader_ui.layout,bounds);
    if active == 2 { ss_workspace_draw(app,shader_ui,bounds); }
    # Host dispatches Scene / World / Rendering here.
    app.end();
    if ss_workspace_take_apply(shader_ui) {
        # Host assigns stable document/material IDs to its selected mesh and
        # records scene history. Render via ss_material_draw / draw_material.
    }
}
```

Dispatch input only to the active workspace and only when the host has no modal
or captured scene tool. A navigation click takes effect for input/offscreen
rendering on the next frame. Host scene rendering must use per-draw material
overrides rather than permanently attaching the replaceable shader to a model.
The workspace preserves the document's applied markers around preview draws.

### Hosted document events and input

Set `workspace.state[42] = 1` for hosted authoring. The UI then raises document
action edges rather than writing source/checkpoints itself:

| State | Meaning / host handling |
| --- | --- |
| `43` | Save editable source/material checkpoint; clear after handling |
| `46` | Reset to host checkpoint; call `ss_workspace_reset_complete` after restoration |
| `47` | **Save As**, replacing the former required include-open edge; staged destination is `ss_workspace_save_as_path` |
| `53` | Reload source/material checkpoint; `panel.state[3]` is the requested discard flag |
| `55` | Captured for the entire current frame; suppress host navigation/global shortcuts |

For Save As, copy the destination into the host's existing pending path owner.
Stage/guard the new editable identity, preserve empty include origins using the
previous source path, and write the corresponding source/material checkpoint
sidecar transaction before publishing `document.path`, host editable path,
checkpoint state or sidecar identity. A failed transaction keeps the old identity
and calls `ss_workspace_save_as_complete(workspace,false)` to reopen the path
field. Success calls the same helper with true. The pending modal keeps its path
stable until that acknowledgment. An existing target requires explicit Replace
confirmation before the hosted edge is raised.

Reload is a distinct event: restore the host's saved source, schema, material
overrides and texture resource references, then acknowledge through
`ss_workspace_reload_complete`. Source publication remains owned by the host.
Standalone Save As/Reload use bounded candidate staging and engine document APIs;
their failure leaves the source identity unchanged. Save As pins the original
relative include roots before changing filenames.

The host's pending-field handler must use `ss_workspace_commit_field`, rather
than treating every field as a numeric parameter. Field IDs are:

- `0`: exact parameter component;
- `100` / `101`: draft minimum / maximum;
- `102` / `103`: draft group / units;
- `200..263`: draft default components;
- `900`: Save As path.

The workspace normally polls its field once in `ss_workspace_update`. If a host
polls that field itself, it should dispatch the result with the shared helper and
call `ss_workspace_claim_input` before the workspace update. Capture remains
latched through Enter/Escape and modal cancellation. Hosts should gate navigation,
shortcuts and close/open dispatch with `ss_workspace_input_captured`, including
the interval when a schema draft has no text field focused.

## Source editor

`ss_code_editor(source) -> SSCodeEditor` independently accepts an existing
`Array<Int64>` in `[length, UTF8 bytes...]` format. It owns no shader/document.
Call `ss_code_input(editor,rect)` and notify the owning document with
`ss_document_source_change(doc)` when it returns true. The workspace does this
automatically for both stages. External document revisions are checked against
retained source hashes; parameter edits preserve source undo history.

- Mouse selection and dragging, UTF-8-safe arrows, Home/End, Ctrl+Home/End,
  Up/Down with preferred visual column, Page Up/Down, and Shift selection.
- Ctrl+A/C/X/V; clipboard validation reads original `String.byte_at` bytes before
  staging, so malformed UTF-8 is rejected rather than silently replaced by a
  decoder. CRLF and lone CR normalize explicitly to LF. Stored-buffer edits
  reject embedded NUL, partial-codepoint bounds and capacity overflow atomically.
  The native clipboard returns a C string: a NUL terminator and any hidden bytes
  after it are not observable, so the clipboard boundary cannot promise detection
  of an embedded NUL in the original native payload.
- Enter copies leading indentation; Tab inserts four spaces; selection Tab and
  Shift+Tab indent/unindent complete selected lines.
- Ctrl+Z / Ctrl+Shift+Z / Ctrl+Y. A fixed bank holds 17 source snapshots, including
  caret/selection metadata. Branching truncates redo; full banks evict the oldest
  entry and increment `editor.state[10]`. No edit expands source/history storage.
  Indentation snapshots include the final multiline selection, so redo restores
  its anchor as well as its text. Enter preflights newline plus indentation and
  the replacement's final length before touching the staging buffer.
- Ctrl+F opens a retained UTF-8 find query; Enter/F3 selects the next exact match,
  wrapping once. Escape closes find. Wheel scrolls lines; Shift+wheel scrolls
  horizontally. Gutter markers and diagnostic rows jump to source locations.
- Line offsets and block-comment entry states are cached per source revision.
  Token spans are generated only for visible lines and cached by revision,
  scroll origin and visible-line count. Adjacent spans coalesce. Keywords,
  comments, numbers/strings and preprocessor lines have restrained syntax colors.
  Highlight overflow falls back to plain text and reports a status message.

Build-stage headings route vertex and fragment diagnostics to the matching editor
and gutter. Source ID is retained separately from stage, line and column. Only
root-source rows appear in the root gutter. Include rows retain copied paths from
the attempted build (including failed compilation). Clicking one opens the
workspace's read-only **Source view** drawer with its exact path and marked line.
The existing message bank caches up to sixteen nearby lines, each truncated on a
UTF-8 boundary. Files are checked against a 64 KiB limit before reading and copied
inside a bounded region only on open/page requests. Draws never reread the file.
Previous/Next request additional context; the external-editor event is only
raised by the explicit Open externally button. Unknown IDs and linker
messages remain explicit log notices rather than jumping the wrong root buffer.
Graph source IDs emitted by the engine for both declarations and expressions
select the stable node ID and pan the graph canvas to it.
Successful builds copy the engine's published include map; failed builds use
the same engine preprocessor to recover include IDs into UI-owned path slots.
No compiler/driver log String or returned preprocessing wrapper survives a frame.

`ss_code_replace`, `ss_code_insert`, `ss_code_indent`, `ss_code_undo`,
`ss_code_find`, `ss_code_jump`, `ss_code_paste` and `ss_code_enter` also work without input polling. Standalone editor
owners call `ss_code_reset_history` after loading a different source.

### History allocation measurement

For a 65,537-element source slot (65,536 payload bytes), the old flat stride
reserved 1,114,129 elements and rounded to 2,097,152 Int64 elements: **16 MiB per
editor**. History now stores payloads separately from lengths/carets: a
1,048,576-element bank for sixteen payload slots plus a 65,536-element final bank,
both exact powers of two. That is **8.5 MiB per editor**, or **17 MiB across both
stages**, saving **15 MiB** while retaining all sixteen undo steps.

The regression measured **12,193,552 allocated bytes** for the entire 64 KiB
editor constructor, including staging, line/block caches, token spans and
metadata. Circular logical-to-physical slot mapping avoids copying the whole
history bank when evicting. Tests exercise full-length payloads through forty
edits, all sixteen undo/redo steps, spill-slot rollover and redo branching.

## Shader and preview actions

**Draft** means source/schema differs from the published compile. **Compiled**
means a successful current program is available. **Applied** requires the
explicit Apply action and matching parameter revisions. **Failed** records the
last unsuccessful compile attempt; the last successful program remains usable
for preview. Compilation and saves use engine document APIs.

Ctrl+Enter compiles. In the integrated host, Ctrl+S/Save commits `.vshader`, its
authoritative `.vmaterial` source/material checkpoint and layout. Reset restores
the saved source, schema, material values, sampler bindings and resource table;
per-parameter reset restores defaults. Ctrl+Shift+S/Save As changes the editable
identity transactionally. Reload restores disk state with explicit local-discard
confirmation. Graph and Rendering have individual Save actions; combined host
Save/close also saves graph and pipeline. Failed saves retain dirty/pending state.

Standalone workspace Save writes source plus layout and Reset restores source
snapshots/defaults. A standalone source write may succeed while layout fails;
the combined result reports failure and retains pending layout. The integrated
host checks layout before committing source/material. Layout auto-retries are
spaced one second apart; only a successful write clears pending state. Explicit
Save/host flush can retry immediately.

Parameters show group, GL type, array length, reflection status and units.
Every array/vector/matrix component has a control. Left-drag edits within the
declared range; integer/bool types snap to integers. **Right-click** opens exact
numeric entry: Enter validates and commits through `ss_workspace_parameter_set`, Escape
cancels. Missing and incompatible uniforms remain visible. Samplers are resource
bindings rather than numeric drag controls; the host binds imported textures
through the material engine API.

All numeric controls use `ss_material_parameter_set`: the engine validates the
document value, stages migration by `(ID, GLtype, array length)`, and publishes
both the document value and compatible material override together. A failed
validation or exhausted orphan capacity changes neither value nor revision.
Exact fields retain their stable ID and captured type/array length; a shape
change while typing requires reopening the field rather than editing a stale row.
Untouched numeric fields preserve the original Float64, even though the runtime's
display formatter rounds to six significant digits.

### Expose and edit schema from ordinary GLSL

Write, for example, `uniform vec3 tint;` in a shader that uses it, then Compile.
The inspector's **Expose / Schema** view lists active reflected uniforms missing
from the resource schema. Built-in transforms, material colors and texture0..11
are filtered. Expose opens a preallocated draft with a stable fresh ID, reflected
GL type/cardinality, per-component defaults, minimum/maximum, group, units, and
continuous/fit flags. Parameter IDs also avoid retained material orphan IDs and
exposed graph parameter IDs.

Apply schema validates bounds, type/cardinality, flags, every current/default
component and compatible retained override in bounded candidates before publishing
either the document schema or material. It then compiles the new bindings, so the
new uniform is immediately editable in Values and influences actual pixels.
Failures leave schema, values and published program unchanged. The Schema action
on an existing parameter permits metadata edits and adoption of its current
reflected type/cardinality. Unsupported uploads, block/negative locations,
invalid names, oversized component arrays and sampler arrays are labeled
explicitly. Scalar sampler metadata can be exposed; its live texture binding is
provided by the host material/resource panel.

The reflected-uniform cache holds the first 64 unrepresented rows and reports
overflow. Optimized-out uniforms cannot be discovered through active reflection;
existing resource metadata remains visible. Exposed `time` or `resolution`
parameters become manually controlled rather than being overwritten by the
automatic preview context.

Value edits have a separate eight-transaction numeric journal. Each transaction
stores only one stable parameter's before/after document and material values, up
to 64 components. One drag stroke and one parameter reset each produce one undo
entry; exact entries are grouped at commit. Schema application, document reload
and checkpoint reset clear this value journal. Source history remains separate.
Undo/Redo value buttons and Ctrl+Z/Y when the source is unfocused expose the
journal. Delayed tooltips explain compile, exposure, schema, document and capture
actions without retaining tooltip Strings.

### Additional retained storage

`SSShaderPanel` adds **91,088 measured allocated bytes** (about 88.95 KiB), below
the 128 KiB allowance. Its largest owners are a 64-row UTF-8 reflection-name bank
and eight selective numeric transactions. It retains one small schema draft.
The existing parameter SEUI's idle preview/name buffers are acquired as long path
text/staging aliases before frames; there is no additional SEUI clone. Include
excerpts reuse the existing diagnostics message bank. Tooltip state is numeric.
The integrated portable authoring fixture records **133,974,016 preframe bytes
(127.768 MiB)** for the whole editor owner delta, within its enforced 128 MiB
bound. This is a recorded fixture measurement; startup paths and small bookkeeping
additions can change it. See [host limits](SHADER_HOST.md#explicit-ownership-limits).

Preview controls select Plane, Cube, Sphere, or a borrowed selected model.
Right-drag orbits, wheel zooms. Freeze holds time, Step advances exactly 1/60 s,
and resolution presets are 128, 256, 512. Reflected `time:float` and
`resolution:vec2` receive preview context updates. Draft/failed source never
silently replaces the published program.

Capture reference uses `RenderTarget.copy_from` to copy the current output into a GPU target and preallocated
CPU float samples. Live / Reference / Difference x4 switches presentation;
Measure difference reports peak absolute and RMS error over RGBA components.
Click the image to inspect top-left-origin linear float RGBA values. Captures
preserve the actual pipeline output dimensions; a resolution change invalidates
the reference. The reference bank has 512×512×4 components; larger explicit
pipeline outputs produce an explicit capture-capacity error. Pixel readback is
only requested by capture/measurement/click, not on every preview frame.
The copy does not alpha-blend, clamp signed/HDR float channels, or apply a display
shader. Regressions compare every component of nonopaque and spatially asymmetric
signed/HDR fixtures in both the GPU reference target and the retained CPU bank.

## Graph and pipeline

Graph uses stable node/socket IDs and engine validation/generation. Add color
constants, exposed vec4 parameters, arithmetic/mix nodes or UV nodes; drag node
titles, click an output then an input to connect, wheel over constant components
to edit. Middle-drag pans a larger graph. Generate GLSL validates types/cycles,
publishes into the same document source buffer and starts a fresh source history.
Generation failures retain the authored source and copy the engine log.

The integrated **Rendering** workspace uses ordered engine passes with dependency
validation. Its controls select programs, wire external/prior-pass inputs, change
divisor/format/fixed extent, enable/bypass, capture/inspect, and reorder/remove.
**Load bloom gallery** loads five real programs and the shipped bloom DAG through
the host's transactional loader. **Open plasma seed** / **Open lit material**
replace the editable preview document. The external pipeline input is the Studio
preview; its outputs use actual engine targets.

The shared registry is ten entries: eight cached scene/pass documents (IDs 3..10),
editable 1 and identity 2, with no eviction. New/Shader cycling selects compiled
programs with active scalar `inputTexture`; a two-input shader additionally
requires `inputTexture1`. Invalid input bindings or dependency-breaking edits
retain the old descriptors. Rendering Save persists `.vpipeline` and the
authoritative `.vpipeline.host` path ledger. A standalone workspace initially has
only editable 1 and identity 2; a custom host can register preframe documents.

## Layout and theme

`SSLayout` retains workspace/source/inspector/drawer tabs, split fraction,
logical inspector width and drawer height, DPI scale, and preview maximization.
Three five-logical-pixel splitters are draggable. F11 toggles preview maximization
without changing the normal splits. `VMGE_LAYOUT|1` decode validates a candidate
before publication; saves are atomic. Layout is loaded/saved at
`shaders/studio.layout` by default (`VYNE_VMGE_LAYOUT` overrides it), and dirty
split/tab changes flush after drag release. The integrated host also flushes on
navigation changes and shutdown through `ss_workspace_save_layout`.

Initial DPI comes from the native window when no saved layout exists. Set
`layout.values[3]` to the host's current effective DPI/user scale (0.75–3.0) when
moving between monitors. Sizes are saved as logical units. `ss_scale`,
`ss_font_size`, `ss_line_height`, `ss_cell_width`, and `ss_color` are the new shared
tokens; `ss_theme_dpi` updates their effective scale. Existing scene helper names,
resource tokens and six legacy `se_theme_metrics` slots retain their ABI.

## Verification

Run from the repository root; all executables use unique shader UI output names:

```powershell
.\vynec.exe --compile -o build/shader-ui-dev-layout examples/projects/vmge/tests/layout.vy
.\build\shader-ui-dev-layout.exe
.\vynec.exe --compile -o build/shader-ui-dev-editor examples/projects/vmge/tests/shader_ui.vy
$env:VYNE_POISON_ARENA="1"
.\build\shader-ui-dev-editor.exe
.\vynec.exe --compile -o build/shader-ui-dev-workspace examples/projects/vmge/tests/shader_ui_workspace.vy
.\build\shader-ui-dev-workspace.exe
.\vynec.exe --compile -o build/shader-ui-dev-schema examples/projects/vmge/tests/shader_ui_schema.vy
.\build\shader-ui-dev-schema.exe
```

Markers: `VMGE_LAYOUT_OK`, `VMGE_SHADER_UI_OK`, `VMGE_SHADER_UI_WORKSPACE_OK`.
The workspace check also writes `build/shader-ui-dev-workspace.png` and
`build/shader-ui-dev-graph.png` for visual inspection.
Checks cover layout containment/roundtrip/atomic rejection, true mono metrics,
Unicode selection/history, indentation/find, comment-state highlighting,
explicit capacity/history limits, compiler diagnostic formats, publication
states and failure recovery, real changed-parameter pixel differences, pipeline
output, resize reference invalidation, balanced passes, zero retained frame
allocations and zero handles after shared App shutdown.
Review regressions additionally cover original malformed clipboard bytes,
preflight-before-staging Enter overflow, indentation redo selection, exact
full-length history rollover, typed shape migration and capacity failures,
include/graph diagnostic routing, layout-save failure/retry, and fractional-alpha
signed/HDR reference copies. Review executables use `shader-ui-dev-*-review` names.
`VMGE_SHADER_UI_SCHEMA_OK` verifies the ordinary-GLSL-uniform workflow through
exposure/editing/real pixels, invalid metadata atomicity, value stroke/reset undo,
Save As failure and hosted hooks, Reload conflict/discard, actual cached include
context, active inspector-clip schema application, cross-frame draft/path forms,
and the additional-owner budget. It runs with poisoned arenas and checks zero
retained frame allocations and zero handles after shared session shutdown.
Visual artifacts are `build/shader-ui-dev-schema-form.png` and
`build/shader-ui-dev-save-as.png`.

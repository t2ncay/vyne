# Shader Studio model and API contract

The model APIs below are used by the integrated main Scene/World/Shaders/Rendering
host. For launch, ordinary GLSL uniform exposure, gallery Apply and pass editing,
see [Shader Studio](SHADER_STUDIO.md); host lifecycle/persistence is in
[SHADER_HOST.md](SHADER_HOST.md), and authoring/input APIs in [SHADER_UI.md](SHADER_UI.md).

## Runtime and host contracts

`App.can_release(tokens::Array<Int64>) -> Bool`
checks the entire compact live-token bank without mutation/allocation, including
active-pass and external dependency blockers. Zero/stale/foreign tokens are
invalid; `ss_pipeline_compact_bank(tokens)` filters zero storage slots.
`App.release_bank(tokens) -> Bool` rechecks and releases the whole bank atomically;
blocked/invalid banks throw before mutation. Native release staging allocation
also precedes mutation, so an allocation failure preserves the entire bank.
The pipeline preflights the old bank before allocating and uses release_bank
for both replacement and candidate cleanup. Cross-session old banks are checked
through their original App session.

Actual fullscreen contract: `destination.draw_from(source,shader) -> Bool`
owns the immediate replacement draw, requires an active application frame and
no enclosing render passes, and replaces RGBA without SRC_ALPHA reblending.
It preserves top-left target orientation and restores GL state. Pipeline passes
use this operation directly, without target/shader begin/end. `copy_from(source)`
provides same-size replacement copies; pipeline effects use `draw_from`.

`ss_material_sized(document_id,instance_id,row_capacity,binding_capacity)`
supports 1..128 rows and 0..64 bindings, including the host's 16/16 bank;
`ss_material` retains its 128/64 defaults. `ss_material_copy(to,from)` validates
destination capacity against actual instance counts before mutation and copies
only live rows/bindings, clearing unused destination storage. Resolve last-good
overrides through `ss_material_compatible_slot(m,id,gltype,arraylen)`, not the
currently active draft ID alone. `ss_material_parameter_set(doc,m,id,component,value)`
validates the schema/value and stages migration before atomically publishing
the coupled document/material edit. The integrated UI/host uses this helper for edits.

Entry points live in `src/shader/`. Allocate all constructors before the managed
frame loop. Strings returned by queries are transient; retained text is always
`Array<Int64>` in the `se_text_*` format `[UTF8 byte length, bytes...]`.

## Document (include `document.vy`; GPU operations include `compile.vy`)

`ss_document() -> SSDocument`. Public fields: `source` (fragment, 64 KiB),
`vertex` (64 KiB), `path`, `fragment_path`, `vertex_path` (2048 bytes each),
`state`, `parameters`, `log`. `ss_document_source_change(doc)` must follow
source/vertex/editor buffer changes. `ss_document_load(doc,path) -> Bool`,
`ss_document_save(doc,path) -> Bool`, `ss_document_reload(doc,discard_local) -> Bool`.
Load validates in candidate staging before copying into retained storage.
Save uses `vfs.write_atomic`. `.vshader` is versioned textual, with UTF8 hex fields.

`state[0..]`: edit revision, disk revision, compiled revision, applied revision,
program token, application session, external conflict, disk content hash,
parameter revision, applied parameter revision. Zero program means no successful
build yet. `state[10]` is include dependency count, `state[11]` is source/schema
build revision; compiled revision tracks this counter, while parameter value edits
advance edit/parameter revisions without requiring recompilation.
`ss_document_dirty`, `ss_document_compile_dirty`, `ss_document_applied`
are the UI queries. `ss_compile(app,doc) -> Bool` always builds uncached,
reflects/validates before publication; failure retains the previous program.
`ss_program(doc) -> vraylib.Shader` returns a transient token wrapper.
`ss_document_close(doc)` releases the published program. Do not attach this
program persistently to models: use per-draw material calls so old programs can
be released without model dependencies.

If an integration still holds model dependencies, release is deferred into a
fixed 16-token queue (`state[12]` count). `ss_document_collect(doc) -> Bool`
retries releases after dependencies detach; true means drained. Builds stop
before staging if the queue is full. Tokens are never discarded on a blocked
release. Closing a document also retires a blocked program; collect again after
detaching references, or close the owning application session.

`ss_parameter_define(doc,id,name,gltype,arraylen,default_value,min,max,group,units,continuous,fit)`
defines metadata (defaults initially broadcast to all components).
`ss_parameter_set(doc,id,component,value)`, `ss_parameter_get(doc,id,component)`;
IDs are positive integers, not reflection positions. Metadata buffers and
current/default values are retained. Compatible reflected uniforms retain values;
missing/incompatible uniforms remain as orphans with diagnostics. Samplers are
resource bindings, not continuous fitting variables.

`SSParameters`: `state[0]` count (64 maximum); `meta` stride 8 = ID, GLtype,
array length, component count, continuous, fit, reflection status, location.
Status 0 uncompiled, 1 active, 2 missing, 3 incompatible. `values/defaults`
stride 64; `ranges` stride 2 = minimum/maximum. `names/groups/units` contain
retained text buffers. `doc.published` is the successful-build schema; loading
or editing a document cannot destroy the old program's parameter bindings.
Uniform arrays have at most 64 components per parameter. Runtime support for a
GLtype is checked by `set_typed`; the current runtime supports float/int/bool
scalar/vector and mat4 uploads. Other reflected matrix types retain metadata.

`ss_component_validate(gltype,value)` is shared by define/set/default/parse,
migration and material preflight. Boolean components are exactly 0/1; integers
are exact signed Int32 (-2147483648..2147483647); continuous components remain
finite within +/-1000000. Ranges use the same component validator.
`ss_parameter_validate(doc,id,component,value)` checks type and schema range;
`ss_parameter_default_set(doc,id,component,value)` validates each default.
`ss_parameters_validate(parameters)` verifies every current/default component.
`ss_parameter_edit_metadata(doc,id,name,defaults::Array<Float64>,low,high,group,units,continuous,fit)`
stages validated metadata/default edits with the existing stable ID/type/arraylen.
It preserves all current values and requires the new range to contain them.
Uniform-name changes increment the build revision; other metadata edits increment
only disk edit revision. Published last-good schema remains intact until compile.
Reflection rows with location -1 or scalar-length sampler names ending `[0]`
are incompatible, including uniform-block members and sampler arrays of length 1.
`state[13/14]` record successfully compiled root source/vertex hashes.

`ss_document_dependencies_changed(doc) -> Bool` polls successful-build include
dependencies and advances only the build revision on changes; external include
changes do not mark the root resource disk-dirty. `dependencies` holds
retained canonical paths; `dependency_hashes` holds observed content hashes.
Includes are quoted relative paths, at most 32 files and depth 16, source size
64 KiB per file and expanded total 256 KiB. Root `#version` must be first;
includes cannot supply a version. Numeric `#line` source IDs 1..32 map to
`dependencies[id-1]`; root uses ID 0. Paths are resolved against `fragment_path`
or `vertex_path`, falling back to the resource path. The buffers contain the
actual root source, not filenames to load implicitly.

## Materials (`material.vy`)

`ss_material(document_id,instance_id) -> MaterialInstance`; IDs persist, native
tokens do not. `ss_material_migrate(material,doc)` synchronizes compatible stable
IDs, preserving orphaned overrides. `ss_material_set/get(material,id,component,...)`.
`ss_material_apply(doc,material)` resets every reflected parameter for each draw.
`ss_material_draw(model,transform,doc,material,slot,tint)` uses shared geometry.
`ss_material_text/parse/save/load` serialize overrides and IDs independently of
scene persistence.

`ss_material_bind_texture(material,param_id,resource_id,texture)` resolves a
scalar sampler2D. `bindings` stride 4 = parameter ID, persistent resource ID,
native token, session; `binding_state[0]` is count. Serialization saves the first
two only. Reopen requires explicit texture resolution. Materials borrow textures;
close shader programs before releasing textures they have sampled. Every active
sampler must be resolved before material application, preventing inherited bindings.

Use `ss_material_rows(m)` / `ss_material_binding_capacity(m)` for capacities,
`state[2]` / `binding_state[0]` for live counts. `ss_material_copy(to,from)` is
available from `material.vy` (not evaluation-only) and copies across capacities
when all live rows/bindings fit. `ss_material_compatible_slot(m,id,gltype,arraylen)`
selects a published-schema orphan even when the draft has a different active type.
`ss_material_validate_for(doc,m,controlled)` preflights all components before any
uniform upload. `ss_material_parameter_set(doc,m,id,component,value)` is the
coupled edit entry point; validation/capacity failure leaves both stores/revisions
unchanged. Native texture bindings remain borrowed and are never serialized.

## Ordered multipass (`pipeline.vy`)

```text
ss_pipeline() -> SSPipeline
ss_pipeline_add(p,id,document_id,input0,input1,width,height,divisor,format) -> Int64 slot
ss_pipeline_enable(p,id,enabled,capture) -> Bool
ss_pipeline_validate(p) -> Bool
ss_pipeline_prepare(app,p,width,height) -> Bool  # true if rebuilt
ss_pipeline_run(app,p,documents::Array,document_ids::Array<Int64>,input_target) -> RenderTarget
ss_pipeline_capture(p,id) -> Array<Float64>
ss_pipeline_close(p) -> Bool
ss_pipeline_text(p) -> String
ss_pipeline_parse(p,text) / ss_pipeline_save(p,path) / ss_pipeline_load(p,path) -> Bool
```

16 ordered passes maximum. Pass IDs are output resource IDs; input0 `-1` is
external input; input1 `-2` is absent, `-1` external, otherwise a pass ID.
All dependencies must precede consumers. Missing inputs, cycles and write
feedback are rejected. A disabled pass aliases input0. Width/height zero means
base dimension divided by divisor (1..16), otherwise fixed, maximum 4096.
Formats 0 RGBA8 and 1 RGBA32F. Input samplers are `inputTexture` and
`inputTexture1`; pipeline owns and clears those transient bindings after each
pass. The pass shader's schema should contain its numeric parameters; these
two pipeline-controlled samplers do not need parameter metadata.
`documents`/`document_ids` are a preallocated parallel registry.
Preparation builds candidate targets before replacing old ones; target wrappers
returned by run/capture are transient. Capture buffers returned by runtime are
transient; copy data into retained evaluation storage when needed.
Preparation/close preflight the complete compact old bank; a blocker preserves
every old token/output. Candidates are guarded through allocation, atomic old
bank release and publication. Fresh unescaped candidates have an allocation-free
single-close fallback if native bank-release staging allocation fails.
All controlled sampler names/types/cardinalities/locations/descriptors and
material values are checked for every pass before the first draw. Every active
controlled input must be supplied, and every supplied controlled input must be
active. Both controlled bindings are cleared on all execution exits, including
validation errors. Outputs publish after completion. `draw_from` replacement
writes preserve nonopaque RGBA through identity passes.
`meta` stride 10 = id, document id, input0, input1, enabled, width, height,
divisor, format, capture. `state[0]` count; mutate through add/enable or advance
`state[1]` after direct descriptor edits, then validate/prepare.

## Typed graph (`graph.vy`)

```text
ss_graph() -> SSGraph
ss_graph_add(g,id,kind,type_id,name,parameter_id) -> Int64 slot
ss_graph_connect(g,to_id,input_socket,from_id) -> Bool
ss_graph_value(g,id,component,value) -> Bool
ss_graph_validate(g) -> Bool
ss_graph_glsl(g) -> String
ss_graph_generate(g,doc) -> Bool
ss_graph_text(g) -> String
ss_graph_parse(g,text) / ss_graph_save(g,path) / ss_graph_load(g,path) -> Bool
```

64 stable-ID nodes. Types: 1 float, 2 vec2, 3 vec3, 4 vec4, 5 sampler2D.
Kinds: 0 constant, 1 exposed parameter, 2 add, 3 multiply, 4 mix, 5 UV,
6 texture sample, 7 fragment output, 8 subtract, 9 divide. Input sockets
0/1/2 correspond to A/B/C; `-1` disconnects. Arithmetic allows scalar/vector
promotion. Mix requires matching A/B and scalar or matching C. Texture takes
sampler2D/vec2, yields vec4; UV yields vec2. Exactly one vec4 output is required.
Values stride 6: constant/default components 0..3, UI position x/y 4..5.
Meta stride 8: ID, kind, type, A ID, B ID, C ID, parameter ID, reserved.
Names are retained text. Exposed parameters use unique names and IDs; compatible
existing document schema is reused, incompatible schema diagnoses a conflict.
Generation validates and stages before publishing. `g.source`/`g.log` are retained
buffers; state 0 count, 1 edit revision, 2 generated revision. GLSL order is
deterministic by dependency and node ID, independent of UI insertion order.
`#line 1 <node-id>` maps compiler diagnostics directly to node IDs.
Exposed declarations receive their own node `#line`, too. Exposed names reject
GLSL keywords, generated `n_` locals, `gl_`/double-underscore reservations,
generated interface names and the built-ins used by generated expressions.
Graph resources reopen as graph data. GLSL edits are independent source mode;
there is no GLSL-to-graph conversion.

## Evaluation (`evaluation.vy`)

```text
ss_evaluation(width,height) -> SSEvaluation
ss_evaluation_freeze(e,doc,material,time,camera::Camera3D) -> Bool
ss_evaluation_camera(e) -> Camera3D
ss_evaluation_reference(e,pixels::Array<Float64>) -> Bool
ss_evaluation_compare(e,pixels::Array<Float64>) -> Float64 MSE
ss_evaluation_capture(e,target,reference::Bool) -> Float64 MSE
ss_evaluation_step(e,doc,epsilon,rate,objective) -> Float64 loss
ss_evaluation_rebind(e,doc) -> Bool
ss_evaluation_save(e,path) / ss_evaluation_load(e,path) -> Bool
```

Allocate matching reference/difference buffers preframe, maximum 256x256 RGBA.
Use `App.float_target` and linear shaders/linear reference pixels, without display
gamma transforms. Pixels are top-left; signed RGBA difference is retained.
`metrics[0/1]` are RGB MSE/MAE (alpha excluded), `[2/3]` last epsilon/rate.
`context`: time, camera position xyz, target xyz, up xyz, fovy, projection.
Resolution is `state[0/1]`; reference-ready `[2]`, fit step `[3]`, fit count `[4]`,
frozen `[5]`, source hash `[6]`, compiled revision `[7]`, vertex hash `[8]`.
`[9]` fingerprints the full shader schema/values and resolved include contents.
Freeze copies the material and full shader resource into retained storage;
fitting rejects changed source/schema/include resources.
The checkpoint resource includes effective absolute vertex/fragment stage paths,
so a document's `path` fallback remains resolvable after reopening elsewhere.
If a document has a GPU program/revision, freeze/rebind/step require a valid,
current compiled build and matching compiled root hashes; includes are polled
as part of this check. Uncompiled CPU objectives remain supported.

Objective is a synchronous callable `(MaterialInstance) -> Float64`. It must
render the same frozen camera/time/resolution/scene/resource inputs and return
finite linear loss. Render from the supplied MaterialInstance argument: it is a
bounded staged candidate; `e.material` remains the published state until commit.
GPU pipeline execution and GPU objective steps run inside an active `app.frame()`
with no enclosing camera/shader/target pass; callbacks balance their own passes.
Fitting selects at most 16 scalar continuous parameters explicitly marked fit;
uses central differences (one-sided at bounds), then actual `voptim.sgd_step`,
and clamps to schema ranges. A bounded staged material/optimizer and full
evaluation snapshot protect the entire step, including final objective evaluation.
Final throw/NaN/nonfinite metrics or differences restore material, optimizer,
step, context, reference, difference, gradients and checkpoint state exactly.
Only a validated finite final objective commits parameter/metric/step changes.
Authored document/material values are not changed. No arbitrary GLSL autodiff is claimed.
Checkpoints atomically save full source/schema, frozen camera/time, exact material
overrides/resource IDs, reference, difference, loss, selected IDs, gradients,
SGD parameters and step count. Native tokens and compiled revision do not persist.
CPU objective checkpoints resume directly; after GPU reopen parse `e.document`
into a document, compile it, resolve saved material texture resource IDs and call
`ss_evaluation_rebind(e,doc)` before GPU fitting. Rebind verifies matching source,
schema and external include content before accepting the new compiled revision.

## Resource encodings and checks

`.vshader`, `.vmaterial`, `.vpipeline`, `.vgraph`, `.vevaluation` have distinct
`VMGE_*|1` headers. Text fields are UTF8 hex. Float fields accept decimal and
exact binary rational `signed_integer_mantissa@binary_exponent`; writers use
the latter to retain Float64 values exactly. Parser entry points are intended
for bounded load operations; `*_load/save` provide their own bounded regions.
Constructors and long-lived registries must be outside frame regions.

The integrated `tests/run.py` includes `shader_model`, `shader_pipeline`,
`shader_graph`, `shader_evaluation` and `shader_gallery`, alongside UI/schema and
graphics host/authoring checks. Focused model fixtures use `build/shader-model-dev/`
outputs and separate `VMGE_SHADER_*_CPU_OK` markers; `VYNE_SHADER_GPU=1` enables
their hidden-window GPU checks and `VYNE_POISON_ARENA=1` poisons transient pointers.
GPU checks assert actual pixels and zero remaining resource handles. See the
[host verification reference](SHADER_HOST.md#verification) for bounded host flows.

Shipped samples: `shaders/studio/gain.vshader` with document ID 1 for
`gain.vpipeline`, and `color.vgraph` for constant/exposed-gain arithmetic.
`gain.fs`/`common.glsl` demonstrate include-aware source import.

The developer starter gallery is indexed by
[`shaders/studio/GALLERY.md`](shaders/studio/GALLERY.md): seven self-contained
`.vshader` resources, a custom-normal 3D material, periodic procedural plasma/grid,
exposure tonemap and an actual five-pass separable blur/bloom DAG. `gallery.vy`
exports `ss_gallery_load(root)`, `ss_gallery_document(g,id)`,
`ss_gallery_compile(app,g)`, `ss_gallery_prepare(app,g,w,h)` and
`ss_gallery_close(g)`. The explicit registry uses document IDs 101/102/201..205.
`ss_gallery_sources(root)` plus `ss_gallery_schema(doc,id)` are source/schema
authoring helpers. The standalone `tests/shader_gallery.vy` uses the same
`VYNE_SHADER_GPU`/arena-poison fixture convention and unique gallery markers.

The integrated workspace owns its editable document and uses explicit operations;
its host shares the fixed scene/pass registry. Custom callers likewise must not
retain temporary return strings or Shader wrappers in frame-created boxes.

The desktop runtime requires OpenGL 3.3 and main-thread operations. Host-specific
bounds are material slot 0, eight cached scene/pass programs plus editable/identity,
16 total parameter/orphan rows per node, and four native managed sampler bindings
per program (even with 16 stored host bindings). Owner storage is preallocated
under the enforced 128 MiB editor main-arena delta; engine API capacities above
are not all exposed at their maximum by the host.

Evaluation freeze/finite-difference/SGD is an API and test workflow, not a GUI
optimizer. Fit metadata marks eligible parameters; neither arbitrary GLSL nor GPU
autodiff is implemented. The typed graph and frozen context are useful foundations
for future differentiation, without promising it in the current studio.

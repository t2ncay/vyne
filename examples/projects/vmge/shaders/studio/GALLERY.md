# VMGE developer starter gallery

Open these **`.vshader` resources** from the asset browser's `shaders/studio/`
folder. Their fragment/vertex sources and editable parameter schemas are embedded;
they do not need external images or shader includes. `.fs`/`.vs` companions are
readable authoring copies. Acceptance tests compare their bytes and schemas with
the executable resources and save/reopen every source encoding.

All programs use GLSL **330**, scalar/vec3/vec4 controls and standard mat4 renderer
semantics. These small art-directed examples combine stylized diffuse shading
and a Fresnel-like rim controlled by artistic strength/power parameters.

## Resource registry

IDs are the explicit **`SSGallery` document registry IDs**, not native GPU tokens.
A host scene integration may assign its own resource IDs to these paths.

| ID | Resource | Use | Starting controls |
|---:|---|---|---|
| 101 | [`lit-material.vshader`](lit-material.vshader) | 3D meshes with normals; custom vertex stage | blue base / amber accents, contrast 1.4, stripe density 6, rim strength .28 |
| 102 | [`plasma-grid.vshader`](plasma-grid.vshader) | procedural 2D surface / fullscreen HDR seed | blue-to-orange palette, grid overlay .35, intensity 1.5, period 8 seconds |
| 201 | [`exposure-tonemap.vshader`](exposure-tonemap.vshader) | fullscreen color mapping | exposure 0 stops, white reference 1, saturation 1, gamma 1 |
| 202 | [`bloom-extract.vshader`](bloom-extract.vshader) | fullscreen bright extraction | threshold .6, gate width .3, gain 1 |
| 203 | [`blur-horizontal.vshader`](blur-horizontal.vshader) | horizontal normalized five-tap blur | texel step 1/16, radius 1 pixel |
| 204 | [`blur-vertical.vshader`](blur-vertical.vshader) | vertical normalized five-tap blur | texel step 1/16, radius 1 pixel |
| 205 | [`bloom-composite.vshader`](bloom-composite.vshader) | original + tinted blurred light | strength .35, tint (1,.9,.75) |

Each schema carries stable parameter IDs, per-component defaults, ranges, groups,
units and explicit continuous/fit flags. Every exposed parameter is actively
reflected by the compiled sample; renderer matrices/samplers are not saved tokens.

### Lit material controls (101)

- IDs **1/2**: `baseColor` / `accentColor`, linear RGB, each component 0..1.
- IDs **3/4/5**: `lightDirection` (world-space direction), `lightColor` (linear
  RGB 0..4), and `ambient` fraction 0..1.
- ID **6**: `contrast`, angular diffuse power .2..4.
- IDs **7/8**: `patternScale` (.25..32 cycles/model unit), `patternStrength` 0..1.
- IDs **9/10**: `fresnelStrength` 0..1 and `fresnelPower` 1..8 for the stylized rim.
- ID **11**: `cameraPosition`, world xyz. Update it from the actual camera.
- ID **12**: `normalView`, 0 for lit color, 1 for encoded world-space normals.

`vertexPosition` and `vertexNormal` are standard mesh attributes. The vertex uses
raylib's **`mvp`**, **`matModel`** and **`matNormal`** mat4 uniforms; the renderer
uploads the inverse-transpose world normal matrix per draw. These matrices are
renderer semantics, not editable parameter defaults. `colDiffuse` preserves the
supplied material tint/alpha. Use white tint when inspecting `normalView`.
Rotate the cube or use a normal-bearing curved mesh to see the angular/rim controls.

### Plasma/grid controls (102)

IDs **1/2/3** are low/high/grid RGB colors (0..4, so HDR palettes are possible).
IDs **4/5** are `time` / `period` in seconds; the shader uses `mod(time,period)`.
At the default period, times 0 and 8 render identical pixels. IDs **6/7** control
pattern scale and palette contrast; **8/9/10** control cell count, line width and
grid overlay. ID **11** is linear intensity (0..8).

Gamma **1** on the tonemap keeps captures linear. Set `displayGamma` (201, ID 4)
to 2.2 only for an intentional display conversion, after linear comparison work.
Exposure (201, ID 1) is in stops: +1 doubles the linear input before mapping.

## Load a single material

From the repository root, include `examples/projects/vmge/src/shader/material.vy`.
Allocate document/material/geometry/camera/target before the frame loop:

```vyne
doc :: SSDocument := ss_document();
ss_document_load(doc, "examples/projects/vmge/shaders/studio/lit-material.vshader");
if !ss_compile(app, doc) { throw se_text_read(doc.log); }
material :: MaterialInstance := ss_material_sized(101, 1, 16, 0);
ss_material_migrate(material, doc);
```

Inside a frame, keep the view uniform synchronized and draw in a target/camera
pass using existing managed geometry:

```vyne
view :: vraylib.Vector3 := camera.position();
ss_material_set(material, 11, 0, view.x);
ss_material_set(material, 11, 1, view.y);
ss_material_set(material, 11, 2, view.z);
target.begin(); app.clear(vraylib.BLACK); camera.begin();
ss_material_draw(model, transform, doc, material, 0, vraylib.WHITE);
camera.end(); target.end();
```

A fully native procedural geometry fixture needs no file texture:

```vyne
image :: vraylib.Image := app.image(1, 1, vraylib.WHITE);
white :: vraylib.Texture := app.texture(image); image.close();
model :: vraylib.Model := app.cube(white);
```

The mesh/program is shared; each `MaterialInstance` has independent overrides.
Close the model before its borrowed texture, then close the document/program.

## Actual five-pass bloom DAG

[`bloom.vpipeline`](bloom.vpipeline) references the registry above:

```text
external scene (linear RGBA32F)
  -> pass 10: document 202, bright extract
  -> pass 20: document 203, horizontal blur
  -> pass 30: document 204, vertical blur
external scene + pass 30
  -> pass 40: document 205, tinted additive composite
  -> pass 50: document 201, exposure/tonemap
```

All targets are relative full-resolution RGBA32F; intermediate capture is enabled.
Composite input0 is **external**, input1 is **pass 30**. This is an explicit DAG
over the same document registry; no hidden shader clones or native tokens persist.
The sampler bindings `inputTexture` / `inputTexture1` are owned by pipeline
execution and cleared afterward. Every pass uses replacement RGBA writes.

Include `src/shader/gallery.vy`. Preframe:

```vyne
gallery :: SSGallery := ss_gallery_load("examples/projects/vmge/shaders/studio");
ss_gallery_compile(app, gallery);
ss_gallery_prepare(app, gallery, 256, 256);
seed :: SSDocument := ss_gallery_document(gallery, 102);
seed_material :: MaterialInstance := ss_material_sized(102, 1, 16, 0);
ss_material_migrate(seed_material, seed);
dummy :: vraylib.RenderTarget := app.float_target(256, 256);
scene :: vraylib.RenderTarget := app.float_target(256, 256);
```

Inside each managed frame, with no enclosing target/camera/shader pass:

```vyne
seconds :: Float64 := app.elapsed();
period :: Float64 := ss_material_get(seed_material, 5, 0);
ss_material_set(seed_material, 4, 0, seconds-vmath.floor(seconds/period)*period);
dummy.begin(); app.clear(vraylib.BLACK); dummy.end();
ss_material_apply(seed, seed_material);
scene.draw_from(dummy, ss_program(seed));
result :: vraylib.RenderTarget := ss_pipeline_run(
    app, gallery.pipeline, gallery.documents, gallery.ids, scene);
app.begin(); result.present(float64(app.width()), float64(app.height())); app.end();
```

The dummy supplies a valid native fullscreen source/extent; plasma uses UVs only.
Alternatively render the 3D scene into `scene` before invoking the same pipeline.
Call `ss_gallery_prepare(app,gallery,width,height)` on resolution changes: it
validates horizontal `texelStep = 1/width` and vertical `texelStep = 1/height`,
prepares targets, then publishes both sampling pitches. A blocked old bank leaves
both pitches untouched. Standalone blur documents default to a 16x16 source and must receive
these updates for other resolutions. Their kernel is `[1,4,6,4,1]/16`.

Capture `ss_pipeline_capture(gallery.pipeline,30)` for blurred linear RGBA.
Returned pixels/wrappers are transient; copy reference data to preallocated
evaluation storage before retaining it. Close `scene`/`dummy` and
`ss_gallery_close(gallery)` after the frame loop, then the App.

## Metadata editing / authoring

Validated metadata editing retains the parameter's ID, GLtype, array length and
current values. It rejects ranges excluding any retained component and stages
all text/default changes before publishing:

```vyne
ss_parameter_edit_metadata(doc, 1, "baseColor", [0.2,0.4,0.6],
    0.0, 1.0, "Paint", "linear RGB", true, false);
```

The name is the actual uniform binding name. Renaming marks the source/schema
build dirty; update GLSL separately and compile explicitly. Group/default/range/
fit edits without renaming are disk edits and do not recompile the source.
Existing per-instance overrides are validated during explicit migration.
Fit flags select eligible small continuous parameters for finite differences;
this gallery does not convert GLSL into a graph or claim arbitrary shader AD.

`ss_gallery_sources(root)` reconstructs authoring documents from the readable
GLSL companions plus `ss_gallery_schema`. It writes nothing. Save explicitly with
`ss_document_save` after changing sources/schema. `.vshader` files are self-contained
versioned UTF8 resources and survive the repository's generated-asset ignore rules.

## Acceptance

```powershell
.\vynec.exe --compile -o "build/shader-model-dev/shader_gallery" "examples/projects/vmge/tests/shader_gallery.vy"
$env:VYNE_SHADER_GPU = "1"
$env:VYNE_POISON_ARENA = "1"
& "build/shader-model-dev/shader_gallery.exe"
```

The unique test emits `VMGE_SHADER_GALLERY_CPU_OK`, `VMGE_SHADER_GALLERY_GPU_OK`
and `VMGE_SHADER_GALLERY_OK`. It validates all source encodings/default schemas,
active reflection, lit CPU references, transformed native normals, exact periodic
plasma, HDR exposure, thresholding, separable blur impulse samples, bloom spread,
alpha replacement and zero remaining GPU handles. All fixture outputs stay under
`build/shader-model-dev/`. CPU-only mode needs no window; GPU mode uses a hidden one.

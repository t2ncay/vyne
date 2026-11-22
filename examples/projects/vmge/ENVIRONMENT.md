# Sky, lighting and player-camera effects

VMGE supports **Sky**, **Directional Light** and **Point Light** nodes, plus
custom post-processing on the active scene camera. Build the compiler from this
branch: the native backend now preserves logical short-circuit evaluation,
including guarded resource/visibility calls.

## Try it in the editor

From the repository root:

```powershell
xmake build vynec
$env:VYNE_VMGE_MODE = "scene"
$env:VYNE_VMGE_ASSETS = "procedural"
$env:VYNE_VMGE_SCENE = "$PWD/build/my-camera-world.vscene"
.\vynec.exe --run examples/projects/vmge/src/main.vy
```

An absent scene path starts the existing demo. Use a writable scene path and
Save As when editing bundled scenes.

1. Click **+ Add Node…** in the hierarchy, press **Shift+A**, or right-click the
   hierarchy to add a child. Search by name, category or description; `sun`
   finds Directional Light and `lamp` finds Point Light.
2. Choose **Daylight Scene**. It creates linked Sky and Sun nodes as one undoable
   action. The parent button switches between the captured selection and root.
   Up/Down selects results, Enter creates, Escape cancels. On short windows the
   list follows keyboard selection.
3. Select Sky to edit zenith/horizon RGB, brightness, sun angular size and energy,
   sun slot, and ambient illumination. Sky RGB is **0..1**, rather than 0..255.
4. Select Sun to edit rotation, RGBA light color, intensity and ambient. Its local
   **+Z axis points along light travel**; illumination arrives from the opposite
   direction. Parent transforms affect the light. The arrow shows light travel.
5. Add a Point Light for a local lamp. Set position, color, intensity and radius.
   Selecting it displays its radius sphere. Point-light attenuation is bounded
   to its radius; node scale does not change this world-space radius.
6. Select the demo Camera, click **Make active**, then **Use VHS preset**.
   **Preview camera** shows its authored view with effects. **Return to editor
   view** restores editor navigation. F6/Play uses the same capture/effect path.
7. **Edit pipeline / camera input** opens Rendering with the full scene camera
   as input. Its input button switches between player camera and material
   preview. Edit pass resources, reorder valid dependencies, bypass passes, and
   save the pipeline. **Reload camera effects** publishes the saved pipeline to
   camera Preview/Play. Failed loading/compilation retains the last working bank
   and reports the error in Output/console.
8. Save the `.vscene`. Active selections, environment properties, camera pipeline
   reference and effect-enabled state participate in persistence and undo/redo.

The Rendering workspace is an authoring preview; camera Preview/Play reloads
**saved** resources explicitly. Stop Play before modifying authored settings.
VHS has strength, grain, scanlines, wobble, separation and vignette controls.
Its second pass tonemaps HDR into display-range RGBA8. Effects bypass on the
Camera presents the raw scene; disabling every pipeline pass also returns the
exact raw target. A raw HDR image may exceed display range.

## Rendering and lighting

```text
Sky background + world/terrain + material-shaded meshes
  -> active camera RGBA32F target
  -> ordered custom passes
  -> final presentation
  -> editor UI / game HUD
```

Editor grid, selection outlines and gizmos are omitted from camera captures.
Effects run after camera/target passes close, before the presentation pass opens.
Unassigned meshes and world geometry use the bundled lit shader when lights are
visible. Existing scenes without lights retain the original unlit rendering.
Terrain and map cubes use derivative face normals oriented toward the camera;
mesh materials use transformed vertex normals.

One directional light is selected deterministically: the active Sky's visible
linked directional light, otherwise the first visible directional slot. Sky's
`Sun slot/-1` uses `-1` for automatic selection. One active Sky is explicit;
legacy camera selection falls back to the first camera slot. Visibility is
inherited through parents for Sky and lights.

Up to **four visible Point Lights** are gathered into a fixed preframe buffer.
A fifth produces a capacity diagnostic rather than silently truncating lights.
Intensity is 0..32; radius is 0.1..10000; ambient is 0..1. Ambient comes from
the active visible Sky, otherwise the selected Sun or first Point Light.

Custom materials opt into scene lighting by declaring active `float seAmbient`
and the relevant engine uniforms. See `shaders/environment/lit.vs` and `lit.fs`:
`seSunDirection`, `seSunColor`, `sePointCount`, `sePoint0..3` (XYZ/radius),
`sePointColor0..3`, `seCameraPosition`, and `seWorldMode`.
Engine lighting is uploaded after material values; keep these names reserved.
Custom shaders without this contract retain their authored behavior.

## Write a custom camera shader

Create a `.vshader` in the Shader workspace, with a fragment such as:

```glsl
#version 330
in vec2 fragTexCoord;
out vec4 finalColor;
uniform sampler2D inputTexture;
uniform float seTime;
uniform vec2 seInputResolution;
uniform float strength;
void main() {
    vec4 source = texture(inputTexture, fragTexCoord);
    float luma = dot(source.rgb, vec3(0.2126, 0.7152, 0.0722));
    float pulse = 0.5 + 0.5 * sin(seTime * 2.0);
    finalColor = vec4(mix(source.rgb, vec3(luma), strength * pulse), source.a);
}
```

Expose `strength` as a typed Float64 control and save the shader. Add it to an
ordered Rendering pipeline and save. Assign the `.vpipeline` path to the Camera;
keep its sibling `.vpipeline.host` document ledger alongside it. Ledger `D`
records map document IDs to hex-encoded **project-root-relative shader paths**.
Paths are data references; scenes never serialize native shader/target tokens.
The bundled `shaders/camera/vhs.vpipeline.host` is a complete example.

| Engine input | GLSL type | Meaning |
|---|---|---|
| `inputTexture` | `sampler2D` | Previous pass or external camera image |
| `inputTexture1` | `sampler2D` | Optional second declared pipeline input |
| `seTime` | `float` | Accumulated managed-frame game time for camera execution |
| `seDeltaTime` | `float` | Current managed-frame delta |
| `seInputResolution` | `vec2` | Actual first-input pixel dimensions |
| `seOutputResolution` | `vec2` | Current output target dimensions |

These inputs are engine-owned, not editable parameter schema entries. Active
engine inputs are checked before pipeline drawing; wrong types produce a
diagnostic. Source alpha and orientation are preserved by the fullscreen path.
For HDR effects, keep intermediate passes RGBA32F and add explicit tonemapping
before the final RGBA8/display output. Animated effects deliberately use time;
they are not `@deterministic` workloads.

### Gameplay-side API

`src/scene/camera_effects.vy` exposes a reusable owner for game code:

```vyne
# Construct scene, transform cache, asset bank, camera and owner before frames.
effects :: SECameraEffects := se_camera_effects();
se_camera_effects_prepare(app,effects,project,"shaders/camera/vhs.vpipeline",640,360);
while app.frame() {
    # VHS ledger document 3, parameter 1 (strength), component 0.
    se_camera_effect_set(effects,3,1,0,0.8);
    input :: vraylib.RenderTarget := vraylib.RenderTarget(effects.state[1],app.token);
    se_scene_capture(app,scene,cache,bank,cubes,player.camera,input);
    output :: vraylib.RenderTarget := se_camera_effects_run(app,effects);
    app.begin();
    try { output.present(float64(app.width()),float64(app.height())); }
    finally { app.end(); }
}
se_camera_effects_close(effects);
```

Update transforms before capture. Prepare/reload/resize with **no active passes**;
wrap the owner and App in the usual session `try/finally` cleanup. Parameter
updates validate against the loaded document schema and do not recompile GLSL.
Prepared camera output textures are borrowed; clear external sampler bindings
before resize or close. `se_camera_effects_reload` explicitly retries failed
loads/resizes and refreshes saved shader documents.

## Memory model, formats and limits

- Scene extension data is a fixed **128 × 16 Float64** sidecar. Two numeric slots
  select active camera/Sky. Seventeen owned extension snapshots accompany the
  existing sixteen-step undo ring. Copying Play state deep-copies these buffers.
- Picker state reuses the existing retained UTF-8 edit buffer. Catalog names are
  immutable literals. Temporary search strings and wrappers die at frame rewind.
- Camera resources use **four live + four staging** preframe document owners,
  at most **eight passes**, and reusable descriptors. Loads use a named 64 MiB
  `@bounded` staging region. No repeated `@persistent` allocation or
  `region.commit` occurs during edits, rendering or reload.
  Camera documents retain **8191 UTF-8 bytes per source stage**, **16 typed
  parameter rows**, and **511 UTF-8
  bytes per include dependency path**. Larger GLSL bodies can be organized in
  include files; oversized retained documents are rejected before publication.
  The editor's existing 128 MiB preframe arena limit remains enforced.
  The eight published scene-program owners retain 65,535 source bytes per stage,
  fitting their length prefix without power-of-two capacity doubling; the
  editable studio document retains its existing full 65,536-byte source limit.
- Camera targets are at most 2048 pixels per axis. The integrated viewport caps
  camera capture at 1024 on the longest axis and Rendering input at 512, keeping
  aspect ratio. The camera's **old + candidate GPU bank** is capped at 128 MiB,
  including its input and estimated depth: 20 bytes/pixel for RGBA32F and
  8 bytes/pixel for RGBA8 targets. Other editor/asset banks have their own budgets.
- GPU release is independent of arena rewind. Candidate resources are cleaned
  on failure; the entire old input/output target bank is released atomically
  before validated numeric ownership transfer. Repeated failed requests are
  suppressed until a changed request or explicit reload.
- Scene v4 adds `A` active-selection and `E` typed-extension records while
  retaining the 21-field node layout and existing material/world records. v1–v3
  still load; old scenes save in their older format until extensions are used.
  Reserved fields, duplicates, missing owners and invalid references are rejected
  in staging before replacing the authored scene. Pipeline ledgers use LF on
  Windows and POSIX checkouts.

Shadows, spotlights, cubemap skies, depth sampling and temporal-history effects
are later extensions. This implementation is bounded forward lighting and
color-based post-processing for one active player-camera output.

## Verification

```shell
python scripts/run_tests.py --suite compiler --suite ast --suite safety --suite codegen
python examples/projects/vmge/tests/run.py --poison
python examples/projects/vmge/tests/run.py --graphics --poison
```

Linux headless graphics: prefix the graphics command with `xvfb-run -a` and set
`LIBGL_ALWAYS_SOFTWARE=1`. CPU environment checks run on all desktop CI platforms;
camera pixel/ownership checks run in the existing Linux graphics job.

`environment.vy` covers the picker catalog, one-step Daylight undo, exact v4
persistence, invalid-load isolation, active selections, duplicated/deleted sun
links, Play copying and repeated frame/history churn. `camera_effects.vy` covers
real directional/point/terrain HDR pixels, VHS/tonemapping, bypass, resize,
failed shader reload, valid GLSL with an invalid engine-uniform contract,
externally referenced bank-close rejection, GPU peak-budget rejection, pass balance, poisoned-arena
reuse, zero commit drift and zero live GPU handles.
The integrated 24-frame environment flow additionally exercises picker modal
capture, camera Preview/Play isolation, the Rendering camera-input switch,
saved-ledger reload, viewport resize and v4 save/reopen in the actual editor. It
exports `build/vmge-tests/environment-flow.png` for the CI preview artifacts.

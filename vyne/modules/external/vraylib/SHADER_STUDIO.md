# Managed shader studio runtime

Requires the application's live desktop OpenGL 3.3 context. All operations are
main-thread only. Import `vraylib/vraylib.vy` as usual.

## Public APIs

```text
App.compile_shader(vertex :: String, fragment :: String) -> vraylib.ShaderBuild
ShaderBuild { shader :: vraylib.Shader, success :: Bool, log :: String }
Shader.uniforms() -> Array
Shader.set_typed(name :: String, gl_type :: Int64, values :: Array<Float64>) -> Bool
Shader.set_texture(name :: String, texture :: vraylib.Texture) -> Bool
Model.draw_material(transform :: vraylib.Matrix, shader :: vraylib.Shader,
                    material_slot :: Int64, tint :: vraylib.Color) -> Bool
App.float_target(width :: Int64, height :: Int64) -> vraylib.RenderTarget
RenderTarget.read_pixels() -> Array<Float64>
RenderTarget.read_pixel(x :: Int64, y :: Int64) -> Array<Float64>
RenderTarget.format() -> Int64
RenderTarget.copy_from(source :: vraylib.RenderTarget) -> Bool
RenderTarget.draw_from(source :: vraylib.RenderTarget, shader :: vraylib.Shader) -> Bool
App.can_release(tokens :: Array<Int64>) -> Bool
App.can_release_tokens(tokens :: Array<Int64>) -> Bool
App.release_bank(tokens :: Array<Int64>) -> Bool
```

`type` is reserved in Vyne; the second `set_typed` parameter is `gl_type`.
Existing `width`, `height`, `resize`, `texture`, pass and close methods apply.
These extensions use `managed_config`; registration and generated bindings are
unchanged. Existing path-based `load_shader` caching is unchanged.

## Candidate compilation

Inputs are GLSL **source strings**, not paths. Null vertex selects the standard
GLSL 330 vertex stage with `vertexPosition`, `vertexTexCoord`, `vertexColor`,
`mvp`, `fragTexCoord`, and `fragColor`. Fragment source is required. Standard
raylib vertex attribute bindings and matrix/material uniform locations are
initialized for both batched drawing and mesh drawing.

Every call builds a distinct, uncached candidate. Ordinary syntax or stage-link
failures return `success == false`, `shader.token == 0`, and a diagnostic log;
they never return the default shader. GL shader/program creation failure also
returns a failed build. Invalid arguments, unavailable context/entry points,
and native allocation failures throw. Existing good shaders stay alive.

Driver logs are collected directly from GL and bounded to 16 KiB including the
terminator. Long diagnostics end with `[driver log truncated]`. Successful builds
can contain driver warnings; a clean build has an empty log. No trace callback is
installed and no logger setting is changed, so the caller's logger state is
preserved. Stage objects are detached/deleted; failed programs and candidate
metadata are released. A successful shader belongs to the application.

The returned log and reflection/readback arrays are owned transient language
values, subject to the normal frame-arena lifetime. Copy text/data into your
persistent document storage when retaining it across frame rewinds. Native
session metadata stores owned native strings and generation-checked tokens,
never pointers to those transient values.

## Reflection and typed values

`uniforms()` returns rows in driver enumeration order:

```text
[name :: String, gl_type :: Int64, array_length :: Int64, location :: Int64]
```

Only **active** uniforms are enumerated; optimized-out declarations are absent.
Array names normally end in `[0]`. Length is the driver's reflected active
array length, which may be shorter than the declared length. Uniform-block
members can appear with location `-1`; they cannot be uploaded by `set_typed`.
Reflection is bounded to 4096 uniforms and 4096 bytes per name.

Supported standard GL types (these are GL enums, **not** raylib's
`SHADER_UNIFORM_*` enums):

| GLSL | GL enum | Decimal | Components per element |
|---|---|---:|---:|
| float | GL_FLOAT `0x1406` | 5126 | 1 |
| vec2 | GL_FLOAT_VEC2 `0x8B50` | 35664 | 2 |
| vec3 | GL_FLOAT_VEC3 `0x8B51` | 35665 | 3 |
| vec4 | GL_FLOAT_VEC4 `0x8B52` | 35666 | 4 |
| int | GL_INT `0x1404` | 5124 | 1 |
| ivec2 | GL_INT_VEC2 `0x8B53` | 35667 | 2 |
| ivec3 | GL_INT_VEC3 `0x8B54` | 35668 | 3 |
| ivec4 | GL_INT_VEC4 `0x8B55` | 35669 | 4 |
| bool | GL_BOOL `0x8B56` | 35670 | 1 |
| bvec2 | GL_BOOL_VEC2 `0x8B57` | 35671 | 2 |
| bvec3 | GL_BOOL_VEC3 `0x8B58` | 35672 | 3 |
| bvec4 | GL_BOOL_VEC4 `0x8B59` | 35673 | 4 |
| mat4 | GL_FLOAT_MAT4 `0x8B5C` | 35676 | 16 |
| sampler2D | GL_SAMPLER_2D `0x8B5E` | 35678 | use `set_texture` |

Other commonly reflected types include mat2 (`0x8B5A`), mat3 (`0x8B5B`),
samplerCube (`0x8B60`), unsigned int (`0x1405`), uvec2/3/4
(`0x8DC6`/`0x8DC7`/`0x8DC8`), and double (`0x140A`). They are reported by
reflection but are **unsupported uploads**. Unsupported types receive explicit
diagnostics, including non-mat4 matrices, unsigned/double values, sampler arrays,
cube/shadow/integer samplers, and uniform-block uploads.

`set_typed` verifies exact reflected type and flattened cardinality
`array_length * components`. Whole-array upload accepts either the base name or
its reflected `[0]` name; nonzero element names are rejected. Arrays are bounded
to 65536 reflected elements. Values must be finite and fit finite Float32 range;
integer/bool values arrive as Float64 but must be exact signed 32-bit integers,
with booleans restricted to 0 or 1. Matrices are **column-major**, 16 values per
mat4, with no transpose. Invalid uploads throw before changing the uniform;
successful uploads return true. GL upload failures throw after restoring the
program binding. Queued batched draws are flushed before shared uniform changes.

`set_texture` validates a scalar sampler2D and keeps a dependency on its logical
texture/target. Up to four managed sampler uniforms may be bound per shader.
`set_texture(name, null)` removes the managed binding/dependency; assign a new
texture before using that sampler again. Logical target bindings refresh after
resize. Sampling the current render target is rejected on sampler assignment,
shader-pass begin and material drawing; managed texture draws/native-view access
also reject same-target feedback. Raw GL calls remain the raw API's responsibility.

## Shared geometry / per-instance drawing

`draw_material` requires an active drawing surface and camera. It draws all model
meshes with the supplied transform/tint, replacing the shader **only** for meshes
assigned to the zero-based `material_slot`. Bounds are validated before drawing.
It uses stack-local copies of materials/maps; shared geometry, model transform,
material shaders, colors, textures and permanent dependency edges stay intact.
Managed sampler textures are refreshed from logical views on each draw. Standard
texture0/1/2 sampler bindings replace local map copies; custom samplers use texture
units 12..15 and restore those bindings and the previous active texture/program.

Set each instance's uniforms immediately before its draw, even when instances
share a program. Uniform state belongs to the program, not the Model wrapper.
Temporary overrides create no persistent shader reference. Permanent
`Model.set_shader` references retain the existing semantics: closing a referenced
shader fails until the referring resource closes or changes its attachment.

## Float targets and readback

`float_target` allocates RGBA32F (`PIXELFORMAT_UNCOMPRESSED_R32G32B32A32`), with
depth storage and framebuffer completeness validation. Unsupported allocation
throws; it never falls back to RGBA8. Dimensions are 1..16384. `format()` returns
the raylib pixel-format enum; `width()`/`height()` report current size.

`resize` preserves the format and existing logical texture views and sampler/model
bindings. Resizing an active target is rejected. A referenced raw attachment
blocks resize, preserving the old target; use `target.texture()` logical views.
Allocation failure also preserves the old target. Rendering content is discarded
on a successful resize. Float targets do not automatically follow window size.

Creation and resize snapshot and restore the **separate** read/draw framebuffer
bindings, active texture unit and its 2D binding, renderbuffer binding, and unpack
PBO/alignment/row/skip/swap state. Null texture allocation always runs with unpack
PBO zero and canonical unpack state. Cleanup after GPU allocation or raw-view
rebind failure restores that state too. On successful resize, a raw binding to
the deleted target FBO/color texture/depth renderbuffer is remapped to its new
storage; unrelated bindings retain their values.

`read_pixels` returns a flat row-major RGBA array in **top-left** origin order.
`read_pixel(x,y)` uses the same top-left coordinates and returns four values.
RGBA8 targets yield normalized channels (byte/255); float targets yield their
unquantized stored Float32 values promoted to Float64, preserving negative and
HDR values. Blending, framebuffer writes and shader output still determine the
stored values. Full readback is bounded to 16 million pixels; use `read_pixel`
for larger targets. Out-of-bounds reads throw. Readback flushes pending drawing
and restores the read framebuffer, pixel-pack buffer/alignment/row/skip/swap state.

## Replacement copies and fullscreen effects

`destination.copy_from(source)` is a GPU color-attachment replacement blit. Both
targets must be live, belong to the same application, have equal dimensions, and
be distinct. It requires no active render passes; an active frame is not required.
Same-format RGBA8 and RGBA32F copies preserve **all four stored channels**, including
nonopaque alpha and negative/HDR float values, with no blending, tint or vertical
flip. RGBA8→RGBA32F is supported with normal GL normalization/Float32 rounding.
RGBA32F→RGBA8 narrowing and other formats are rejected explicitly. Copy flushes
queued drawing and restores separate FBOs, attachment read/draw-buffer selection,
scissor and framebuffer-sRGB state. Use `draw_from` for intentional shader conversion
or scaling.

`destination.draw_from(source, shader)` owns an immediate fullscreen replacement
draw and requires a live **active application frame with no active passes**. It
does not open a nested managed pass or alter the application's pass stack. Source
and destination must differ. RGBA8 and RGBA32F targets are supported; output is
scaled to the destination size and stored according to its format.

The source is automatically supplied to an active scalar `texture0` sampler2D.
Other configured managed sampler uniforms are refreshed from their logical views
and assigned separate units. There is **no blanket requirement** that every active
or declared sampler be configured; the pipeline decides which inputs are required.
Optional/optimized-out samplers do not make a fullscreen pass fail. Any configured
sampler that references the destination is rejected before drawing, even if it is
the overridden `texture0` binding. Use `shader.set_texture(name, null)` to remove
such a binding.

The default vertex stage works directly. Custom fullscreen vertex stages may use
float scalar/vector attributes named `vertexPosition`, `vertexTexCoord`, and
`vertexColor`, or no attributes with `gl_VertexID`. Integer, array, and other vertex
attributes receive explicit unsupported diagnostics. Fullscreen input positions
are clip-space, UVs are native GL bottom-left, and vertex color is white; top-left
logical readback orientation is preserved. Active built-ins must be scalar `mvp`
mat4, `colDiffuse` vec4, and `texture0` sampler2D. `mvp` is temporarily identity and
`colDiffuse` temporarily white; their prior values and sampler assignments are
restored afterward. User effect uniforms retain their values.

Blend, depth/stencil/cull/scissor, rasterizer discard, framebuffer-sRGB, dither,
alpha-to-coverage and sample coverage are disabled for the replacement draw;
color writes are enabled and polygon mode is fill. The draw restores the previous
enable states, color mask, polygon mode, viewport, program, VAO/array-buffer,
texture/sampler bindings for units 0..4, active texture, and separate FBO bindings.
Blend factors/equations are not changed. Native geometry is temporary and is
deleted on success or driver allocation/draw failure, with state restoration
before any error is raised. A float-target identity effect therefore preserves
nonopaque RGBA instead of applying SrcAlpha a second time.

## Transactional resource banks

`app.can_release(tokens)` (also exposed as `app.can_release_tokens(tokens)`) is a
nonmutating, allocation-free preflight. It returns
false if a token is zero/stale/foreign, is the application session itself, is a
borrowed/implicit child rather than an owned root, if **any** managed render pass
is active, or if a dependency enters the bank from an ownership root outside it.
Ownership descendants are included automatically. Checking incoming edges over
the complete bank covers transitive external paths and sharing cycles. Duplicates
are accepted; an empty bank succeeds when no pass is active. Invalid array shape
or a bank larger than 65536 tokens throws.

`app.release_bank(tokens)` repeats the full preflight and allocates bookkeeping
before mutation. A blocked preflight or allocation failure throws with every
resource and dependency intact. After success it removes outgoing edges from
the bank and releases all roots, including mutually referring resources and
their owned descendants, returning true. The caller's token array is unchanged.
This is the bank replacement primitive: preflight old-bank tokens before swapping
pipeline state, then release them together. There is no partial-release loop on a
blocked old bank. Existing single-resource close/dependency semantics still apply.

## Focused tests

```sh
python vyne/runtime/tests/shader_run.py
```

Uses the existing compiler, `build/shader-runtime-dev` outputs, and its separate
`VYNE_RAYLIB_CACHE`. Requires a real desktop GL display (a GL-capable virtual
display is suitable on Linux). Does not invoke the full test runner.

- `shader_studio.vy`: public facade compilation, failure result shape, reflection,
  HDR readback, resize/logical views, format, replacement copy/identity preserving
  nonopaque RGBA, bank preflight/release and application-close cleanup.
- `shader_runtime.c`: real stage/link failures and owned log survival; reflection
  arrays/types/inactive uniforms; invalid cardinality/type/range/finite/bool
  uploads; mat4 driver-value verification; same-target feedback; refreshed sampler
  readback; uniform-block and unsupported sampler rejection; RGBA8 normalization,
  float HDR/top-left readback, pixel-pack state restoration and resize; shared
  two-material/two-instance pixel separation and complete material restoration;
  custom sampler instance textures; permanent shader dependencies;
  injected shader/program creation failures,
  native allocation failures (including registry growth), reflection/readback
  partial results, float-target construction and preserving targets on resize
  allocation failure; 100 reloads, GPU object deletion,
  zero live handles on close and context recreation.
- `shader_state.c`: deliberately perturbed separate FBO, active texture, renderbuffer
  and unpack/PBO state during creation, resize, native allocation failures, real
  partial GPU allocation failure and raw-view rebind rollback; successful raw-binding
  remapping; exact same-format HDR/nonopaque replacement and top-left identity;
  RGBA8→float conversion, unsupported narrowing/size/attribute/feedback/pass paths;
  fullscreen state and built-in uniform restoration with blend both enabled and
  disabled; temporary VAO/buffer cleanup on injected driver allocation failures;
  transitive/ownership-descendant bank preflight, external-dependency and allocation
  rollback, cycles, duplicates, stale/foreign/borrowed/session tokens and close cleanup.

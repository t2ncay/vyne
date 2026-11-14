# vraylib — raylib 6.2, raymath, and rlgl for compiled Vyne

The [managed external facade](MANAGED.md) adds App-owned typed resources,
automatic frame memory/music updates, cached named uniforms, and managed render
passes. Start there for application code; this reference documents native API
coverage and the lower-level helpers.

```vyne
use external "vraylib/vraylib.vy";
vraylib.open(800, 450, "Hello raylib", 60);
while !vraylib.window_should_close() {
    region frame {
        vraylib.begin_drawing();
        vraylib.clear_background(vraylib.RAYWHITE);
        vraylib.draw_text("Hello from Vyne", 40, 40, 24, vraylib.BLUE);
        vraylib.end_drawing();
    };
}
vraylib.shutdown();
```

Run with `vynec --run hello.vy`, or build with `vynec --compile hello.vy`.
The first build compiles the vendored library; later builds reuse its cache.
There is no raylib DLL to copy beside the resulting program.

## Requirements and platforms

Compiled desktop Windows (MinGW), Linux, and macOS. `--ast`, AVR, and ESP32
produce explicit diagnostics. Rendering requires a desktop OpenGL context.
The default graphics backend is **OpenGL 3.3 / bundled GLFW** with audio and JPEG.

- Windows: GCC/MinGW, CMake, `mingw32-make` on PATH.
- Ubuntu: GCC, CMake, make, `libgl1-mesa-dev libx11-dev libxrandr-dev
  libxinerama-dev libxcursor-dev libxi-dev`; use `xvfb` for CI rendering.
- macOS: CMake, make, Apple command-line tools. The compiler itself needs a
  C++23-capable toolchain, as documented in the main README.

Native archives live in `%LOCALAPPDATA%/vyne/raylib/`,
`$XDG_CACHE_HOME/vyne/raylib/`, or `~/.cache/vyne/raylib/`. Override with
`VYNE_RAYLIB_CACHE`. The cache fingerprints the compiler, source location,
vendored source contents, binding contents, and build configuration.
`--emit-c` / `--emit-asm` do not build or link the native archives.

## API naming and coverage

`use native vraylib as rl;` exposes the native API without loading facade helpers.
The umbrella facade above adds typed constructors and reusable higher-level
helpers. All **619 raylib**, **146 raymath**, and **163 rlgl** declarations are
inventoried in [`api.json`](../../../runtime/vraylib/api.json), with their original
C signature, Vyne name, binding status, and capability restrictions.

| Upstream | Vyne |
| --- | --- |
| `InitWindow` | `vraylib.init_window` |
| `BeginMode3D` | `vraylib.begin_mode_3d` |
| `Vector3Normalize` (raymath) | `vraylib.math_vector3_normalize` |
| `rlDrawRenderBatchActive` (rlgl) | `vraylib.gl_draw_render_batch_active` |
| `KEY_SPACE`, `FLAG_VSYNC_HINT`, `RL_TRIANGLES` | original uppercase constants |

Of 928 declarations, **901 have direct/adapted pointer or resource bindings,
2 use literal-safe text adapters, and 25 are explicitly unavailable**:

- C callback setters/processors cannot capture arena-backed Vyne closures and
  have no Vyne callback ABI. They fail at compile time; no callback stubs run.
- `rlglInit`, `rlglClose`, and `rlLoadExtensions` are managed by raylib's window
  lifecycle and are not callable separately through the facade.
- Software-framebuffer and OpenGL 4.3 compute/SSBO operations are unavailable
  in the bundled OpenGL 3.3 build.
- `trace_log(level, text)` and `text_format(text)` treat their text literally.
  Format in Vyne with string interpolation; C printf varargs are not exposed.

The exact list and signatures in the inventory are authoritative. Raw rlgl
state operations retain upstream context/state requirements. These APIs are
main-thread-only; they are not a replacement for a concurrent rendering system.

## Values, pointers, and ownership

- Colors, vectors, matrices, quaternions (`Vector4`), rectangles, cameras, rays,
  bounding boxes, and transforms use ordinary Vyne value structs. Named structs
  or positional arrays can be passed to native value parameters. Returned fields
  use the compilation's StringPool ids, so ordinary `.x` / `.position` access works.
- Resource values are **Int64 generation-checked handles**, not C addresses.
  Resource type and unload function are checked. Released/borrowed handles are
  rejected before native code uses them.
- Native resources and buffers survive region rewinds. Temporary input
  conversions are reclaimed at the end of each call; returned value structs live
  in the caller's region. Use a `region frame` for bounded transient frame data.
- `shutdown()` destroys tracked resources while their graphics/audio contexts
  still exist, then closes the contexts. An exit hook also handles unhandled
  errors and ordinary program exit. Explicit unloading remains available.
  `close_window()` and `close_audio_device()` first release the resources of
  their respective contexts; CPU images, waves, and buffers have separate lifetimes.
- Models own their imported meshes and internally loaded textures. Constructing
  a model from an owned mesh transfers mesh ownership. Borrowed material/texture
  views are invalidated when their parent is destroyed.
- Explicit model/material texture and shader assignments create dependencies;
  release the model/material before unloading a referenced resource.

### Typed pointer buffers

```vyne
count := vraylib.buffer("int", 1);
codepoints := vraylib.load_codepoints("Vyne", count);
out(vraylib.buffer_read(count, 0));          # 4
out(vraylib.buffer_read(codepoints, 0));     # 86
vraylib.unload_codepoints(codepoints);
vraylib.release(count);
```

Use exact C element spellings (`"float"`, `"int"`, `"unsigned char"`,
`"Vector2"`, `"Camera3D"`, etc.). `float_buffer`, `int_buffer`, and `bytes`
copy Vyne arrays into persistent native buffers. `buffer_size`, `buffer_read`,
`buffer_write`, `valid`, and `release` manage them. C mutable/output pointers
require buffers; supported input array conversions last only for the call.
Retained-pointer APIs must receive persistent buffers kept alive until detached.
Pointer/count and shader-uniform byte capacities are validated where their size
is specified by the C signature. Upstream parameter/context requirements still
apply to raw low-level operations.

Allocation-returning APIs retain their upstream unload operation. For example,
`LoadImageColors` yields a Color buffer handle and `load_file_text` yields a
character buffer handle; character handles can be passed to string parameters.
Static returned strings are copied immediately, before another call can reuse
the upstream buffer. Resource structs are not shallow-copyable into buffers.

`field(handle, name, index)` inspects bounded resource fields and arrays.
`set_field` allows non-owning scalar/value changes; ownership-sensitive fields
must use their dedicated API. For animation, `buffer_read(animations, i)` borrows
a sequence and `animation_pose(sequence, keyframe)` borrows its Transform buffer.
Unload the original animation collection with its original count.

### Cameras, models, and post-processing

`camera_create` stores a mutable Camera3D in a native buffer. Use `camera_view`
for a value passed to `begin_mode_3d`, plus `camera_position`,
`camera_set_position`, `camera_set_height`, `camera_step`, and `camera_mouse_look`.

`textured_cube`, `textured_plane`, `model_at`, `model_texture`, and `model_shader`
reuse meshes/materials. `present_target` handles the negative-height source
rectangle needed to display a render texture upright and reuses its attachment
handle. `uniform_float`, `uniform_vec2`, and `uniform_vec3` reuse one caller-owned
float buffer instead of allocating uniform storage every frame.

`ambiance(path, volume)` opens audio if available and starts looping streamed
music; call `update_music_stream` each frame. It returns zero when no audio device
is available. Sound aliases are dependent resources and are released before
their backing sound.

### rlgl resource/state rules

`GL.vy` supplies `flush()`. Flush raylib's batch before raw rlgl drawing or state
changes, and restore the state needed by subsequent raylib operations.
rlgl-created VAOs, VBOs, textures, shaders, shader programs, and FBOs are checked
handles accepted by their matching API. Borrowed raw IDs from resource fields or
default-ID getters may be used for binding, but cannot be unloaded as owned handles.
Depth renderbuffers use `release()` because rlgl has no standalone delete API.
Managed FBO attachments remain independently owned; detach/unload the FBO before
releasing its attached owned resources. This avoids upstream FBO depth deletion
double-freeing an independently tracked texture/renderbuffer.

## Examples and verification

- [`raylib-showcase`](../../../../examples/projects/raylib-showcase/README.md):
  minimal desktop project and automated tests.
- [`camera_test.vy`](../../../../examples/graphics/camera_test.vy): full original
  bodycam scene, modernized syntax and frame-time-based movement.

```sh
python scripts/generate_vraylib.py --check
python examples/projects/raylib-showcase/tests/run.py
python examples/projects/raylib-showcase/tests/run.py --graphics
python examples/projects/raylib-showcase/tests/run.py --graphics --camera --audio
# Linux:
xvfb-run -a python3 examples/projects/raylib-showcase/tests/run.py --graphics --sanitize
```

The tests verify conversion/field ids, pointer outputs, binary roundtrips,
wrong unloads, stale handles, catchable bounds errors, GPU readback, mixed rlgl
drawing, mesh transfers, dependency protection, bounded handles, and `vimage`
coexistence. Native sanitizer checks instrument both bridge and upstream raylib.
The camera's original asset files are needed only for `--camera`.

## Updating upstream

The unmodified vendored source is pinned in `vendor/raylib/UPSTREAM.md`, under
its upstream zlib/libpng license. Vyne's bridge is MIT; compiler integration is
AGPL-3.0-only. Update the upstream pin, review ownership/capability policy in
`scripts/generate_vraylib.py`, regenerate, and run the acceptance suite. Never
hand-edit the generated binding tables.

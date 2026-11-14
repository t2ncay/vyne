# Managed external vraylib facade

Import `vraylib/vraylib.vy` to get a hybrid API: an `App` owns resources and
frames, resource objects expose methods, and drawing primitives remain concise
module functions. The native raylib/raymath/rlgl calls are also available.

## Start with an application

```vyne
use external "vraylib/vraylib.vy";
app :: vraylib.App := vraylib.app(800, 450, "My application");
try {
    while app.frame() {
        app.begin();
        app.clear(vraylib.RAYWHITE);
        vraylib.draw_text("Hello", 40, 40, 24, vraylib.BLUE);
        app.end();
    }
} finally {
    app.close();
}
```

`app(width,height,title)` defaults to 60 FPS, a resizable window and VSync.
`app_options(width,height,title,fps,flags)` selects explicit raylib flags.
`session(asset_root)` creates the same App without opening graphics, useful for
CPU image processing. A headless loop needs `limit(n)` or an explicit `stop()`.
Only one managed application/context owner may be live at once.

`frame()` completes the previous frame, reclaims its transient arena data,
updates streamed music, resizes screen-following targets, and starts a new frame.
It returns false on window exit, `stop()`, or the configured frame limit.
`end_frame()` finishes a frame explicitly; `close()` aborts any unfinished frame
and releases the application's resources and contexts. Both close operations on
already-released resource tokens and App.close are idempotent.

### Let App own the loop

```vyne
fn update(app :: vraylib.App) -> Bool {
    # Your simulation; app.dt() is clamped to at most 0.05 seconds.
    return true;
}
fn draw(app :: vraylib.App) -> Bool {
    app.clear(vraylib.BLACK);
    vraylib.draw_text("Managed loop", 20, 20, 24, vraylib.WHITE);
    return true;
}
app :: vraylib.App := vraylib.app(800, 450, "Callbacks");
app.run(update, draw);
```

`run(update,draw)` invokes named compiled Vyne functions with the App argument,
opens/closes window drawing for `draw`, and closes the application on success or
failure. Headless sessions invoke both callbacks without opening drawing passes.
It returns the completed frame count. These are ordinary Vyne calls,
not C callbacks. Named function values and callable parameters use the compiler's
boxed ABI; callback arity/type errors are catchable. Use `stop()` for loop exit.

## Typed, shared resources

```vyne
app.assets("assets");
texture :: vraylib.Texture := app.load_texture("building.png");
model :: vraylib.Model := app.cube(texture);
camera :: vraylib.Camera := app.first_person();
```

Factories return `Texture`, `Image`, `Model`, `Font`, `Shader`, `Camera`, `Music`,
and `RenderTarget` value wrappers. Internally each wrapper carries stable tokens.
Copying a wrapper shares the underlying resource; it does not duplicate ownership.
`valid()` checks the session and generation. Early `close()` is available, and
dependent resources must be released before resources they reference.

App loads cache by resolved path and resource kind. Repeating a load returns the
same token. Closing it invalidates aliases and removes the cache entry, so a later
load creates a fresh resource. Cache paths, uniform names, ownership and pass
state are stored persistently, even when first populated inside a frame.
Changing `assets(root)` affects subsequent path resolution. Absolute paths are
accepted. Relative roots are relative to the process working directory.

| App factory | Result / purpose |
| --- | --- |
| `load_texture(path)` | Checked GPU texture; `width`, `height`, `draw`, `close` |
| `load_image(path)`, `image(w,h,color)` | CPU image; `resize`, `save`, `close` |
| `load_model(path)`, `cube(texture)`, `plane(texture,w,length)` | Model; material count, shader/texture assignment and drawing |
| `load_font(path)` | Font; `draw(text,position,size,tint)` |
| `load_shader(vertex_or_null,fragment)` | Custom shader; rejects default fallback on compilation failure |
| `camera(position,target,fovy)`, `first_person()`, `orbit(position,target)` | Persistent mutable camera |
| `music(path,volume)` | Looping streamed audio; automatically updated every frame |
| `target(w,h)`, `screen_target()` | Fixed-size or window-following render target |

Model/camera/target drawing requires an open App window. Image sessions need no
graphics or audio device. Music has `valid`, `play`, `pause`, `volume` and `close`;
if no audio device can open, it returns an unavailable Music token (`valid=false`).
Missing files and failed resource/shader loads produce descriptive exceptions.

## Named shader uniforms

```vyne
shader :: vraylib.Shader := app.load_shader(null, "post.fs");
shader.set("time", app.elapsed());
shader.set("size", vraylib.vec2(float64(app.width()), float64(app.height())));
shader.set("matrix", vraylib.math_matrix_identity());
shader.set_ints("counts", [1, 2, 3]);
shader.set_texture("sourceTex", target.texture());
```

`set` supports Float64/Float32 scalars, Int64/Bool scalars, numeric vectors of
1..4 components (arrays or Vector2/3/4), and Matrix values. Use floating literals
for GLSL floats; `1` is an int and `1.0` is a float. `set_ints` selects integer
vector uniforms. `set_texture` selects sampler uniforms and retains dependencies.
Locations are queried once per shader/name; warm updates use stack scratch
storage, so users do not allocate uniform buffers. Missing or optimized-out
uniforms throw a named-operation error rather than silently disabling an effect.
`uniform_queries()` provides diagnostic verification of the cache.

## Render targets, passes and cameras

```vyne
scene :: vraylib.RenderTarget := app.screen_target();
scene.begin();
app.clear(vraylib.BLACK);
camera.begin();
model.draw(vraylib.vec3(0.0, 0.0, 0.0), 1.0, vraylib.WHITE);
camera.end();
scene.end();
app.begin();
scene.present(float64(app.width()), float64(app.height()));
app.end();
```

Targets, cameras and shaders expose `begin/end` and `with_draw(callback)`.
The latter invokes a zero-argument named function and pairs the pass using
try/finally. Passes are tracked in LIFO order and unwound when a frame or App
closes, including exceptions. A drawing surface must precede camera/shader
passes; nested targets or same-kind passes are rejected rather than corrupting
raylib's state. Keep raw begin/end calls outside managed pass composition.

Target `present` handles render-texture orientation. `texture()` returns a
borrowed, non-owning logical view that follows target resizing; its `close()`
does not release the target. `resize(w,h)` replaces storage while preserving the
target token. `screen_target()` follows the window at the next frame boundary.
Managed shader samplers are refreshed when their shader pass begins, so a cached
sampler continues to reference a resized logical target. Resizing active targets
is rejected. Managed model texture assignments also refresh after replacement.
Raw attachment references used by low-level model/material APIs
must be detached before replacing storage; `native_handle()` is an explicit
escape hatch whose returned attachment handle is invalidated by resize.

Camera methods include `position`, `set_position`, `set_height`, `move`, `look`,
`update`, `begin/end` and `with_draw`. First-person update normalizes diagonal
WASD input and uses App.dt; orbit uses raylib's orbital controller. Speed and
sensitivity are wrapper value fields that can be configured before updating.
Collision, crouching, jumping and scene-specific physics remain application
logic, as demonstrated by the full bodycam example.

## Region and exception rules

The managed frame **is** a transient arena scope. Avoid keeping new strings,
arrays, maps or boxed records from one frame into the next. Native resources
loaded during a frame survive because the session owns their native storage;
thin primitive-token wrappers can be copied, and cached assets can be retrieved
again in the next frame. Long-lived application containers should be created
before the loop; use explicit `region.commit` for arena data that must escape.

Start managed frames outside `@persistent` regions. Balanced nested ordinary
regions are fine. An exceptional nested persistent region is exited during frame
cleanup so its allocator routing and commit lock do not remain active.
The frame exception hook promotes string/array/map/struct error data into the
commit arena before throw propagation. Consequently errors remain readable after
App.close or automatic run cleanup; their copies live until program teardown.
Shared and cyclic array/map/struct error graphs are copied with identity tracking.
Frame scopes bound allocation lifetimes, not a guarantee of zero allocations.

## Diagnostics and examples

App exposes `frames`, `dt`, `elapsed`, `width`, `height`, `resources`, `passes`,
`uniform_queries`, `valid`, `limit`, `fps` and `stop`. The full bodycam example
now uses typed assets, managed frames/music, named uniforms and resizing targets.
The small desktop showcase demonstrates a managed orbit camera.

```sh
python examples/projects/raylib-showcase/tests/run.py --graphics
python examples/projects/raylib-showcase/tests/run.py --graphics --camera --audio
```

The original low-level facade and the pinned API coverage/capability limits remain
documented in [README.md](README.md).

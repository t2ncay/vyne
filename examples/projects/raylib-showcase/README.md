# raylib showcase and acceptance workspace

This is the small standalone desktop project for `vraylib` (raylib 6.2,
raymath, rlgl). The original **Bodycam Horror** scene remains at
`examples/graphics/camera_test.vy` and is the larger real-world showcase.

The project now uses the [managed external facade](../../../vyne/modules/external/vraylib/MANAGED.md):
an App owns typed resources and frame regions, an orbit camera supplies controls,
and application shutdown is paired with try/finally. The bodycam example uses
the same App API for cached assets, named shader uniforms, streamed music, and
screen-following post-processing targets.

## Run the project

```sh
cd examples/projects/raylib-showcase
vynec build
vynec run
```

The first build requires GCC, CMake, and make and builds the vendored native
library in the user cache. Windows/Linux/macOS link it statically. No raylib DLL
or system-installed raylib package is needed. See the
[library manual](../../../vyne/modules/external/vraylib/README.md) for dependencies,
API naming, pointer adapters, resource lifetimes, and capability restrictions.

## Automated tests

From the repository root (the runner also works from other directories):

```sh
python examples/projects/raylib-showcase/tests/run.py
python examples/projects/raylib-showcase/tests/run.py --graphics
python examples/projects/raylib-showcase/tests/run.py --graphics --camera
python examples/projects/raylib-showcase/tests/run.py --graphics --camera --audio
```

Linux graphical/sanitizer checks:

```sh
xvfb-run -a python3 examples/projects/raylib-showcase/tests/run.py --graphics --sanitize
```

`VYNEC` selects the compiler executable. Outputs and disposable negative-test
fixtures stay under `build/vraylib-tests/`. Core tests need no display or audio
device. `--graphics` checks actual rendered pixels and writes `render.png`.
`--sanitize` instruments the native bridge and vendored library, not only the
calling test. `--camera` renders 30 frames using the original local assets and
asserts completion with zero live handles. `--audio` opts into a real audio device.

## Original camera demo

```sh
vynec --run examples/graphics/camera_test.vy
```

Controls: WASD, mouse look, Shift sprint, Ctrl crouch, Space grounded jump,
Tab cursor capture, F11 fullscreen, Escape exit. The camera starts at the original
position facing the scene. Cat/airplane animation, all 16 buildings, fog,
VHS/scanline post-processing, the VCR font, ambient music, and the recording
overlay are preserved. Movement and animation use real elapsed frame time.

The existing `examples/assets/` files are locally stored and ignored by Git.
Keep the original OBJ/MTL files and their texture dependencies together there;
shaders remain in `examples/graphics/shaders/`. Missing assets produce a named
error and resource cleanup; the demo does not silently substitute a different scene.
The scene's texture/font/model/audio files have not been moved or overwritten.

For a bounded run, set these environment variables before running the binary:

| Variable | Default | Purpose |
| --- | --- | --- |
| `VYNE_CAMERA_FRAMES` | `0` | Zero means interactive; otherwise exit after N frames |
| `VYNE_CAMERA_WIDTH` | `1920` | Initial window/render-target width |
| `VYNE_CAMERA_HEIGHT` | `1080` | Initial window/render-target height |
| `VYNE_CAMERA_AUDIO` | `1` | Zero skips audio device initialization |

Assets resolve from the repository working directory or relative to the normal
example/build executable locations. All other historical graphics scripts remain
available for future individual migrations.

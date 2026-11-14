---
id: graphics
title: Graphics Showcase
group: Showcase
icon: fa-gamepad
order: 30
description: Run the managed raylib project, Bodycam Horror, and Sector Shift; verify rendering, resource cleanup, and audio.
---

# Graphics Showcase

[vraylib](page:vraylib) integrates vendored raylib 6.2, raymath, and rlgl into
compiled desktop Vyne. The [managed facade](page:vraylib-managed) adds App-owned
resources, frame arena cleanup, cameras, named shader uniforms, resizable render
targets, streamed music, and one-shot sound.

## Start with the portable project

```shell
vynec build examples/projects/raylib-showcase
vynec run examples/projects/raylib-showcase
```

This small orbit-camera project needs no ignored historical assets. The first
build compiles vendored raylib into the user cache; later builds reuse it.
Install GCC, CMake, make, and your platform's graphics development dependencies
as described in [Installation](page:install). Rendering needs an OpenGL context.
The native library is statically linked; no raylib DLL needs copying.

## Bodycam Horror

From the repository root:

```shell
vynec --run examples/graphics/camera_test.vy
```

The restored scene includes the original 16 buildings, model animations, fog,
VHS/scanline post-processing, VCR font, ambient audio, and recording overlay.
The managed API caches typed assets, updates music each frame, tracks render
passes, caches named uniform locations, and resizes screen-following targets.
Movement/animation use elapsed time; collision, crouch, and jumping remain scene logic.

| Input | Action |
| :--- | :--- |
| WASD / mouse | Move / look |
| Shift / Ctrl / Space | Sprint / crouch / grounded jump |
| Tab | Toggle cursor capture |
| F11 / Escape | Fullscreen / exit |

The original OBJ/MTL, texture, font, and audio files in `examples/assets/` are
local and ignored by Git. Preserve their relative dependencies. They are needed
for this full scene, and are not downloadable assets bundled with the site.
Missing assets produce named errors and managed cleanup.

For a bounded PowerShell run:

```powershell
$env:VYNE_CAMERA_FRAMES = "30"
$env:VYNE_CAMERA_WIDTH = "640"
$env:VYNE_CAMERA_HEIGHT = "360"
$env:VYNE_CAMERA_AUDIO = "0"
vynec --run examples/graphics/camera_test.vy
```

Unset the variables or set `VYNE_CAMERA_FRAMES=0` to return to interactive mode.

## Sector Shift / Sector 4 Incident

```shell
vynec --run examples/graphics/sector_shift.vy
```

The managed horror corridor retains the paired walls, stalker billboard, radio
log, bodycam malfunction, blackouts, recording overlay, and ending. WASD moves,
Shift sprints, Tab toggles mouse capture, F11 toggles fullscreen, and Escape exits.
The default is 1920×1080 fullscreen at 60 FPS.

Sector fog preserves billboard transparency. `sector_fog.fs` exposes `cameraPos`
and `u_density`; `sector_vhs.fs` exposes `time`, `renderSize`, and `noise_amount`.
The radio plays once after Z=100. After Z=150 the App enables a fixed native hard
saturator at drive 0.8. Ambient music streams automatically; radio uses managed
`Sound`. The native audio callback uses no Vyne calls, allocations, or locks.

Local assets required under `examples/assets/`: `wall.jpeg`,
`asphalt_road_3.jpg`, `ebil_qehbe.png`, `VCR_OSD_MONO_1.001.ttf`, `akira.wav`,
and `radio_log.wav`. Shader sources are committed; original media are ignored.

| Environment variable | Default | Purpose |
| :--- | :--- | :--- |
| `VYNE_SECTOR_FRAMES` | `0` | Interactive, or bounded frame count |
| `VYNE_SECTOR_WIDTH`, `VYNE_SECTOR_HEIGHT` | `1920`, `1080` | Initial resolution |
| `VYNE_SECTOR_AUDIO` | `1` | Set to 0 to skip audio initialization |
| `VYNE_SECTOR_WINDOWED` | `0` | Set to 1 for a window |
| `VYNE_SECTOR_AUTOPLAY` | `0` | Set to 1 for deterministic acceptance progression |

Autoplay uses scripted positions and simulation-clock jumps to exercise both
stalker freezes, radio, malfunction, and ending in 60 frames. Normal gameplay
uses real frame time. Runs report state counters and remaining resource handles.

## Acceptance tests

From the repository root:

```shell
python scripts/generate_vraylib.py --check
python examples/projects/raylib-showcase/tests/run.py
python examples/projects/raylib-showcase/tests/run.py --graphics
python examples/projects/raylib-showcase/tests/run.py --graphics --camera --audio
python examples/projects/raylib-showcase/tests/run.py --graphics --sector --audio
# Linux virtual-display and native sanitizer checks
xvfb-run -a python3 examples/projects/raylib-showcase/tests/run.py --graphics --sanitize
```

Core tests need no graphics/audio device. `--graphics` checks rendered pixels,
managed targets/uniforms/passes, and asset-independent Sector shaders. CPU tests
cover Sector state thresholds/freeze timing and saturation DSP. `--camera` and
`--sector` require their local assets; `--audio` opts into a real audio device.
Full Sector acceptance checks one radio play, a live saturation callback, the
ending sequence, and zero remaining native handles. `--sanitize` instruments
the native bridge and vendored library. `VYNEC` selects the compiler executable.

See the [project acceptance guide](https://github.com/t2ncay/vyne/blob/master/examples/projects/raylib-showcase/README.md)
and [Sector Shift details](https://github.com/t2ncay/vyne/blob/master/examples/graphics/sector_shift.md).

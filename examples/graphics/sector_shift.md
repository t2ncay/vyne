# Sector Shift / Sector 4 Incident

The original horror corridor now uses the managed external `vraylib` facade.
Its paired walls, stalker billboard, radio log, bodycam malfunction, blackouts,
recording overlay and "YOU NEVER EXISTED" ending are retained.

From the repository root:

```sh
vynec --run examples/graphics/sector_shift.vy
```

Controls: WASD movement, Shift sprint, Tab mouse capture, F11 fullscreen, Escape
exit. The default is 1920x1080 fullscreen at 60 FPS. Head bob, movement and final
fade use elapsed time; diagonal speed is normalized and collision slides along
the corridor walls. The three-second stalker blackout now actually freezes
movement/look. Audio and fade values stay bounded.

## Assets and rendering

Keep the original files together under `examples/assets/`:
`wall.jpeg`, `asphalt_road_3.jpg`, `ebil_qehbe.png`,
`VCR_OSD_MONO_1.001.ttf`, `akira.wav` and `radio_log.wav`.
They are already locally stored and ignored by Git. Paths resolve from the
repository working directory or the normal example/build executable locations.
Missing assets produce a named error and application cleanup.

Sector-specific shaders expose the uniforms the game actually uses:
`sector_fog.fs` has `cameraPos` and `u_density`; `sector_vhs.fs` has `time`,
`renderSize` and `noise_amount`. Shared camera-demo shaders remain available.
Fog preserves stalker PNG transparency and uses an identity model transform
for world-space billboard vertices. Managed targets follow window changes.

The fog-malfunction baseline is **0.28**, with a 0.1 oscillation, correcting the
old jump to 28. Radio playback triggers once after Z=100. After Z=150, a fixed
native hard saturator runs on mixed stereo output at drive 0.8. It uses a single
packed lock-free atomic configuration, no allocation or Vyne calls on the audio
thread, and is detached before application audio teardown. Ambient music loops
and updates automatically; the radio is a managed one-shot Sound.

## Bounded runs and verification

| Environment variable | Default | Purpose |
| --- | --- | --- |
| `VYNE_SECTOR_FRAMES` | `0` | Zero means interactive; otherwise limit frames |
| `VYNE_SECTOR_WIDTH`, `VYNE_SECTOR_HEIGHT` | `1920`, `1080` | Initial resolution |
| `VYNE_SECTOR_AUDIO` | `1` | Zero skips audio initialization |
| `VYNE_SECTOR_WINDOWED` | `0` | One starts windowed |
| `VYNE_SECTOR_AUTOPLAY` | `0` | Deterministic acceptance sequence, windowed/uncapped |

Autoplay uses scripted positions and simulation-clock jumps to exercise both
stalker freezes, radio, malfunction and ending in 60 frames. It defaults to a
60-frame limit when no limit is supplied. Normal gameplay uses real frame time.
Completion prints state counters and the remaining native handle count.

```sh
python examples/projects/raylib-showcase/tests/run.py --graphics --sector --audio
# CPU state/DSP tests and asset-independent shader tests are part of CI:
python examples/projects/raylib-showcase/tests/run.py --graphics
```

The suite checks story thresholds and freeze timing, one-shot radio state,
frame-independent bounded fades, real shader pixel changes, saturation bypass/
soft/hard/invalid-input behavior, and complete cleanup. The full local run also
asserts live audio, one radio play and an executed native saturation callback.

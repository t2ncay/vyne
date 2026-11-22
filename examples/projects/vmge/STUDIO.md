# Integrated scene editor

The scene workspace combines node authoring, resource map/terrain tools, the
Inter UI, asset browser, and isolated Play state.

## Files and document state

* **Ctrl+S / Save:** saves directly once a document has loaded or saved; otherwise
  opens the path dialog. **Ctrl+Shift+S / Save As:** always chooses a path.
* **Ctrl+N / New**, **Ctrl+O / Open**, and **Escape / Exit** prompt when dirty:
  **Save / Discard / Cancel**. Save failure retains dirty history, the pending
  action, its original Open path, and the current scene. A failed Save As stays
  focused for retry. Escape cancels the field/dialog and pending action.
* Escape in Play first releases captured mouse input. Escape in world settings,
  a stroke, or terrain preview cancels that operation for the whole frame.
* **Ctrl+I:** imports legacy DAT into the map resource, preserving scene nodes
  and terrain. **Ctrl+E:** chooses a geometry-only OBJ map export path;
  **Ctrl+Shift+E:** chooses a legacy DAT map export path. Exports contain
  **resource cells only**, omitting terrain and node meshes, and do not change
  the scene document path or clean/dirty revision.

The default path is `<project>/scene.vscene`, independent of the process working
directory. `VYNE_VMGE_SCENE` overrides it (relative overrides use the process
working directory). An absent default or explicit startup path intentionally
starts the demo with that proposed Save path; it is created only on Save. This
supports acceptance fixtures that intentionally name a new file. An existing
malformed/unreadable scene fails loading rather than silently replacing it.

Startup and field-16 Open/Save use extended world IO. Ordinary node-only files
keep the version-1 layout with roundtrip-safe numeric spellings; resources use version 2 and retain the
optional `G` generator metadata. Successful Open reloads world tools options.

Play uses grounded per-axis slope probes, retaining crouch/headroom and vertical
gravity/landing. `VYNE_VMGE_SPAWN_PAD=1` explicitly opts into the generated pad
spawn; the authored camera remains the default. This does not regenerate terrain.

The studio opts into managed deferred close requests. Titlebar close cancels any
unapplied preview/stroke, releases the mouse and uses the same document prompt.
Cancel keeps the window alive; explicit stop and acceptance frame limits remain
definitive. Other managed applications retain their original close behavior.

## Input and retained storage

`se_editor_input` dispatches modal-owned input once, before global input, caching
ownership even when Escape closes the modal. Integration uses UI state indices
34–43 (document actions, capture/reload flags and a modal dispatch counter).
Numeric state and one fixed 256-element pending path buffer are allocated before
frames. A preallocated passive UI state/numeric view renders disabled background
panels without mutating real scroll/focus state. Its browser uses the existing
cached directory, preventing draw-time rescans while captured.

World busy state guards history, files, Play, selection, orbit and splitters.
The world panel retains Preview Apply/Cancel, and existing strokes still process
release/cancellation. Bottom viewport view controls exclude node/gizmo picking
and new world stroke starts. Tooltips draw after background panels; the full
world modal draws last, after every background clip has ended.

## Verification

The maintained acceptance runner includes this fixture and both rendered studio
flows. For an isolated CPU check:

```powershell
$env:VYNE_RAYLIB_CACHE = "$PWD/build/studio-dev-raylib"
.\vynec.exe --compile -o build/studio-dev-test examples/projects/vmge/tests/studio.vy
$env:VYNE_VMGE_STUDIO_PREFIX = "$PWD/build/studio-dev-test"
$env:VYNE_POISON_ARENA = "1"
.\build\studio-dev-test.exe
```

`tests/studio.vy` is CPU/headless and emits **`VMGE_STUDIO_OK`**. Set
`VYNE_VMGE_STUDIO_PREFIX` to an absolute unique prefix with an existing parent
(runner suggestion: `BUILD / "studio"`). It writes `.vscene`, `-saved-as.vscene`,
`-ordinary.vscene`, `.obj`, and `.dat`. The `<prefix>-absent` directory must stay
absent: it supplies deterministic failed-save/load fixtures. Run with spawn pad
unset/0 and with `VYNE_VMGE_SPAWN_PAD=1` to cover both camera policies.

Checks include commit-level G roundtrip/options reload, dirty Open interception,
all pending-action cancellation, direct Save and Save As failure preservation,
retry/resume original Open, failed-load atomicity, map-only exports, authentic
Play slope/crouch/headroom probes, and 16 managed modal input frames with
simultaneous inspector focus, Escape cancellation, zero commits and zero handles.

For the actual editor/render loop, compile `src/main.vy` to `build/studio-dev`
and use the normal procedural/hidden-window acceptance environment with:

```python
env.update(VYNE_VMGE_MODE="scene", VYNE_VMGE_AUTOTEST="1",
           VYNE_VMGE_STUDIO_AUTOTEST="1", VYNE_VMGE_FRAMES="16",
           VYNE_VMGE_SCENE=str(BUILD / "studio-flow.vscene"),
           VYNE_VMGE_STUDIO_PREFIX=str(BUILD / "studio-flow"))
```

This emits **`VMGE_STUDIO_AUTOTEST_OK`**, followed by the usual
`VMGE_SCENE_EDITOR_STATE` and `VMGE_SCENE_EDITOR_COMPLETE` markers. It exercises
world Preview/Apply, stroke history, modal dispatch and whole-frame capture,
failed pending Save/cancel, extended disk Save/Open/options reload, isolated
Play/crouch, explicit map exports, resize, New and dirty Open cancel/discard.
Expect 16 frames, 5 nodes, 1 Play cycle, 0 errors, 0 commit drift, 0 live handles;
the scene is v2 with 2 resource cells, side 4 and `G|1` metadata. OBJ/DAT each
contain exactly those two resource cells (OBJ: 16 vertices, 12 faces, 2 groups).

Leave `VYNE_VMGE_STUDIO_AUTOTEST` unset/0 for ordinary acceptance: it retains
5 nodes, 1 Play cycle, and version-1 scene output. Both ordinary and extended
16-frame flows have passed locally with arena poisoning.

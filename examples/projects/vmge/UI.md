# VMGE editor UI

## Integration

The theme bundles unmodified **Inter 4.1 static Regular and SemiBold**, under
SIL OFL 1.1. License, upstream URL, archive paths, and SHA-256 hashes are in
`assets/fonts/`. The application owns both native fonts and releases them on
`App.close()`.

```vy
se_theme_load(app, project); // once, after window creation, before frames
ui := se_ui(edit_bytes);    // preframe, as before
se_ui_prepare(ui, directory_bytes); // optional preframe listing warmup
```

`se_draw_text(text, x, y, size, tint) -> Bool` is a drop-in replacement for
`vraylib.draw_text`. Widget signatures (`se_button`, `se_panel`, `se_field`,
`se_edit_begin`, `se_edit_input`) remain compatible. This uses
`App.load_font_size(path, 32)` and `Font.measure(text, size)` from the managed
runtime; atlases use its Latin/Extended glyph coverage and bilinear filtering.

Call **`se_ui_overlays(ui)` after toolbar/panels, before the modal dialog**, while
window drawing is active, for toolbar/asset action tooltips. `se_viewport`
automatically synchronizes selected T/R/S, Snap and Play styles. A caller that
does not use `se_viewport` can call `se_ui_toolbar_state(ui)` before its toolbar.
The existing main loop can continue committing `se_edit_input(ui)` through
`se_commit_edit` (or its scene integration wrapper); Tab advancement is handled
by the commit helper.

Call **`se_view_controls(ui, camera, viewport)` after presenting the scene target**
in window drawing to show Perspective/Top/Front/Side buttons at the viewport's
bottom-right. Gate node/gizmo picking and **new world-stroke starts** with
**`!se_view_controls_hit(ui, viewport)`**. `se_orbit` already handles preset
keyboard input and excludes this control strip from orbit/pan/zoom. The buttons
are hidden in Play and when the viewport is too small. Rendering never overlaps
the world's top toolbar. A button click updates the camera for the next rendered
scene target.
Continue processing releases/cancellation for drags that were already active.

Imports and image previews are wired into `se_asset_panel`; no extra main hook
is needed. All preview textures belong to `App`, so `App.close()` cleans them up.
`se_preview_invalidate(ui, app)` is also available to explicitly close/clear the
16-slot preview cache when switching projects or refreshing assets.

UI-owned `state` indices: 26 caret byte offset, 27 anchor byte offset, 28 selected
cached asset, 29 listing count, 30 Tab direction, 31 overflow flag, 32 refresh
request, 33 completed scan count. Indices 34–63 are available to integration.
Existing indices 0–25 retain their roles.
New preset/cache bookkeeping lives in separate preframe-owned buffers, leaving
`ui.state[34+]` and existing `ui.numbers` indices available to world integration.

## Editing and navigation

- Fields select their initial contents. Type to replace; Left/Right move across
  UTF-8 codepoints; Shift extends selection; Home/End jump to the ends.
- Ctrl+A/C/X/V select all, copy, cut, and paste. Backspace/Delete remove the
  selection or the adjacent whole codepoint. Single-line paste rejects control
  characters and oversize replacements before modifying the text.
- Enter applies; Escape cancels scene edits. Tab applies and advances to the
  next applicable inspector field; Shift+Tab reverses. Invalid values keep focus.
- Inspector sections group position, rotation in degrees, scale, mesh RGBA, and
  camera FOV. Mesh-only and camera-only fields appear on their relevant types.
- Hierarchy +/- controls collapse branches; searching temporarily reveals
  matching descendants. Search is live; Escape leaves the search filter in place.
  Use Ctrl+A then Backspace to clear it. Wheel scrolling is bounded to content.
- Assets are cached on navigation or Refresh, with folders before files. Search
  filters cached names live. Up, Project root, editable Directory, and clickable
  path components navigate; long breadcrumbs collapse to an Up shortcut.
- Click a file to select it, then **Assign to mesh** or **New mesh node**. New
  meshes use the filename stem and are parented under the selected node, or root
  when there is no selection. Scene files and legacy `.dat` maps have explicit
  Open/Import actions and the existing confirmation dialog.
- **Import into project** makes a project-owned GLB/image copy in `assets/imports`,
  navigates there, and selects it. Then Assign/New mesh stores a project-relative
  path. Name collisions choose `name-1.ext`, `name-2.ext`, etc.
- External Assign/New actions instead require a second deliberate **Confirm
  external** click, with an explicit non-portability message. Navigation, another
  file selection, or Refresh clears that confirmation. Changing the target node
  requires a new confirmation. They do not copy files.
- Selecting an image displays a cached, aspect-correct thumbnail in the asset
  sidebar. Selecting a model shows a model placeholder rather than a rendered
  3D thumbnail. Narrow browser layouts use an action strip without a thumbnail.
- Numpad **5** selects Perspective; **7** Top; **1** Front; **3** Side, while the
  mouse is over the editable viewport. Top/Front/Side use orthographic projection.
  Wheel zoom and middle-drag pan work in the active view's screen plane. Right-drag
  returns to the saved perspective orbit. Presets preserve the orbit target and
  perspective FOV; Top uses a non-collinear up vector for correct picking rays.

## Storage and import limits

All UI buffers and boxed headers are acquired before frames. The browser has
512 retained 255-byte UTF-8 filename slots, a parallel primitive kind array,
and primitive selection/scan state. Listings and display strings stay local to
the current frame. No directory-list arrays or transient strings are assigned
into retained UI objects. Capacity-limited listings announce the limit in their
footer. Paths are limited to 255 UTF-8 bytes by the existing scene path slots.

Refresh retries previously failed model loads through the existing asset cache
and closes every cached preview texture, then reloads the selected image once if
it still exists. It does not hot-reload successfully loaded models or watch disk
changes. Merely drawing previews or listings performs no file reads or loads.
Search folds ASCII case; non-ASCII spelling must match exactly.

Assets inside the project are stored with forward-slash relative paths.
**Import supports self-contained GLB 2.0 and PNG/JPEG/BMP only**, with nonempty
files limited to **8 MiB**. GLB validation checks the container length, aligned
JSON/BIN chunks, embedded buffer length, `asset.version`, complete JSON syntax,
and all URI properties, including Unicode-escaped keys. Only embedded `data:`
URIs are allowed. JSON chunks are limited to 1 MiB and 64 nesting levels. Image
imports check their file signature; full image decoding happens in the existing
loader/preview. This is a dependency/container check, not full glTF mesh-schema
validation or a guarantee that every glTF extension is supported by raylib.

Import uses native binary buffer reads and `vfs.write_bytes`, then verifies every
copied byte. It avoids `vfs.copy` (string-based, truncates at NUL) and
`vfs.read_bytes` (growing boxed arrays can lose earlier data with arena reuse).
All boxed byte-array growth finishes with zero placeholders before actual data
is filled; byte arrays never escape into retained UI. Imports run synchronously
on the explicit action. A failed write/read-back removes the incomplete new copy.
Collision resolution is serial to this editor; the native write API does not
reserve filenames against simultaneous writers in another process.

OBJ/glTF/IQM/M3D import is rejected with instructions to re-export an embedded
GLB or manually copy the complete dependency tree, preserving relative links.
There is no `.mtl`, glTF-buffer, texture-dependency copying or rewriting. Larger
assets can be placed in the project manually. External references remain absolute
after confirmation. Loading/material support follows the existing raylib loader.

The thumbnail cache retains **16 App-owned texture tokens** and fixed UTF-8 path
slots. Images are loaded only on explicit selection or selected-image Refresh,
resized to at most **128×128** before upload, and bilinear filtered. Selection
evicts the oldest slot on overflow. Preview files are limited to 8 MiB, decoded
images to 16 million pixels; failed/unsupported results are cached until refresh
or eviction. Full-resolution CPU decoding occurs once before the resize; the GPU
cache is bounded to at most 1 MiB of RGBA texels. It is not a model-preview renderer.

## Focused check

From the repository root, compile `tests/ui.vy` into a unique output, for example:

```powershell
.\vynec.exe --compile -o build/vmge-ui-local examples/projects/vmge/tests/ui.vy
$env:VYNE_POISON_ARENA = "1"
.\build\vmge-ui-local.exe
```

The hidden-window `tests/ui.vy` check verifies authentic proportional font metrics, UTF-8
selection/deletion, atomic paste rejection, fitting, clipping restoration,
folders-first caching, navigation/refresh invalidation, poisoned-frame retained
filenames, real mesh creation/assignment/history, failed-load atomicity, zero
commit drift, and native resource cleanup. It is independent of
`tests/run.py` and `build/vmge-tests`.

`tests/ui_assets.vy` is a separate focused hidden-window check for byte-exact
GLB/PNG ownership, source mutation independence, collision/no-overwrite behavior,
escaped external-URI and malformed-container rejection, 16-slot thumbnail
eviction/refresh/cleanup, preview pixels and zero draw-time loads/commit drift,
parallel orthographic rays and restored perspective FOV. Set `VYNE_UI_TEST_ROOT`
to a unique directory whose parent already exists; it creates a scratch project
and sources there, never in the real project:

```powershell
.\vynec.exe --compile -o build/vmge-ui-assets-local examples/projects/vmge/tests/ui_assets.vy
$env:VYNE_UI_TEST_ROOT = "$PWD/build/vmge-ui-assets-$([Guid]::NewGuid().ToString('N'))"
$env:VYNE_POISON_ARENA = "1"
.\build\vmge-ui-assets-local.exe
```

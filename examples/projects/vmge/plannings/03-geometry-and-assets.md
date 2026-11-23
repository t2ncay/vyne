# M03 — Chunk geometry, spatial queries, and textured assets

**Depends on:** M01; coordinate GPU upload integration with M04's managed mesh
support. **Status:** planned.

**Outcome:** reusable world meshes and spatial queries that make repeated lighting
and shadow passes affordable while retaining exact terrain/collision agreement.

## TODO

### Geometry construction

- [ ] **M03.01** Choose a measured fixed chunk size and capacities for vertices,
  indices, dirty queues, GPU mesh banks, and CPU query metadata.
- [ ] **M03.02** Build indexed terrain chunks using the existing exact triangle split,
  explicit positions/UVs/normals, and validated bounds.
- [ ] **M03.03** Build cube-cell chunk geometry; define when touching faces can be
  removed and how unsnapped/overlapping imported cubes are handled.
- [ ] **M03.04** Specify flat/smooth terrain normals and normal transformation under
  nonuniform node scale; keep geometric collision normals separate where needed.
- [ ] **M03.05** Add a checked managed mesh upload/update interface if required;
  mesh candidates must own validated CPU buffers through native upload completion.
- [ ] **M03.06** Rebuild only changed chunks and affected border normals after a
  brush stroke, generation, import, undo, redo, or runtime geometry edit.
- [ ] **M03.07** Stage complete mesh updates before replacing last-good GPU chunks;
  retain old geometry if allocation/upload fails.

### Visibility and query acceleration

- [ ] **M03.08** Add camera-frustum chunk/node culling using conservative bounds.
- [ ] **M03.09** Implement a bounded spatial index for cell/node overlap queries and
  ray picking; select grid/BVH layout using measured scene characteristics.
- [ ] **M03.10** Preserve exact rendered-triangle height/ray agreement and hit normals
  across chunk borders and outer terrain edges.
- [ ] **M03.11** Reuse spatial metadata for local-light selection, trigger overlap,
  character queries, and later shadow-caster selection.
- [ ] **M03.12** Define invalidation and query fallback during staged rebuilds.
- [ ] **M03.13** Keep collision and shadow-caster bounds valid for objects outside
  the visible camera frustum that can still affect the view.

### Texture/model workflow

- [ ] **M03.14** Add UV/material references for resource cells/terrain with a simple
  portable atlas or texture-set policy and explicit sampler costs.
- [ ] **M03.15** Retain self-contained GLB/image import, file-size validation,
  project ownership, duplicate-name handling, and visible import diagnostics.
- [ ] **M03.16** Expose mesh bounds, material slots, texture assignments, UVs, normals,
  and unsupported asset features in the inspector.
- [ ] **M03.17** Define simple collision proxies separately from visual mesh bounds;
  support static room geometry appropriate to the vertical slice.
- [ ] **M03.18** Add intentional nearest filtering, low-resolution textures, vertex
  snapping, or affine-style experiments as selectable material/effect examples.
- [ ] **M03.19** Release chunk/model/texture banks in dependency-safe order on World
  replacement, Stop, project close, and failed loads.

## Acceptance

- [ ] Chunk rendering/picking matches legacy resource geometry on reference scenes.
- [ ] Border sculpt/undo produces no cracks, stale normals, or stale collision hits.
- [ ] Typical/max worlds show measured submission/query improvement with bounded memory.
- [ ] Textured room assets survive save/reopen/relocation and run with procedural fixtures.
- [ ] Forced mesh-upload failure retains the last-good rendered world and leaks no handles.

## Implementation touchpoints

`src/engine/world_render.vy`, `src/engine/terrain.vy`, world motion/query modules,
`src/scene/assets.vy`, `src/ui/assets.vy`, and managed vraylib mesh operations.

| Completion evidence | Record |
| --- | --- |
| Commit / checks / artifacts / costs / limitations | Pending |

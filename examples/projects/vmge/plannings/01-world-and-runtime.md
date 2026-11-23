# M01 — World ownership and runtime/property foundations

**Depends on:** M00. **Status:** planned.

**Outcome:** an explicit World container coordinating geometry, entities, spatial
queries, environment references, and runtime state. Existing `SEWorld` remains a
compatible geometry payload until a deliberate migration is implemented.

## TODO

### Ownership and identity

- [ ] **M01.01** Specify World resource/session identity and its relationship to
  the scene hierarchy, geometry resource, asset registry, and active cameras.
- [ ] **M01.02** Define authored World configuration, immutable/shared assets,
  mutable Play World state, and temporary load/build candidates.
- [ ] **M01.03** Implement checked World creation/reset/copy/close operations with
  bounded retained owners and generation-safe entity/resource references.
- [ ] **M01.04** Expose the current cube/terrain payload through a compatibility
  adapter; keep geometry independent of scene-node capacity.
- [ ] **M01.05** Define update phases: input, behaviors, movement/collision,
  transform propagation, event dispatch, render preparation, drawing.
- [ ] **M01.06** Define event/deferred mutation buffers so adding/removing entities
  during update does not invalidate active iteration or retained references.

### Properties and editor integration

- [ ] **M01.07** Define stable property IDs, types, units, ranges, defaults,
  serialization rules, and inspector labels in a reusable checked schema.
- [ ] **M01.08** Add named accessors for World configuration and runtime queries;
  use schema APIs rather than scattering new raw state offsets.
- [ ] **M01.09** Add World selection/inspector integration while preserving the
  existing resource-selection and Add Node workflows.
- [ ] **M01.10** Keep beginner controls compact and expose detailed owner/budget
  state through an expandable advanced view.
- [ ] **M01.11** Define collision/query semantics independently from rendering
  visibility and future light/shadow masks.
- [ ] **M01.12** Add World dirty-revision and dependency invalidation tracking for
  geometry, behaviors, environment, and derived GPU caches.

### Persistence and Play

- [ ] **M01.13** Version World references/configuration and specify migration of
  existing v1-v4 scenes and historical DAT resource imports.
- [ ] **M01.14** Validate all resource/entity references in staging before Open
  replaces authoring; reject duplicate IDs and unsupported records.
- [ ] **M01.15** Extend history with bounded snapshots of mutable configuration
  and durable references to immutable assets.
- [ ] **M01.16** Start Play using independent World/entity/event owners; Stop clears
  runtime state and restores editor focus/capture without authoring changes.
- [ ] **M01.17** Specify single-active-World behavior for the first release and
  explicit World switching; leave future simultaneous worlds behind a clear API.
- [ ] **M01.18** Preserve atomic Save/Save As and pending Save/Discard/Cancel decisions.

## Acceptance

- [ ] Repeated Play/Stop and World replacement never revive stale references.
- [ ] A runtime-only entity/geometry/environment edit does not modify authoring.
- [ ] Undo/redo and old-scene reopening preserve existing mesh/terrain/material data.
- [ ] Missing resources, bad references, and exhausted event/entity capacity leave
  the last valid World intact.
- [ ] Owner/native-handle counts stabilize across repeated load and Play cycles.

## Implementation touchpoints

`src/engine/world.vy`, `src/scene/scene.vy`, `src/scene/play.vy`,
`src/scene/io.vy`, `src/scene/history.vy`, and the World inspector. New World
orchestration modules should have explicit ownership boundaries.

| Completion evidence | Record |
| --- | --- |
| Commit / checks / artifacts / costs / limitations | Pending |

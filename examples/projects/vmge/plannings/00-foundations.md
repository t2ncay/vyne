# M00 — Baseline, contracts, and measurable limits

**Depends on:** existing VMGE. **Status:** planned.

**Outcome:** a measured starting point and explicit contracts that allow the game,
teaching, and research features to grow together.

## TODO

### Baseline and evidence

- [ ] **M00.01** Run the existing CPU and graphics acceptance suites with arena poisoning.
- [ ] **M00.02** Record compiler revision, OS, GPU/driver, resolution, fixture counts,
  retained owner bytes, native handles, and elapsed/frame measurements.
- [ ] **M00.03** Capture the island, textured-mesh, daylight, VHS, and shader gallery
  scenes as baseline visual artifacts in runner-owned storage.
- [ ] **M00.04** Measure world submission/query costs at minimum, typical, and maximum
  resource sizes; distinguish raylib calls, GPU draws, and CPU query work.
- [ ] **M00.05** Measure current preframe/editor/history/publication-bank allocation
  separately from GPU targets, imported assets, and temporary evaluation storage.

### Design contracts

- [ ] **M00.06** Write a short architecture decision for World versus scene hierarchy
  versus geometry resource; preserve independent runtime and authored payloads.
- [ ] **M00.07** Define world units, handedness, transform composition, Euler units,
  front-face convention, camera near/far behavior, and screen/target orientation.
- [ ] **M00.08** Define linear/sRGB semantics for textures, colors, lighting, HDR
  targets, captures, loss functions, and final presentation.
- [ ] **M00.09** Define resource identity, version migration, atomic publication,
  native borrowing, invalidation, and dependency-release rules.
- [ ] **M00.10** Specify property identifiers/accessors for new state; inventory raw
  numeric offsets that should become named constants or checked APIs.
- [ ] **M00.11** Define behavior of optional instrumentation, quality profiles,
  capacity exhaustion, and last-good fallback.
- [ ] **M00.12** Publish a capability table for desktop OpenGL 3.3 and managed
  vraylib; identify depth, mesh-update, sampler, and profiling extensions needed.

### Incremental structure and budgets

- [ ] **M00.13** Introduce named state/property constants in touched subsystems with
  token/behavior review and existing regression checks.
- [ ] **M00.14** Identify input/update/render/resource-lifetime boundaries in editor
  orchestration; extract modules incrementally as their owning milestones land.
- [ ] **M00.15** Define separate admission limits for document owners, history,
  world chunks, program cache, targets, neural models, and experiment buffers.
- [ ] **M00.16** Plan how added environment/behavior resources participate in history
  without duplicating large immutable assets in all seventeen snapshots.
- [ ] **M00.17** Define measured benchmark budgets for the atmospheric vertical slice
  on at least one named reference machine; use profiles rather than universal FPS claims.
- [ ] **M00.18** Specify the first slice's rooms, player actions, assets, atmosphere,
  editable effect, inspection view, and fitting experiment before building systems.

## Acceptance

- [ ] Baseline artifacts and costs are reproducible from a clean checkout.
- [ ] Every new subsystem has a named owner, persistent identity, budget, and
  failure/publication policy.
- [ ] There is an agreed first playable acceptance scene and capability floor.
- [ ] The current engine's resource cleanup and legacy workflows still pass.

## Implementation touchpoints

`src/editor/scene_editor.vy`, `src/editor/workspace_host.vy`, `src/scene/scene.vy`,
`src/scene/history.vy`, `src/shader/`, and `tests/run.py`. Follow
[memory contracts](../README.md#memory-policy-contract) and
[shader host ownership](../SHADER_HOST.md#explicit-ownership-limits).

| Completion evidence | Record |
| --- | --- |
| Commit / checks / artifacts / costs / limitations | Pending |

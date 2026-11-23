# M13 — Portable game, laboratory, and research releases

**Depends on:** M08 for the first release. Later releases include verified M09-M12
capabilities. **Status:** planned.

**Outcome:** a clean, reproducible distribution with a small runtime, approachable
Studio, course bundles, and optional research tools using common resource formats.

## TODO

### Packaging and launch

- [ ] **M13.01** Define project launch metadata: starting World, input bindings,
  quality profile, resource roots, and optional experiment entry points.
- [ ] **M13.02** Package standalone runtime code plus reachable scenes, shaders,
  immutable materials, textures/models, environments, behaviors, audio, and models.
- [ ] **M13.03** Validate dependency closure and supported external asset trees;
  report unresolved references before producing a package.
- [ ] **M13.04** Separate runtime-required resources from optional Studio/lab/training
  data without creating divergent material/render implementations.
- [ ] **M13.05** Verify launch from a relocated directory, paths containing spaces,
  and clean working directories on supported desktop platforms.
- [ ] **M13.06** Make shader compilation/capability failure diagnostics useful to
  end users; expose explicit tested quality fallbacks and default settings.
- [ ] **M13.07** Include required fonts/assets/licenses and document dependencies
  and platform OpenGL/audio requirements.

### Developer/student experience

- [ ] **M13.08** Provide a small starter game and shader-first starter experiment
  using the same project/world/resource conventions.
- [ ] **M13.09** Document import mesh/texture, build World, assign Environment,
  add behavior/audio, Play, inspect, fit, publish, and export workflows.
- [ ] **M13.10** Add concise API documentation and architecture diagrams with
  algorithm/reference links beside the relevant implementations.
- [ ] **M13.11** Package a reproducible offline course bundle with verified lab
  versions, reset checkpoints, and instructor materials when M10 is complete.
- [ ] **M13.12** Package optional differentiable/neural experiment bundles with
  numerical/resource limits when M11/M12 are complete.
- [ ] **M13.13** Run usability walkthroughs for a game creator, a first-principles
  student, and a researcher; turn concrete friction into scoped follow-up issues.

### Maintenance and evidence

- [ ] **M13.14** Version public resource formats and APIs and publish migration
  notes for old scenes/materials/environments/graphs/models.
- [ ] **M13.15** Extend CI with applicable CPU, hidden-window GPU, lifetime/fault,
  relocation, and standalone walkthrough checks.
- [ ] **M13.16** Store representative render/metric artifacts with platform/driver
  information; separate algorithm correctness from device-dependent timing.
- [ ] **M13.17** Publish measured owner/GPU budgets, reference-machine performance,
  supported features, and known limitations for each quality profile.
- [ ] **M13.18** Verify clean shutdown and no persistent/native drift across long
  gameplay, transitions, editor experiments, and repeated project reloads.
- [ ] **M13.19** Release the M08 vertical slice with source/inspection entry points
  and a short demonstration of the shared game/teaching/research workflow.
- [ ] **M13.20** Repeat the release checklist for advanced environment/shadow,
  course, differentiation, and neural additions as they become independently ready.

## Acceptance

- [ ] A clean checkout builds and runs the documented sample/runtime/lab paths.
- [ ] A relocated game package includes every required asset and starts correctly.
- [ ] Course/research bundles state reproducibility and capability boundaries.
- [ ] Checked resource versions, migration, budgets, and failure behavior are documented.
- [ ] CI and release artifacts substantiate supported features and cleanup claims.

## Follow-up decision backlog

These require a separate scoped milestone and evidence before entering the release:

- [ ] **B01** Larger streamed worlds, LOD, asynchronous loading, and multi-World rendering.
- [ ] **B02** Skeletal animation, complex physics, navigation, and richer game tooling.
- [ ] **B03** Reflection probes, baked lighting, indirect illumination, and volumetrics.
- [ ] **B04** Temporal-history effects, motion vectors, antialiasing, and temporal shadows.
- [ ] **B05** Larger neural architectures, optional compute backend, and GPU training.
- [ ] **B06** Differentiable visibility/rasterization and inverse-geometry research.
- [ ] **B07** Full custom docking and editor extensibility beyond the small default workflow.

| Completion evidence | Record |
| --- | --- |
| Release / package checks / CI artifacts / walkthrough feedback / limits | Pending |

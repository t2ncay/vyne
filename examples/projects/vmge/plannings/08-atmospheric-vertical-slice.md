# M08 — First playable atmospheric vertical slice

**Depends on:** M02, M03, M05, M06, M07. **Status:** planned.

**Outcome:** one portable playable level demonstrates that game making, graphics
teaching, and shader experimentation operate on the same engine resources.

## TODO

### Content and game loop

- [ ] **M08.01** Build a small corridor and two connected rooms with project-owned
  textured meshes, a clear player route, and one simple interaction objective.
- [ ] **M08.02** Add a usable door, a trigger-controlled light/sound event, and a
  return/exit path using the reusable runtime behaviors.
- [ ] **M08.03** Add collision proxies, spawn/reset, interaction prompts, pause,
  and a readable minimal HUD.
- [ ] **M08.04** Author two reusable environment presets and an explicit triggered
  transition; continuous spatial blending is added in M09.
- [ ] **M08.05** Add a shadowed directional light and local lighting that establish
  a measurable atmosphere; expose the scene's lighting sources in inspection.
- [ ] **M08.06** Add one editable surreal effect such as low-resolution distortion,
  deliberate color quantization, vertex snapping, or animated material response.
- [ ] **M08.07** Include procedural replacements for imported demonstration media
  so acceptance can run without local/private asset dependencies.
- [ ] **M08.08** Document ownership/license/provenance for every distributed asset.

### Joined experiment and teaching views

- [ ] **M08.09** Make the scene runnable in Studio Play and the standalone launcher
  from the same starting-scene/project settings.
- [ ] **M08.10** Provide an inspection entry point that freezes a representative
  frame and opens light-space depth, visibility, HDR, and final-display views.
- [ ] **M08.11** Add an experiment manifest fixing camera/time/resolution/material
  versions for a selected room surface.
- [ ] **M08.12** Demonstrate material fitting using the existing scalar finite-
  difference/SGD path; freeze visibility and publish results only through Apply.
- [ ] **M08.13** Save a short student exercise: change one lighting/filter/effect
  algorithm, predict its result, capture the output, and explain the difference.
- [ ] **M08.14** Include a reset-to-known-scene workflow and safe editable shader copies.

### Acceptance and polish

- [ ] **M08.15** Measure the slice on the M00 reference machine at documented quality
  settings: frame costs, CPU submissions, owner/GPU bytes, shadow updates, and voices.
- [ ] **M08.16** Add a frame-scripted complete walkthrough covering movement,
  interaction, trigger, atmosphere change, reset, and shutdown.
- [ ] **M08.17** Add visual checks for material independence, shadow occlusion,
  color conversion, and the surreal effect at fixed sample points.
- [ ] **M08.18** Run Save/Open, relocation, repeated Play/Stop, failed transition,
  partial asset load, resize, and poisoned-frame lifetime acceptance.
- [ ] **M08.19** Prepare a short playable/inspectable demonstration for the professor
  and record where the workflow is confusing or requires engine knowledge.
- [ ] **M08.20** Turn concrete feedback into bounded follow-up tasks before broadening
  the scene or adding advanced lighting/research features.

## Acceptance

- [ ] The level is playable end-to-end with imported textures/meshes, door, trigger,
  audio, atmosphere change, real shadowing, and an editable distinctive effect.
- [ ] The same scene supports an inspectable graphics exercise and a repeatable
  parameter-fitting experiment.
- [ ] Defaults are usable, explicit limits hold, and last-good failure recovery works.
- [ ] A relocated project runs without Studio-only state or local ignored media.

| Completion evidence | Record |
| --- | --- |
| Commit / walkthrough / screenshots / experiment / costs / feedback | Pending |

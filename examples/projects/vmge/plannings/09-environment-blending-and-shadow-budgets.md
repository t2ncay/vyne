# M09 — Environment volumes and explainable light/shadow scheduling

**Depends on:** M08. **Status:** planned.

**Outcome:** larger atmospheric scenes with predictable transitions and visible,
authorable quality decisions. First-class explanation is part of the feature.

## TODO

### Spatial environment composition

- [ ] **M09.01** Add Environment Volume resources/nodes with bounded shapes,
  priority, blend distance, affected fields, and Environment references.
- [ ] **M09.02** Define resolver order: World defaults, spatial volumes, gameplay
  overrides, camera overrides; specify equal-priority tie-breaking.
- [ ] **M09.03** Blend compatible numeric settings with documented weights and
  interpolation domains, including linear color and logarithmic exposure where chosen.
- [ ] **M09.04** Define discrete sky/graph/resource switching separately from scalar
  parameter interpolation; never blend incompatible shader topology implicitly.
- [ ] **M09.05** Add an explicit crossfade option for incompatible pipelines only
  when both live banks fit the old/candidate/output budget.
- [ ] **M09.06** Use spatial queries to find active volumes and add stable entry/exit
  behavior during camera movement, teleport, pause, and World transitions.
- [ ] **M09.07** Show every contributor's priority, distance, weight, field override,
  and final resolved value in a "why this value?" inspector.
- [ ] **M09.08** Add depth-aware post fog using M04 depth with documented
  reconstruction and opaque/transparent/sky behavior.

### Advanced shadows

- [ ] **M09.09** Extend directional shadows to a measured cascade count with split
  controls, texel stability, transition blending, and coverage debug views.
- [ ] **M09.10** Add spotlight shadow maps using the existing cone/light contracts
  and a bounded atlas/target allocation policy.
- [ ] **M09.11** Evaluate point-light shadow representations, costs, and sampler
  support before selecting a baseline implementation.
- [ ] **M09.12** Implement the selected point-light path with explicit face/atlas
  scheduling, seam behavior, and failure-safe target ownership.
- [ ] **M09.13** Add optional soft/contact-hardening filtering only after visibility
  and depth/bias correctness; expose algorithm/sample cost and fallback quality.
- [ ] **M09.14** Reuse M03 spatial indices for caster selection and preserve
  off-camera contributions to visible receivers.

### Budgets and cache policy

- [ ] **M09.15** Separate stored lights, selected lights, shadowed lights, and updated
  shadow maps; expose capacities and actual counts independently.
- [ ] **M09.16** Define a shadow atlas/bank budget, per-frame update budget,
  per-light importance, desired quality, and minimum accepted quality.
- [ ] **M09.17** Define static/dynamic/realtime update policies and revisions for
  light transforms, casters, cutout materials, World chunks, and camera cascades.
- [ ] **M09.18** Add stable allocation and hysteresis so small camera changes do not
  continuously reassign maps or visibly oscillate quality.
- [ ] **M09.19** Define explicit stale-shadow, reduced-resolution, deferred-update,
  and unshadowed fallback policies; expose them to creators.
- [ ] **M09.20** Protect critical lights such as a player flashlight within the
  selected budget while documenting the cost of that priority.
- [ ] **M09.21** Record why each light's map was reused, invalidated, updated,
  resized, deferred, or denied; show byte/submission estimates and timing when available.
- [ ] **M09.22** Use reproducible scheduling for fixed inputs and document that
  GPU image precision and timing can vary across devices.

## Acceptance

- [ ] Moving between room/cave/outdoor volumes produces explained, stable blends.
- [ ] Incompatible effect transitions follow explicit switching/crossfade rules
  and do not exceed admitted live-bank memory.
- [ ] Cascades/spot/point examples demonstrate correct coverage and debug depth.
- [ ] A dense-light scene degrades according to visible policy rather than failing silently.
- [ ] Static maps stay cached; relevant changes update the correct maps within budget.
- [ ] Budget exhaustion, atlas resize, and invalid resources preserve a usable frame.

| Completion evidence | Record |
| --- | --- |
| Commit / volume demo / shadow demo / policy trace / measurements | Pending |

# M04 — Scene render graph and managed GPU resources

**Depends on:** M00, M01. **Status:** planned.

**Outcome:** typed, inspectable connections between scene/shadow/lighting/effect
passes, backed by checked resource ownership. The material-expression graph and
fullscreen pipeline remain usable through explicit adapters.

## TODO

### Managed resource foundation

- [ ] **M04.01** Inventory native target/depth/mesh capabilities and decide which
  checked vraylib operations must be added or extended.
- [ ] **M04.02** Add sampleable depth/shadow-target creation with explicit format,
  dimensions, attachment semantics, native ownership, and capability diagnostics.
- [ ] **M04.03** Expose borrowed depth views with session/generation checks and
  invalidate them on target resize/replacement/close.
- [ ] **M04.04** Define comparison sampling versus manual depth comparison for the
  OpenGL 3.3 baseline; validate supported sampler types and limits.
- [ ] **M04.05** Add depth preview/readback suitable for inspection with documented
  nonlinear-depth and linearization conventions.
- [ ] **M04.06** Track color, depth, sampler, target, and geometry dependencies in
  atomic release-bank preflight and failure cleanup.
- [ ] **M04.07** Preserve viewport, depth test/write, culling, blend, framebuffer,
  active texture, and program state across managed pass exits and exceptions.
- [ ] **M04.08** Extend mesh-upload/update checks required by M03 and test partial
  native construction cleanup before integrating it into World rendering.

### Graph model and validation

- [ ] **M04.09** Define stable pass/resource IDs, input/output types, access modes,
  attachment formats, size rules, clear/load operations, and ownership.
- [ ] **M04.10** Define scene-depth, shadow-depth, opaque-scene, sky, lighting,
  fullscreen-effect, tonemap, and presentation pass contracts.
- [ ] **M04.11** Define the initial opaque/alpha-cutout rendering path and explicit
  ordering for later blended transparency; include alpha-cutout shadow semantics.
- [ ] **M04.12** Validate missing producers, cycles, unsupported formats, cross-session
  handles, read/write feedback, and incompatible shader inputs before drawing.
- [ ] **M04.13** Produce a stable dependency schedule and a resource-use/lifetime plan
  with measured memory requirements before GPU allocation.
- [ ] **M04.14** Use separate authored, compiled, and last-good execution state;
  failed edits retain the runnable graph.
- [ ] **M04.15** Begin with explicit resource allocation; introduce aliasing only
  after lifetime proofs and instrumentation can demonstrate correctness.
- [ ] **M04.16** Wrap the existing fullscreen pipeline as compatible graph nodes
  without silently changing its alpha/orientation/bypass semantics.
- [ ] **M04.17** Declare engine-owned depth/color/light uniforms separately from
  editable material parameters and validate the complete active contract.

### Authoring and persistence

- [ ] **M04.18** Add a pass-list/resource-flow editor with connections, shader paths,
  formats, resolution policies, bypass, and validation diagnostics.
- [ ] **M04.19** Provide a default graph so existing scenes and simple games work
  without requiring graph authoring.
- [ ] **M04.20** Serialize versioned descriptors and durable resource references;
  retain no native tokens or execution wrappers on disk.
- [ ] **M04.21** Recompile/reallocate candidate graphs after resize or capability
  changes under explicit old-plus-candidate memory limits.
- [ ] **M04.22** Expose pass order, target sizes, resource dependencies, and errors
  for M07 inspection; add a fallback when GPU timers are unavailable.

## Acceptance

- [ ] Depth, opaque color, identity, HDR, fractional alpha, and display output have
  reference checks with balanced state and correct target orientation.
- [ ] Invalid graph connections fail before any draw or partial output publication.
- [ ] Resize/recompile/external borrowing failures retain a valid last-good graph.
- [ ] Graph Save/Open preserves topology and shader references across relocation.
- [ ] Fault injection and poisoned-frame churn leave zero live native handles.

## Implementation touchpoints

`src/shader/pipeline.vy`, `src/editor/workspace_host.vy`,
`src/scene/camera_effects.vy`, managed vraylib APIs/native runtime, and a new
scene-render-graph module with its own descriptors/compiler/executor boundaries.

| Completion evidence | Record |
| --- | --- |
| Commit / checks / artifacts / costs / limitations | Pending |

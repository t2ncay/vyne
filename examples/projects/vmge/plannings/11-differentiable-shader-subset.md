# M11 — Explicit differentiable shader expressions

**Depends on:** M07 and M05's validated material hooks. **Status:** planned.

**Outcome:** automatic derivatives for a documented numerical shader subset, with
independent gradient checks and reproducible fitting. The existing finite-difference
evaluator remains a reference and a useful black-box optimization path.

## TODO

### Scope and mathematical contract

- [ ] **M11.01** Define supported input/output shapes and scalar/vector parameter
  types, derivative seeds, loss channels, and parameter ID correspondence.
- [ ] **M11.02** Choose a first derivative representation using measured workload:
  forward-mode for a small parameter set or reverse-mode for a larger graph.
- [ ] **M11.03** Define operation rules for constants, parameters, add/subtract,
  multiply/divide, dot, mix, and selected continuous math operations.
- [ ] **M11.04** Define domain/derivative behavior for normalization, near-zero
  division, powers, clamps, branches, discontinuities, and nonfinite results.
- [ ] **M11.05** Publish a differentiability capability table. Arbitrary GLSL,
  rasterization, discrete visibility, resource switches, and hard shadows require
  explicit treatment and are not automatically differentiated by this subset.
- [ ] **M11.06** Hold geometry/camera/visibility fixed for the initial fitting
  workloads; identify exactly which material/light parameters receive gradients.
- [ ] **M11.07** Define optional fixed-LOD bilinear texture derivatives separately:
  coordinate derivatives and texel-weight derivatives have different resource costs.

### Typed representation and execution

- [ ] **M11.08** Extend or introduce a typed expression IR that preserves stable
  parameter/node IDs, source mappings, shapes, and validated operation metadata.
- [ ] **M11.09** Lower supported material-graph expressions into the IR and reject
  unsupported nodes with precise diagnostics; source-to-graph conversion is separate work.
- [ ] **M11.10** Implement bounded CPU forward evaluation and derivative kernels
  with explicit scratch/tape capacities and reusable buffers.
- [ ] **M11.11** Add derivative visualizations and node-level value/gradient inspection.
- [ ] **M11.12** Generate matching GLSL forward expressions and verify their output
  against the CPU numerical reference under the same inputs.
- [ ] **M11.13** Decide whether generated GPU derivative passes are warranted by
  measured costs; implement them only within the OpenGL capability/resource plan.
- [ ] **M11.14** Keep IR, derivative, optimizer, and material candidates separate
  from published authored/rendering state.

### Validation and fitting

- [ ] **M11.15** Compare derivatives against analytic scalar/vector examples and
  finite differences over an epsilon sweep away from nondifferentiable boundaries.
- [ ] **M11.16** Test broadcasting, shared subexpressions, parameter reuse, edge
  domains, masks, cancellation, and nonfinite rollback.
- [ ] **M11.17** Integrate supported derivatives with the existing bounded fitting
  interface, selected IDs/ranges, loss, optimizer state, and explicit Apply.
- [ ] **M11.18** Version checkpoints with derivative method, IR/schema/resource
  fingerprint, frozen context, optimizer state, and numerical settings.
- [ ] **M11.19** Compare CPU and optional GPU derivative results within stated
  precision limits; never require cross-driver bit equality.
- [ ] **M11.20** Provide a teaching exercise implementing the chain rule and a
  research example fitting a material response using the same expression resource.
- [ ] **M11.21** Record forward/gradient time, scratch/tape bytes, convergence,
  and differences from the finite-difference baseline.

## Acceptance

- [ ] Every supported operation has a documented derivative rule and independent check.
- [ ] Unsupported/nondifferentiable operations are diagnosed with explicit policy.
- [ ] A known continuous objective converges using verified derivatives.
- [ ] Checkpoint/resume and invalid-step rollback preserve the last valid result.
- [ ] The fitted result publishes as a normal material usable in the playable scene.

## Implementation touchpoints

`src/shader/graph.vy`, `src/shader/evaluation.vy`, `src/shader/material.vy`,
`voptim` where its APIs are applicable, and new checked numerical/IR kernels.
See [current graph/evaluation limits](../SHADER_MODEL.md).

| Completion evidence | Record |
| --- | --- |
| Operation table / gradient checks / fitting artifact / costs / limits | Pending |

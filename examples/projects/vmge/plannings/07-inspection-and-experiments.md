# M07 — Frame inspection and reproducible experiments

**Depends on:** M04, M05. **Status:** planned.

**Outcome:** creators and students can explain a frame, and researchers can freeze
the same scene as a measured experiment. Extend existing capture/evaluation APIs.

## TODO

### Render and value inspection

- [ ] **M07.01** Display executed pass order, resource producer/consumer links,
  formats, dimensions, lifetimes, and estimated allocation costs.
- [ ] **M07.02** Inspect a pass's actual last-good shader source, active uniforms,
  types, units, resource bindings, and source/include/node diagnostics.
- [ ] **M07.03** Add geometry views for positions, UVs, transformed normals, bounds,
  chunk IDs, and model/view/projection stages.
- [ ] **M07.04** Add depth, light-space position, shadow comparison, bias, visibility,
  direct diffuse/specular, ambient, HDR, and display debug outputs.
- [ ] **M07.05** Implement optional shader instrumentation or explicit diagnostic
  attachments for contribution values; do not infer them from final RGB alone.
- [ ] **M07.06** Add selected-pixel inspection with screen-to-target orientation,
  coordinate space, sample location, and capture-frame identity.
- [ ] **M07.07** Show the resolved environment/light values and their source/override
  history; extend to volume weights in M09.
- [ ] **M07.08** Capture actual CPU timings/submission counts and optional GPU timer
  queries; label unavailable GPU timing and avoid forced readback every normal frame.
- [ ] **M07.09** Bound instrumentation/readback queues and histories; disable their
  targets/work when inspection is inactive.

### Frozen experiment resources

- [ ] **M07.10** Define a versioned experiment manifest referencing the World,
  geometry/material/environment/render graph, camera, time, seed, and resolution.
- [ ] **M07.11** Freeze relevant resource versions or content hashes and record
  effective include/resource roots; detect changed dependencies before resuming.
- [ ] **M07.12** Define reset semantics that restore experiment state while keeping
  separately saved student/research shader documents available.
- [ ] **M07.13** Add capture/reference/difference workflows in linear space with
  selected channels, ROI/masks, and documented MSE/MAE definitions.
- [ ] **M07.14** Record platform/driver/precision metadata and use tolerances instead
  of claiming cross-device bit-identical GPU images.
- [ ] **M07.15** Add bounded batch execution for a parameter sweep or repeated
  camera/material test using runner-owned outputs and immutable inputs.
- [ ] **M07.16** Expose the existing finite-difference/SGD API in a simple experiment
  control: selected scalar parameters, ranges, step size, rate, stop, and loss history.
- [ ] **M07.17** Keep candidates separate from authored materials; explicit Apply
  publishes a validated result through normal material/history workflows.
- [ ] **M07.18** Add checkpoint/resume, divergence/NaN diagnostics, cancellation,
  and rollback of the complete experiment state on a failed step.

### Teaching/research workflow

- [ ] **M07.19** Provide a minimal example whose diffuse factor can be calculated by
  hand and checked against an instrumented pixel.
- [ ] **M07.20** Provide a gain/material-fitting example that reduces a documented
  loss against a reference with fixed camera/geometry/visibility.
- [ ] **M07.21** Add compact default views plus expandable advanced data, formula,
  source, and resource explanations.
- [ ] **M07.22** Export an experiment bundle containing references, sources, settings,
  metrics, and enough provenance for another student/developer to reproduce it.

## Acceptance

- [ ] A selected reference pixel's lighting/shadow/display result is traceable to
  actual recorded inputs and explicit diagnostic contributions.
- [ ] Freeze/reset/reopen preserve experiment inputs and detect stale dependencies.
- [ ] Parameter fitting improves a known objective without modifying authoring until Apply.
- [ ] Checkpoint resume and failed/cancelled steps preserve valid previous state.
- [ ] Normal Play incurs no hidden continuous readback or unbounded inspection storage.

## Implementation touchpoints

`src/shader/evaluation.vy`, `src/editor/shader_workspace.vy`,
`src/ui/code_editor.vy`, render-graph instrumentation, and scene/material publication.
See [existing evaluation API](../SHADER_MODEL.md#evaluation-evaluationvy).

| Completion evidence | Record |
| --- | --- |
| Commit / checks / artifacts / costs / limitations | Pending |

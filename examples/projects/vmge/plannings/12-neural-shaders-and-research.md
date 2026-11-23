# M12 — Bounded neural shaders and playable research

**Depends on:** M08, M11. **Status:** planned.

**Outcome:** compact learned materials/effects with inspectable CPU/GPU inference,
reproducible training/fitting, and an explicit path into the real game runtime.

## TODO

### Model format and numerical reference

- [ ] **M12.01** Select an initial small MLP workload: learned material/light
  response over fixed continuous inputs, with a procedural analytic reference.
- [ ] **M12.02** Define a versioned model resource with architecture, activations,
  input/output semantics, normalization, weights/biases, precision, and provenance.
- [ ] **M12.03** Set explicit layer/width/parameter/weight-byte/inference-work limits
  using reference-machine measurements and project resource admission checks.
- [ ] **M12.04** Validate tensor shapes, finite parameters, supported activations,
  format versions, and bounded file lengths in staging.
- [ ] **M12.05** Implement a bounded CPU inference reference using reusable buffers
  and compare selected model outputs against independently calculated examples.
- [ ] **M12.06** Define input normalization/coordinate-space/color-space contracts
  so model assets remain meaningful outside their training scene.

### GPU/runtime inference

- [ ] **M12.07** Choose uniform/texture weight storage for the baseline and account
  for managed sampler slots, target dimensions, uploads, and precision.
- [ ] **M12.08** Generate or author a checked GLSL inference implementation for the
  supported architecture, with source mappings and typed resource bindings.
- [ ] **M12.09** Verify CPU/GPU inference parity within precision tolerances over
  both training-range and out-of-range inputs.
- [ ] **M12.10** Expose neural inference through material-response or effect nodes
  that compose with the normal WorldEnvironment/render graph.
- [ ] **M12.11** Stage weight/program replacements and preserve the last-good model
  if loading, compilation, binding, or budget validation fails.
- [ ] **M12.12** Define explicit artist controls and optional analytic fallback
  behavior for unavailable or rejected model resources.

### Training bridge and inspection

- [ ] **M12.13** Use CPU training/fitting as the initial supported path; identify
  any optional external trainer format without assuming OpenGL 3.3 compute support.
- [ ] **M12.14** Define training samples/reference captures, masks, frozen inputs,
  loss, optimizer, parameter constraints, seeds, and stop/cancel rules.
- [ ] **M12.15** Reuse M11 supported differentiation or validated model-specific
  gradients; check shared layers/activations with finite differences.
- [ ] **M12.16** Save full training/model checkpoints and resource fingerprints;
  resume only when architecture/normalization/reference contracts match.
- [ ] **M12.17** Show network input/output, selected layer activations, loss history,
  gradients, analytic-versus-learned differences, and measured cost.
- [ ] **M12.18** Publish a validated trained model as an immutable game resource,
  requiring no optimizer or training data to run ordinary Play.

### Novel examples and evidence

- [ ] **M12.19** Fit a learned surface response to an analytic/procedural target and
  report error over a held-out sample set.
- [ ] **M12.20** Integrate that model into the atmospheric level and compare its
  visual result, runtime cost, and memory against the analytic implementation.
- [ ] **M12.21** Add one clearly scoped novel example, such as an unusual learned
  attenuation response or dreamlike coordinate/color transform.
- [ ] **M12.22** Keep approximate rendering claims measurable: report what the model
  approximates, its training domain, failure cases, and resource tradeoffs.
- [ ] **M12.23** Export research bundles with reference sources, model/version,
  dataset provenance, training settings, metrics, and demonstration scene.
- [ ] **M12.24** Decide on larger networks, GPU training, learned indirect lighting,
  or differentiable visibility only after documenting evidence and backend requirements.

## Acceptance

- [ ] A bounded model trains/fits, checkpoints, reopens, and produces verified
  CPU/GPU outputs under the declared input/color/precision conventions.
- [ ] Another developer can reproduce the reference experiment from its bundle.
- [ ] The published model runs in the standalone game without training-only owners.
- [ ] Invalid shape/weights/program/budget requests retain the last-good shader/model.
- [ ] Artistic controls and debug views remain usable for students and game creators.

| Completion evidence | Record |
| --- | --- |
| Model schema / parity checks / training metrics / playable demo / costs | Pending |

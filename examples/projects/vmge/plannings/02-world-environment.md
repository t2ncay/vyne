# M02 — Environment resources and the color pipeline

**Depends on:** M01. **Status:** planned.

**Outcome:** a reusable Environment resource and WorldEnvironment authoring node
with coherent sky/ambient/exposure/effect defaults and camera override behavior.
Spatial blending arrives in M09; first establish one predictable resolved environment.

## TODO

### Resource and resolution

- [ ] **M02.01** Define a versioned Environment schema: background/sky, ambient,
  exposure, tonemapping, fog configuration, effect references, and quality policy.
- [ ] **M02.02** Define resource IDs, retained storage limits, immutable publication,
  and editable-copy behavior consistent with material/shader resources.
- [ ] **M02.03** Add WorldEnvironment to the node catalog and inspector as a
  reference to an Environment resource.
- [ ] **M02.04** Resolve World defaults and explicit camera overrides with per-field
  enable flags; validate conflicting/default selections deterministically.
- [ ] **M02.05** Adapt existing Sky/light ambient/camera-pipeline properties into
  the new model with documented compatibility precedence.
- [ ] **M02.06** Add create/open/save/duplicate/assign/reset environment workflows
  with checked resource publication and one logical history step per edit.
- [ ] **M02.07** Share the resolved result between editor camera preview, Play,
  Rendering camera input, and standalone runtime.

### Color correctness and basic atmosphere

- [ ] **M02.08** Mark albedo/emission color textures as color data and normals,
  roughness, depth, and learned weights as linear numeric data.
- [ ] **M02.09** Implement explicit color decode/encode boundaries appropriate to
  the managed backend; prevent double sRGB conversion.
- [ ] **M02.10** Define whether editor color pickers display encoded colors while
  resource shader uploads use linear values; document conversions.
- [ ] **M02.11** Accumulate lighting in linear HDR and apply display conversion once.
- [ ] **M02.12** Provide simple exposure and tonemapping choices with editable GLSL,
  defaults, units, and linear capture before display conversion.
- [ ] **M02.13** Add basic world-distance/height fog in the scene shading path;
  reserve depth-aware post fog for managed depth/render graph support.
- [ ] **M02.14** Define sky/ambient behavior with no lights, hidden Sky, a missing
  linked sun, or an explicitly disabled effect pipeline.
- [ ] **M02.15** Add a raw-HDR/linear/display comparison view and numeric color readout.

### Persistence and isolation

- [ ] **M02.16** Persist environment references, override flags, and exact numeric
  values; never serialize native shader/target handles.
- [ ] **M02.17** Snapshot configuration/references for undo/redo and copy mutable
  runtime overrides into Play owners.
- [ ] **M02.18** Retain the working environment on malformed resources, compile
  failures, unresolved references, or exceeded target/owner budgets.
- [ ] **M02.19** Add bounded Daylight, Dim Corridor, and Dream Room starting presets
  whose implementations remain editable.

## Acceptance

- [ ] Known linear color patches produce expected decode/light/tonemap/encode
  values within documented GPU tolerances.
- [ ] Assigning/reopening a preset gives the same camera-preview and Play result.
- [ ] Camera overrides show the resolved value and source without changing World defaults.
- [ ] Missing/invalid resources and failed Save leave the previous environment usable.
- [ ] Legacy scenes load with their documented previous appearance/compatibility policy.

## Implementation touchpoints

`src/scene/environment.vy`, `src/scene/camera_effects.vy`, `src/scene/catalog.vy`,
scene extension persistence, `shaders/environment/`, and `shaders/camera/`.
Environment resource/API names introduced here are new design work.

| Completion evidence | Record |
| --- | --- |
| Commit / checks / artifacts / costs / limitations | Pending |

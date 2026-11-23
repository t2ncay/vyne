# M05 — Programmable lighting and first directional shadows

**Depends on:** M02, M03, M04. **Status:** planned.

**Outcome:** understandable direct lighting with a selectable material response,
bounded local-light selection, and a correct single directional shadow map.

## TODO

### Material and light contracts

- [ ] **M05.01** Define named material inputs: albedo, normal, roughness, metallic,
  emission, alpha/cutout, and optional stylized controls, with defaults and units.
- [ ] **M05.02** Preserve a simple Lambert material for first-principles labs; add
  an optional documented specular model and a bounded PBR starter implementation.
- [ ] **M05.03** Expose editable material-response and attenuation hooks while
  keeping engine-owned geometry/light/depth inputs typed and validated.
- [ ] **M05.04** Use explicit geometric/transformed normals; make two-sided
  shading intentional instead of relying on view-facing derivative normals everywhere.
- [ ] **M05.05** Add Spot Light catalog/inspector/runtime support with direction,
  cone angles, range, intensity, and smooth attenuation controls.
- [ ] **M05.06** Define light color-space/intensity/range semantics and separate
  artistic attenuation from an optional documented physical approximation.
- [ ] **M05.07** Separate stored World light capacity from active shader light capacity;
  select intersecting lights using M03's spatial queries.
- [ ] **M05.08** Add stable priorities and explicit capacity diagnostics; avoid failing
  an entire scene simply because more lights are stored than one draw can receive.
- [ ] **M05.09** Define material/receiver masks and independent shadow-caster masks.
- [ ] **M05.10** Resolve sampler usage for albedo, material maps, and shadows against
  managed limits; add validated extensions or explicit reduced-feature variants.

### First shadow algorithm

- [ ] **M05.11** Render one directional light's caster depth with the managed shadow
  target and render-graph pass from M04.
- [ ] **M05.12** Define world-to-light clip/texture transformation, depth encoding,
  near/far bounds, outside-map behavior, and receiver comparison convention.
- [ ] **M05.13** Fit the orthographic shadow volume to the relevant region and include
  off-camera casters whose shadows can reach visible receivers.
- [ ] **M05.14** Stabilize the projection with texel-aligned movement and expose
  bounds/resolution debug overlays before adding cascades.
- [ ] **M05.15** Implement constant/slope/normal bias with documented units and
  inspectable comparison values for acne and detached-shadow diagnosis.
- [ ] **M05.16** Implement hard comparison first, then a small fixed PCF kernel;
  expose filter radius/sample cost with a deterministic sample pattern.
- [ ] **M05.17** Apply visibility to the correct direct-light contribution while
  retaining the separately defined ambient/indirect term.
- [ ] **M05.18** Respect cast/receive flags, alpha cutout, sidedness, and transparent
  material limitations consistently in scene and shadow passes.
- [ ] **M05.19** Cache unchanged shadow data with geometry/light revision tracking;
  dynamic casters invalidate only relevant maps.
- [ ] **M05.20** Retain the previous shadow/lighting program bank on failed compilation,
  allocation, graph edit, or incompatible uniform contract.

### Editor and persistence

- [ ] **M05.21** Add Light/Shadow inspector groups with simple defaults and expanded
  algorithm settings, masks, limits, and effective resource costs.
- [ ] **M05.22** Expose light view, raw/linearized depth, receiver depth, bias,
  visibility, normals, and diffuse/specular contribution debug views.
- [ ] **M05.23** Persist material/light/shadow settings through the existing checked
  schema/resource path and include them in undo and Play copying.
- [ ] **M05.24** Add a compact corridor scene plus a plane/cube/slope shadow
  correctness scene with predictable light/camera positions.

## Acceptance

- [ ] CPU reference values and GPU diffuse/attenuation/visibility samples agree
  within stated tolerances on simple geometry.
- [ ] Shadow depth comparisons and bias units are explainable from captured values.
- [ ] Camera movement avoids gratuitous shadow shimmer; off-camera casters remain correct.
- [ ] Moving a caster/light updates its shadow; static frames reuse cached data.
- [ ] Multiple mesh instances retain independent material values and flags.
- [ ] Mesh/terrain/cells all participate consistently, with measured submission costs.

## Implementation touchpoints

`src/scene/environment.vy`, scene property/catalog/IO modules,
`shaders/environment/lit.vs`, `shaders/environment/lit.fs`, material publication,
and M04's render-graph/depth-target support.

| Completion evidence | Record |
| --- | --- |
| Commit / checks / artifacts / costs / limitations | Pending |

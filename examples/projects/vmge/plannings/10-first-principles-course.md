# M10 — Algorithms-from-first-principles graphics course

**Depends on:** M07, M08. Starter lessons can begin during M02-M05.
**Status:** planned.

**Outcome:** a technical-elective lab sequence using a real, compact engine.
Students implement and explain algorithms while retaining meshes, textures, World
authoring, game behavior, and the ability to turn experiments into playable scenes.

## TODO

### Course contract and lab tooling

- [ ] **M10.01** Agree prerequisites, learning outcomes, lab duration, supported
  machines, and assessment criteria with the professor.
- [ ] **M10.02** Provide one-command setup with procedural assets, known compiler
  revision, capability checks, and a first rendered frame.
- [ ] **M10.03** Define a lab bundle: starter scene, shader/kernel, objective,
  fixed inputs, expected observations, optional hints, and reset checkpoint.
- [ ] **M10.04** Keep starter and student-authored resources separate; reset/reload
  must preserve a saved student's work and make publication history understandable.
- [ ] **M10.05** Add expandable coordinate/formula/resource panels and readable
  diagnostics without requiring students to navigate raw engine state arrays.
- [ ] **M10.06** Supply tiny CPU reference kernels for selected numerical/raster
  algorithms; clearly distinguish reference code from fixed-function GPU behavior.
- [ ] **M10.07** Validate labs numerically with tolerances and structured metrics;
  use visual interpretation and explanations alongside pixel comparisons.
- [ ] **M10.08** Document every lab's algorithm assumptions, edge cases, coordinate
  spaces, color spaces, and cost model.

### Lab sequence

- [ ] **M10.09 — Sampling and images:** implement UV mapping, nearest/bilinear
  sampling experiments, procedural patterns, and a texture-coordinate debug view.
- [ ] **M10.10 — Transformations:** derive model/view/projection, homogeneous
  coordinates, camera basis, and perspective division with inspectable vertices.
- [ ] **M10.11 — Rasterization reference:** implement triangle coverage and
  barycentric interpolation in a small CPU kernel and compare selected samples.
- [ ] **M10.12 — Perspective and depth:** implement perspective-correct
  interpolation/depth examples and explain affine-style artifacts and occlusion.
- [ ] **M10.13 — Normals:** derive geometric/transformed normals, inverse-transpose
  behavior, flat/smooth shading, and nonuniform-scale failure examples.
- [ ] **M10.14 — Diffuse lighting:** implement dot-product response and bounded
  point/spot attenuation; verify a hand-calculated pixel.
- [ ] **M10.15 — Specular and materials:** implement a simple specular model,
  compare roughness responses, and discuss energy/color assumptions.
- [ ] **M10.16 — Shadow mapping:** construct light-space depth, visibility
  comparisons, bias, and PCF; diagnose acne, detached shadows, and aliasing.
- [ ] **M10.17 — Filtering:** implement separable blur/bloom passes and compare
  sample counts, target sizes, edge behavior, and measured costs.
- [ ] **M10.18 — HDR and display:** implement exposure/tonemapping, demonstrate
  linear versus encoded arithmetic, and compare pre/post-display captures.
- [ ] **M10.19 — Procedural worlds:** implement seeded noise/fBm and relate generated
  height samples, mesh triangles, normals, and collision queries.
- [ ] **M10.20 — Inverse graphics:** define a simple loss and finite-difference
  gradient; fit a scalar material and compare against M11 AD when available.
- [ ] **M10.21 — Student project:** implement an original material/light/effect
  algorithm and integrate it into a small playable atmospheric scene.

### Instructor delivery and feedback

- [ ] **M10.22** Prepare instructor solutions/rubrics, known failure examples,
  recorded expected metrics, and dependency diagrams separately from starter bundles.
- [ ] **M10.23** Pilot the initial transform/diffuse/shadow labs with students and
  measure setup time, first successful edit, and explanation/debugging difficulty.
- [ ] **M10.24** Improve concrete UI/API/documentation friction found in the pilot.
- [ ] **M10.25** Package labs with platform limitations, offline assets, source
  references, and reproducible version information.
- [ ] **M10.26** Make neural/differentiable labs optional advanced units once M11/M12
  are verified, preserving a complete first-principles core course.

## Acceptance

- [ ] Students can set up, render, edit a shader, inspect values, and reset an
  experiment through the documented small workflow.
- [ ] Each required lab has a starter, reference observations/checks, explanation
  exercise, and instructor assessment material.
- [ ] Numerical/reference implementations and GPU paths state their differences honestly.
- [ ] Student projects reuse the same game assets, material publications, and runtime.
- [ ] Pilot feedback demonstrates that inspection helps students explain algorithms.

| Completion evidence | Record |
| --- | --- |
| Lab versions / pilot notes / platform checks / assessment artifacts | Pending |

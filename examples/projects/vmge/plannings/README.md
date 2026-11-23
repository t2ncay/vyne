# VMGE: programmable worlds, atmospheric games, and graphics experiments

**Vision:** a compact, inspectable game engine where playable scenes, graphics
algorithms taught from first principles, and novel shader research use the same
resources and rendering systems.

**Created:** 2026-11-23. **Status:** proposed implementation roadmap.
Checkboxes track future work; existing capabilities are described separately below.

## What success looks like

- A creator imports textured meshes, builds a small world, adds interactions and
  sound, configures its atmosphere, and exports a playable game.
- A student opens that game's lighting or shadow implementation, changes an
  algorithm, and observes its inputs, intermediate values, and output.
- A researcher freezes the same scene, measures a loss, fits supported parameters,
  or evaluates a neural material, then publishes the result back into the game.
- The default workflow stays approachable; deeper control and instrumentation are
  available through progressive disclosure.

The first target is an indie-horror/dreamlike low-poly vertical slice: rooms,
textured geometry, deliberate lighting, sound, transitions, and a distinctive
editable effect. Its unusual rendering should be an intentional artistic choice.

## Existing starting point

VMGE already has a shared Scene/World/Shaders/Rendering studio, mesh/image import,
per-node materials, isolated Play/Stop, character collision, atomic scene saves,
undo/redo, terrain tools, typed GLSL controls, a material graph, multipass effects,
linear float captures, and frozen finite-difference/SGD evaluation APIs.

The current baseline has 128 scene slots, a single `SEWorld` geometry resource
with 4,096 cells and a 65-by-65 height grid, one selected directional light, four
visible point lights, and a desktop OpenGL 3.3 renderer. Terrain/cells use bounded
linear submissions and queries. The shared publication bank has eight slots;
the runtime supports four managed sampler bindings per program. Editor retained
owners have a 128-MiB admission check, separate from asset/target budgets.

The existing material graph generates GLSL expressions; the existing pipeline
orders fullscreen effects. A broader scene render graph, sampleable shadow depth,
automatic differentiation, neural inference, environment volumes, and a reusable
gameplay behavior system are planned capabilities.

See [engine overview](../README.md), [world resources](../WORLD.md),
[environment](../ENVIRONMENT.md), [shader APIs](../SHADER_MODEL.md), and
[host integration](../SHADER_HOST.md) for the implemented contracts. The current
acceptance runner lists 17 CPU and 10 graphics fixtures plus integrated flows.

## Milestone checklist

| Done | ID | Deliverable | Depends on |
| --- | --- | --- | --- |
| [ ] | [M00](00-foundations.md) | Baseline, budgets, architectural contracts, measurements | Existing engine |
| [ ] | [M01](01-world-and-runtime.md) | Explicit World ownership and extensible runtime/property model | M00 |
| [ ] | [M02](02-world-environment.md) | Environment resources, WorldEnvironment, linear/HDR conventions | M01 |
| [ ] | [M03](03-geometry-and-assets.md) | Chunk meshes, spatial queries, textured world geometry | M01 |
| [ ] | [M04](04-render-graph-and-resources.md) | Typed scene render graph and managed depth/mesh resource support | M00, M01 |
| [ ] | [M05](05-lighting-and-first-shadows.md) | Programmable materials/lights and first directional shadows | M02, M03, M04 |
| [ ] | [M06](06-gameplay-and-audio.md) | Behaviors, interactions, triggers, audio, runtime launcher | M01; M02 for atmosphere actions |
| [ ] | [M07](07-inspection-and-experiments.md) | Frame/pixel inspection and reproducible experiment workflow | M04, M05; extend existing evaluation |
| [ ] | [M08](08-atmospheric-vertical-slice.md) | Playable level joining game making, inspection, and fitting | M02, M03, M05, M06, M07 |
| [ ] | [M09](09-environment-blending-and-shadow-budgets.md) | Environment volumes, advanced shadows, explainable scheduling | M08 |
| [ ] | [M10](10-first-principles-course.md) | First-principles lab sequence and instructor/student tooling | M07, M08; starter labs can begin at M02 |
| [ ] | [M11](11-differentiable-shader-subset.md) | Explicit differentiable expression subset and gradient validation | M07; validated material hooks from M05 |
| [ ] | [M12](12-neural-shaders-and-research.md) | Bounded neural shader inference, training bridge, playable research examples | M08, M11 |
| [ ] | [M13](13-distribution-and-release.md) | Portable game/lab/research distribution and maintained release | M08; later feature releases include M09-M12 |

### Execution order

```text
M00 -> M01 -> M02 ---------------------------+
          |-> M03 --------------------------+-> M05 -> M07 --+
          |-> M04 --------------------------+               +-> M08
          +-> M06 -----------------------------------------+

M08 -> M09           atmospheric-world/lighting development
M07 + M08 -> M10     course development
M07 -> M11 -> M12    differentiable/neural research
M08 -> M13           first portable release; repeat for later capabilities
```

M03 geometry/query work, M04 resource/graph work, and the core M06 work can proceed
in parallel once M01's contracts are stable. M03's GPU chunk-upload integration
uses the managed mesh support delivered jointly with M04. M08 uses the
**existing finite-difference API** for its fitting
demonstration; it does not depend on automatic differentiation or neural training.
Course examples and debugging views should be developed alongside their algorithms.

## Shared implementation rules

1. **One resource path:** game, teaching, and research workflows consume the same
   scene, material, environment, and render-pass definitions.
2. **Three distinct graphs:** material expressions describe shader calculations;
   render graphs connect passes/resources; differentiation operates on an explicit
   supported numerical subset. Conversions and boundaries must be documented.
3. **Explicit ownership:** retained owners and bounded caches have admission checks;
   frame-local wrappers/text never become retained state. GPU ownership is separate
   from arena lifetime.
4. **Failure-isolated publication:** validate separate staging, preflight release,
   publish complete resources, and retain last-good state on failure.
5. **Stable persistence:** serialize durable IDs/paths/values, version formats, and
   migrate old scenes. Native tokens, sessions, and transient descriptors stay local.
6. **Artist control plus explanations:** budgets, defaults, overrides, and degradation
   policies are inspectable. Control precedence and resource costs are visible.
7. **Evidence-backed limits:** measure new memory and submission costs. The existing
   owner budget is already tight; avoid extending every history snapshot blindly.
8. **Correctness before sophistication:** establish coordinate/color/depth contracts
   and reference checks before adding advanced filtering or learned approximations.
9. **Progressive interfaces:** ready-to-use defaults, editable implementations, and
   advanced instrumentation are levels of access to the same engine.

## Definition of done for every milestone

- [ ] Implement the milestone's core checklist and document explicit feature limits.
- [ ] Demonstrate its acceptance scene/experiment with reproducible launch steps.
- [ ] Verify relevant CPU invariants and actual GPU pixels using stated tolerances.
- [ ] Exercise invalid input, capacity exhaustion, partial failure, and cleanup.
- [ ] Check authored/Play isolation, Save/Open, undo/redo, and resize where applicable.
- [ ] Record owner bytes, GPU bytes, geometry submissions, and measured frame costs.
- [ ] Run the applicable existing acceptance suites; record compiler/platform details.
- [ ] Update this index and the milestone's evidence table only after verification.

## Existing acceptance commands

Run from the repository root. These are existing commands, not future runner flags.

```shell
python examples/projects/vmge/tests/run.py --poison
python examples/projects/vmge/tests/run.py --graphics --poison
python vyne/runtime/tests/shader_run.py
python vyne/runtime/tests/run_vraylib_blockers.py --font examples/projects/vmge/assets/fonts/Inter-Regular.ttf
```

New checks belong in runner-owned disposable directories under `build/`. Use the
native suites when managed graphics operations change. GPU comparisons must account
for driver precision; deterministic input/scheduling does not imply bit-identical
rendering across devices.

## Progress evidence

For each completed milestone, record the commit, commands, representative artifact,
measured resource costs, and remaining limitations in its completion table. Choose
the implementation branch/commit workflow at implementation time.

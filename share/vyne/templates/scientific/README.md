# {{name}}

A Vyne scientific-computing project. It integrates the Lorenz system
with RK4, estimates the largest Lyapunov exponent, and renders the x-y
phase portrait in the terminal.

## Build and run

From the project root:

```bash
vynec run src/main.vy
```

The demo runs 10000 RK4 steps at `dt = 0.01` (100 time units), prints a
finite-time Lyapunov estimate close to the asymptotic Lorenz value, and
draws the attractor with `vtermplot`. The whole integration is wrapped
in `@bounded<16777216> region`, so the scratch space has a
compiler-visible ceiling.

## Layout

| Path | Contents |
| --- | --- |
| `src/main.vy` | integration loop, Lyapunov estimate, phase portrait |
| `src/solver.vy` | Lorenz derivative and RK4 step |
| `project.toml` | targets, dependencies |
| `.devcontainer/` | reproducible development container |

## Learn more

- Vyne repository: <https://github.com/t2ncay/vyne>
- `vlin` and `vstat` manuals under `vyne/modules/external/`
- `vtermplot` manual: `vyne/modules/external/vtermplot/README.md`
{{#if license_spdx}}
- License: {{license_spdx}}
{{/if}}

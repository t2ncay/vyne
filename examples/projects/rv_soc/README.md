# rv_soc

A Vyne ML project. It trains a small MLP on the Iris dataset and prints
a terminal loss curve.

## Build and run

From the project root:

```bash
vynec run src/main.vy
```

The run loads `data/iris.data`, trains a 4 -> 16 -> 3 network for 200
epochs with Adam, prints the final accuracy, and renders the loss curve
with `vtermplot`. The training step is wrapped in
`@bounded<1048576> region`, so the compiler verifies the per-step
working set.

## Layout

| Path | Contents |
| --- | --- |
| `src/main.vy` | training loop, metrics, loss curve |
| `src/models.vy` | MLP definition and multi-class accuracy |
| `src/data.vy` | Iris loader via `vcsv` |
| `data/iris.data` | bundled UCI Iris dataset |
| `examples/` | small, focused Vyne programs |
| `project.toml` | targets, dependencies, and data manifest |

## Learn more

- Vyne repository: <https://github.com/t2ncay/vyne>
- `vml` manual: `vyne/modules/external/vml/README.md`
- `vlin` manual: `vyne/modules/external/vlin/README.md`

- License: MIT


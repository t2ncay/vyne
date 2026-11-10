# voptim - optimizers for training loops

`voptim` is the optimizer layer for Vyne: SGD, momentum/nesterov, Adam,
AdamW, RMSProp, Adagrad, and the common learning-rate schedules. It works on
flat `Array<Float64>` buffers, so it composes with `vlin.Matrix` by passing
`matrix.data` and `matrix.size()`.

The design rule is the same as the rest of the numerical stack: **build state
once, then update in place**. Every `*_step` function writes the parameter
buffer and returns nothing. The update kernels allocate nothing; wrappers pay only the current function-call frame at the call boundary.

```vyne
use external "voptim/voptim.vy";

params :: Array<Float64> := [2.0, -3.0];
opt :: voptim.AdamState := voptim.adam_new(params, 0.9, 0.999, 1e-8);

through step :: 0..199 -> loop {
    grads :: voptim.Series := voptim.Series([params[0], params[1]], 2);
    lr :: Float64 := voptim.cosine(step, 0.05, 200);
    voptim.adam_step(opt, grads, lr);
};
out(params[0] + ", " + params[1]);   # near zero for f(w)=0.5||w||^2
```

## Layout

| File | Role |
| :--- | :--- |
| `Types.vy` | `Series`, `SGDState`, `MomentumState`, `AdamState`, `RMSPropState`, `AdagradState` |
| `Kernels.vy` | Native-ABI update loops over typed arrays |
| `Schedules.vy` | constant, step, exponential, cosine, warmup-cosine |
| `Ops.vy` | constructors, validation, step wrappers |
| `voptim.vy` | facade |

## Optimizers

| Function | State | Update |
| :--- | :--- | :--- |
| `sgd_new(xs)` / `sgd_step(s, grads, lr)` | `SGDState` | `p -= lr*g` |
| `momentum_new(xs, momentum)` / `momentum_step(...)` | `MomentumState` | `v = m*v + g; p -= lr*v` |
| `nesterov_step(...)` | `MomentumState` | `v = m*v + g; p -= lr*(g + m*v)` |
| `adam_new(xs, b1, b2, eps)` / `adam_step(...)` | `AdamState` | bias-corrected Adam |
| `adamw_new(xs, b1, b2, eps, wd)` / `adamw_step(...)` | `AdamState` | decoupled weight decay + Adam |
| `rmsprop_new(xs, decay, eps)` / `rmsprop_step(...)` | `RMSPropState` | running squared-gradient |
| `adagrad_new(xs, eps)` / `adagrad_step(...)` | `AdagradState` | accumulated squared-gradient |

`adam_step` and `adamw_step` increment `state.t` and compute the bias
corrections internally. `state.weight_decay` is zero for plain Adam.

## Schedules

| Function | Result |
| :--- | :--- |
| `constant(step, lr)` | `lr` |
| `step_decay(step, lr, drop_every, gamma)` | `lr * gamma^floor(step/drop_every)` |
| `exponential(step, lr, gamma)` | `lr * gamma^step` |
| `cosine(step, lr, total)` | cosine decay to zero over `total` steps |
| `warmup_cosine(step, lr, warmup, total)` | linear warmup, then cosine decay |

`step` is zero-based.

## Zero-copy setup

The constructors copy a boxed `Array` once. If you already have a typed
buffer, construct the state directly:

```vyne
s :: voptim.AdamState := voptim.AdamState(params, m, v,
                                          0.9, 0.999, 1e-8, 0.0, 0, n);
```

The parameter and moment arrays are shared, not copied, so every step updates
the caller's buffers.

## Integrating with vlin

```vyne
use external "vlin/vlin.vy";
use external "voptim/voptim.vy";

w :: vlin.Types.Matrix := vlin.xavier(64, 16, 42);
opt :: voptim.AdamState := voptim.adam_new([/* copy or wrap w.data */], 0.9, 0.999, 1e-8);
```

For zero-copy use, construct an `AdamState` from `w.data` and its length
directly. Gradient buffers are ordinary `Array<Float64>` values.

## Validation

`examples/libs/voptim_test.vy` checks:

- each schedule at known steps,
- SGD/momentum/nesterov/Adam/AdamW/RMSProp/Adagrad reduce a quadratic,
- AdamW shrinks a zero-gradient parameter,
- AdamW decays a zero-gradient parameter.

`examples/libs/voptim_bench.vy` times 100k-parameter updates.

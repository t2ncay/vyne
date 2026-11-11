# vml

Minimal dense-neural-network primitives for Vyne.

**Version:** 0.1.0
**Status:** unstable — public surface may change between releases
**Module name:** `vml`
**Import path:** `use external "vml/vml.vy";`
**Requires:** `vlin` (matrix primitives), `vmath` (scalar math)

> **On the name.** The three letters stand for _Vyne machine learning_.
> The source has always used `vml` — in the `module vml;` declaration, in
> the mangled names, and in the `use external` path. This manual uses
> `vml` exclusively. If you have call sites that write `vmlx.foo(...)` or
> `vynn.foo(...)`, they will not resolve.

---

## Contents

1. [Overview](#overview)
2. [Quick start](#quick-start)
3. [Importing](#importing)
4. [Module layout](#module-layout)
5. [The `Dense`, `Sequential`, and `SGD` types](#the-dense-sequential-and-sgd-types)
6. [Reference semantics and the memory model](#reference-semantics-and-the-memory-model)
7. [API reference](#api-reference)
8. [Worked example](#worked-example)
9. [Limitations](#limitations)
10. [Design notes](#design-notes)
11. [Roadmap](#roadmap)
12. [Version history](#version-history)

---

## Overview

`vml` is a pure-Vyne CPU neural-network library. It provides the primitives
you need to assemble and train a fully-connected network — dense layers
with a small set of activations, a sequential container that composes
them, CPU optimizer helpers, and the scalar losses that cover common binary
classification and regression examples.

The library deliberately stays focused on CPU dense models. It does not try to
be the device tensor layer: use `vtensor` + `vnn` for explicit `@device` CNN and
GPU-training graphs. `vml` does one thing well: it represents a network as a
list of `Dense` layers and gives you the forward pass, activation derivatives,
loss helpers, and CPU weight-update helpers that close the loop.

`vml` is a thin layer. Every numeric operation is delegated to `vlin`.
A `Dense` layer holds a `vlin.Types.Matrix` for weights and a plain
`Array<Float64>` for biases; the forward pass is a single call to
`vlin.multiply` followed by `vlin.add_bias`. The SGD update is a single
call to `vlin.sgd_update_inplace`. `vml` is the composition logic — the
part that decides _which_ vlin call to make and in _what order_ — not a
second numerical library.

Because `vlin` is unboxed at the boundary and every operation in `vml`
is expressed in terms of `vlin` calls, a well-typed `vml` program
compiles to code where the inner loops of the forward and backward
passes are bare `double` arithmetic. The network's data path from input
matrix to loss is a chain of typed arrays with no boxing boundary in
between.

## Relationship to `vtensor` and `vnn`

| Library | Role |
| --- | --- |
| `vml` | CPU dense/MLP models over `vlin.Matrix` |
| `vlin` | CPU matrix/vector kernels used by `vml` |
| `voptim` | General CPU optimizer states over flat arrays |
| `vtensor` | Region-local device handle + shape metadata |
| `vnn` | Explicit `@device_fn` CNN/device training blocks over `vtensor` |

`vml` is not deprecated. It is the right layer for tabular data, small dense
models, CPU-only examples, and teaching the mechanics of training. `vnn` is the
right layer when tensors must stay device-resident inside `@device` regions.

---

## Quick start

```vyne
use external "vml/vml.vy";
use external "vlin/vlin.vy";
use native vmath;

ruleset { dynamic_casting };

# Build a 4 -> 3 -> 1 MLP.
L1 :: vml.Types.Dense = vml.dense(4, 3, "tanh");
L2 :: vml.Types.Dense = vml.dense(3, 1, "sigmoid");
model :: vml.Types.Sequential = vml.sequential([L1, L2]);

# Some input data.
X :: vlin.Types.Matrix = vlin.from_flat(2, 4, [
    0.5, -0.2, 0.1, 0.3,
    0.0,  0.7, -0.5, 0.2
]);

# Forward pass.
y_hat :: vlin.Types.Matrix = model.forward(X);
out("y_hat[0,0] = " + string(y_hat.get(0, 0)));
```

That is the entire training-ready core: one constructor call per layer,
one container, one forward pass. The backward pass is written by the
user in terms of `vlin` operations; see the **Worked example** below for
what that looks like.

---

## Importing

```vyne
use external "vml/vml.vy";
use external "vlin/vlin.vy";
use native vmath;
```

`vml.vy` is a facade. It pulls in the leaf modules and transitively
brings in `vlin`:

| Module            | Contents                                                      |
| ----------------- | ------------------------------------------------------------- |
| `Types.vy`        | `Dense`, `Sequential`, `SGD` interfaces, group `Types :: vml` |
| `Kernels.vy`      | `k_bias_grad` — the one ML-specific loop                      |
| `Constructors.vy` | `dense`, `dense_scaled`, `sequential`, `sgd`                  |
| `Ops.vy`          | Activation dispatch, forward pass, in-place SGD step          |
| `Reductions.vy`   | `cross_entropy`, `mse`, `accuracy`                            |

You may import individual leaf modules if you want to avoid pulling in
the full surface, but the facade is the recommended entry point.

Because `vml` re-exports `vlin`'s `Matrix` type through its method
signatures, both `use` lines are typically present in user code. If you
only need the network surface and not the raw matrix operations, the
`vml` import alone is sufficient — the types resolve through it.

---

## Module layout

```
vml/
├── vml.vy            # facade: use + deploy
├── Types.vy          # Dense, Sequential, SGD interfaces
├── Kernels.vy        # k_bias_grad
├── Constructors.vy   # dense, dense_scaled, sequential, sgd
├── Ops.vy            # apply_activation, forward, forward_all, sgd_step
└── Reductions.vy     # cross_entropy, mse, accuracy
```

### Layering

`vml` sits on top of `vlin` in a two-tier arrangement:

```
vml/                          ← composition logic, activation dispatch,
                                 loss functions, optimizer wrapper
───────────────────────────
vlin/                         ← matrix arithmetic, matrix products,
                                 elementwise ops, scalar reductions
───────────────────────────
Array<Float64> primitives     ← everything else
```

The boundary between tiers is a `vlin.Types.Matrix` value: a struct
containing two `Int64` dimensions and an `Array<Float64>`. `vml` never
looks inside that array directly except in the two places where it has
to — `k_bias_grad` (which reads `delta.data` row-by-row to accumulate
bias gradients) and the user's own backward pass.

---

## The `Dense`, `Sequential`, and `SGD` types

```vyne
group Types :: vml {

    interface Dense {
        W          :: vlin.Types.Matrix,
        b          :: Array<Float64>,
        activation :: String,
        # ... methods ...
    }

    interface Sequential {
        layers :: Array,
        # ... methods ...
    }

    interface SGD {
        lr :: Float64,
        # ... methods ...
    }
}
```

All three live in the `Types` group under `vml`, mirroring the
`Types :: vlin` convention. Users write `vml.Types.Dense`, not
`vml.Dense`, which makes it obvious at a call site that a name refers
to a type, not an operation.

### `Dense`

A single fully-connected layer with a weight matrix, a bias vector, and
the name of its activation function.

| Field        | Type                | Meaning                                             |
| ------------ | ------------------- | --------------------------------------------------- |
| `W`          | `vlin.Types.Matrix` | Weight matrix, shape `(in_features, out_features)`. |
| `b`          | `Array<Float64>`    | Bias vector, length `out_features`.                 |
| `activation` | `String`            | One of `"tanh"`, `"sigmoid"`, `"relu"`, `"linear"`. |

The forward pass computes:

```
z = x @ W + b
a = activation(z)
```

Note that `W` has shape `(in, out)`, not `(out, in)`. This is the
"transposed" convention — a forward pass is `x @ W`, not `x @ W.T`.
`vlin.xavier_init(in, out)` returns a matrix of shape `(in, out)`, so
`vml.dense(4, 3, ...)` produces a `W` of shape `(4, 3)`, and a forward
pass with `x` of shape `(batch, 4)` produces `z` of shape
`(batch, 3)`.

### `Sequential`

A container holding an `Array` of layers. Layers are applied in order.
The forward pass threads the input through every layer and returns the
final output.

`Sequential.layers` is a plain `Array`, not a `Array<Dense>`. Its
elements are expected to be `Dense` values (or values with a
`forward(Matrix) -> Matrix` method), but the container does not enforce
this. Pushing a non-layer into a `Sequential` produces a runtime error
on the first forward pass, not at construction time.

### `SGD`

A learning-rate holder.

```vyne
interface SGD {
    lr :: Float64,
    lr_value() -> Float64 { return self.lr; }
}
```

The `SGD` interface is deliberately minimal. There is no momentum
buffer, no adaptive learning rate, no per-parameter state. It carries
one number and offers one accessor.

**Note:** the optimizer object is not consumed by `vml.sgd_step`. That
function takes the learning rate as a plain `Float64` argument. The
`SGD` interface exists for API symmetry and for future extension; if
you want to use the object rather than a bare `Float64`, pass
`opt.lr_value()`. See **Roadmap** for the plan to unify the two.

---

## Reference semantics and the memory model

### Layers hold references, not values

A `Dense` layer stores its weight matrix by reference. Assigning a
`Dense` to another variable aliases the underlying data:

```vyne
L1 :: vml.Types.Dense = vml.dense(4, 3, "tanh");
L1_alias :: vml.Types.Dense = L1;

# Modifying L1.W's backing buffer is visible through L1_alias.
L1.W.data[0] = 999.0;
```

Same semantics as `vlin` matrices and Python objects. To obtain an
independent copy, use `vlin`'s `m.copy()` on the weight matrix and
rebuild the layer.

### Weight matrices are the only mutable state

The forward pass reads from `W` and `b` and allocates a fresh output
matrix per call. The backward pass, as written in user code, reads from
`W` and writes into a caller-provided gradient matrix. The only
mutation a training loop performs is the SGD step, which writes back
into `W`'s backing buffer in place:

```vyne
vlin.sgd_update_inplace(W, grad, lr);
```

Because `W`'s backing buffer is not reallocated, `W`'s address is
stable across training iterations. This matters under `vmem`
checkpointing: as long as the weight matrix was allocated _before_ the
first checkpoint, every SGD step mutates that same buffer regardless of
how many regions have rewound underneath it.

### Every forward pass allocates

Each call to `model.forward(x)` allocates:

- One output matrix per layer.
- Additional scratch matrices for `vlin.multiply`, `vlin.add_bias`, and
  the activation, depending on which `vlin` primitives that layer uses.

For a two-layer MLP at batch size 240, roughly 1.5 MB per forward pass
under the default arena model. A training loop that does
forward + backward + step, `K` times, allocates `O(K)` matrix buffers.
Without checkpointing, memory grows monotonically.

```vyne
through epoch :: 1..N -> loop {
    cp = vmem.checkpoint();

    # Every vml and vlin operation in here allocates. All of it
    # is dropped at the rewind.

    vmem.rewind(cp);
};
```

Same caveat as `vlin`: values that outlive the iteration — weights,
optimizer state, running metrics — must be allocated _outside_ the
region. See `vlin`'s **Reference semantics** section for the general
rule and `vfft`'s for a case study of what happens when you get it
wrong.

---

## API reference

### Constructors

```vyne
vml.dense(in_features :: Int64, out_features :: Int64,
          activation :: String) -> vml.Types.Dense
```

Builds a dense layer. Weight matrix initialized with `vlin.xavier_init`,
bias vector initialized to zeros. `activation` is stored verbatim; the
valid strings are `"tanh"`, `"sigmoid"`, `"relu"`, and `"linear"`.

```vyne
vml.dense_scaled(in_features :: Int64, out_features :: Int64,
                 activation :: String, scale :: Float64) -> vml.Types.Dense
```

Same as `dense`, but the weight matrix is post-multiplied by `scale`.
Useful for shrinking the effective learning rate of a specific layer
without changing the global optimizer setting. `scale` multiplies the
weights, not the outputs; `dense_scaled(i, o, a, 0.1)` produces weights
that are 10× smaller than `dense(i, o, a)`.

```vyne
vml.sequential(layers :: Array) -> vml.Types.Sequential
```

Wraps an array of layers in a `Sequential` container. Layers are
applied in the order they appear in the array.

```vyne
vml.sgd(lr :: Float64) -> vml.Types.SGD
```

Builds an `SGD` holder with the given learning rate. See the note under
[The `Dense`, `Sequential`, and `SGD` types](#the-dense-sequential-and-sgd-types)
about how the holder interacts with `sgd_step`.

### Activation dispatch

```vyne
vml.apply_activation(m :: vlin.Types.Matrix, kind :: String) -> vlin.Types.Matrix
vml.apply_activation_prime(a :: vlin.Types.Matrix, kind :: String) -> vlin.Types.Matrix
```

Dispatch on the activation name. `apply_activation` selects the forward
function; `apply_activation_prime` selects its derivative, evaluated at
the post-activation output `a`.

| `kind`      | Forward              | Prime                |
| ----------- | -------------------- | -------------------- |
| `"tanh"`    | `vlin.apply_tanh`    | `vlin.tanh_prime`    |
| `"sigmoid"` | `vlin.apply_sigmoid` | `vlin.sigmoid_prime` |
| `"relu"`    | `vlin.apply_relu`    | `vlin.relu_prime`    |
| `"linear"`  | identity             | matrix of ones       |

Unknown strings fall through to `"linear"` behaviour: the forward pass
returns `m` unchanged, and the prime returns a matrix of ones. This is
a deliberate lenient default, not a validation error — a network with a
misspelled activation will train, but as a linear network.

> **`relu_prime` is broken in `vlin` 0.1.0.** See `vlin`'s
> **Roadmap** for the codegen issue. Until it is fixed, do not use
> `"relu"` as the activation of a hidden layer in a network whose
> backward pass calls `vlin.relu_prime`. `"tanh"` and `"sigmoid"` both
> work correctly.

### Forward pass

```vyne
vml.forward(model :: vml.Types.Sequential, x :: vlin.Types.Matrix) -> vlin.Types.Matrix
```

Equivalent to `model.forward(x)`. Provided so that the forward pass can
be called as a free function when you want to treat the model as data.

```vyne
vml.forward_all(model :: vml.Types.Sequential, x :: vlin.Types.Matrix) -> Array
```

Returns every intermediate activation as an array. `h[0]` is the first
layer's output, `h[1]` the second's, and so on. `h[depth-1]` is the
final output.

This is the function a manual backward pass uses. The gradient of a
dense layer's input depends on the layer's own output, so threading the
intermediates through forward and backward in one object is what makes
a hand-written backprop tractable.

The return type is `Array`, not `Array<Matrix>`. Elements are boxed
matrix values. Indexing with `h[i]` yields a boxed `VyneValue` that
dereferences to a struct on use. See the design note about why the
empty-literal case had to be worked around.

### Optimizer step

```vyne
vml.sgd_step(W :: vlin.Types.Matrix, grad :: vlin.Types.Matrix,
             lr :: Float64)
```

Applies `W = W - lr * grad` in place. Delegates to
`vlin.sgd_update_inplace`. `W` and `grad` must have matching shapes;
the function does not check.

`W`'s backing buffer is mutated. Any other reference to the same matrix
sees the update. `grad` is not modified.

### Losses and metrics

```vyne
vml.cross_entropy(pred :: vlin.Types.Matrix,
                  target :: vlin.Types.Matrix) -> Float64
```

Binary cross-entropy, averaged over the number of rows in `pred`. `pred`
is clamped to `[1e-9, 1 - 1e-9]` before the logarithm, so exact 0 or 1
predictions do not produce infinities.

The averaging divisor is `pred.row`, not `pred.row * pred.col`. For a
`(N, 1)` prediction matrix — the canonical shape for binary
classification — these are the same. For a wider prediction matrix the
loss is per-row, not per-element. See **Limitations**.

```vyne
vml.mse(pred :: vlin.Types.Matrix,
        target :: vlin.Types.Matrix) -> Float64
```

Mean squared error, averaged over every element. Empty input returns
`0.0`.

```vyne
vml.accuracy(pred :: vlin.Types.Matrix,
             target :: vlin.Types.Matrix,
             n :: Int64) -> Float64
```

Fraction of the first `n` elements where `pred[i] > 0.5` matches
`target[i] > 0.5`. The `n` argument is caller-supplied and is not
checked against `pred.row * pred.col`; passing `n > size` reads past
the end of the backing buffer. Passing `n` less than the full size
evaluates on a prefix.

### Internal kernel

```vyne
vml.k_bias_grad(output :: Array<Float64>, delta :: Array<Float64>,
                rows :: Int64, cols :: Int64) -> Int64
```

For a `delta` matrix of shape `(rows, cols)`, computes the column-wise
sum into `output`: `output[c] = sum over r of delta[r * cols + c]`.
This is the operation needed to compute the bias gradient from the
upstream delta.

Public only because the parser resolves the name. Most programs should
not call it directly — the loss modules and user code that compute a
bias gradient tend to inline the loop rather than dispatch to this
kernel.

---

## Worked example

A complete training loop for a two-layer classifier, using only the
current surface. Compare with `vlin`'s worked example, which stops at
the forward pass.

```vyne
use external "vml/vml.vy";
use external "vlin/vlin.vy";
use external "vcolors/vcolors.vy";
use native vmath;
use native vmem;

ruleset { dynamic_casting };

vmath.seed(42);

# ---- Configuration ----
N_SAMPLES :: Int64 = 240;
N_FEATURES :: Int64 = 64;
HIDDEN :: Int64 = 16;
EPOCHS :: Int64 = 100;
LR :: Float64 = 0.5;

# ---- Data ----
# For the example, generate X and Y directly. Real code would load
# or synthesize them.
X :: vlin.Types.Matrix = vlin.random_uniform(N_SAMPLES, N_FEATURES, -1.0, 1.0);
Y :: vlin.Types.Matrix = vlin.from_flat(N_SAMPLES, 1, [...]);

# ---- Model ----
L1 :: vml.Types.Dense = vml.dense(N_FEATURES, HIDDEN, "tanh");
L2 :: vml.Types.Dense = vml.dense(HIDDEN, 1, "sigmoid");
model :: vml.Types.Sequential = vml.sequential([L1, L2]);

# Reach into the model to grab the weight matrices for the backward pass.
W1 :: vlin.Types.Matrix = model.layers[0].W;
W2 :: vlin.Types.Matrix = model.layers[1].W;

# Per-sample learning rate, since gradients are accumulated over the batch.
scale :: Float64 = LR / float64(N_SAMPLES);

# ---- Warm the plan-shaped state before the region loop ----
# See "Reference semantics" — anything that survives a rewind must be
# allocated above every checkpoint. For vml this means the model itself.
# (The model is constructed above, so nothing else is required here.)

# ---- Training ----
through epoch :: 1..EPOCHS -> loop {
    region step {
        # Forward, capturing intermediates.
        h :: Array = vml.forward_all(model, X);
        A1 = h[0];
        A2 = h[1];

        # Backward (manual).
        delta2 :: vlin.Types.Matrix = vlin.subtract(A2, Y);
        dW2 :: vlin.Types.Matrix = vlin.multiply(vlin.transpose(A1), delta2);

        delta1 :: vlin.Types.Matrix = vlin.hadamard(
            vlin.multiply(delta2, vlin.transpose(W2)),
            vlin.tanh_prime(A1));
        dW1 :: vlin.Types.Matrix = vlin.multiply(vlin.transpose(X), delta1);

        # Weight updates.
        vml.sgd_step(W1, dW1, scale);
        vml.sgd_step(W2, dW2, scale);
    };

    if epoch % 10 == 0 {
        h_final :: Array = vml.forward_all(model, X);
        A_final = h_final[1];
        loss :: Float64 = vml.cross_entropy(A_final, Y);
        acc  :: Float64 = vml.accuracy(A_final, Y, N_SAMPLES);
        out("epoch " + string(epoch) + "  loss " + string(loss) +
            "  acc " + string(acc));
    };
};

out(vcolors.green("done."));
```

The `region step` block bounds memory per iteration. `W1` and `W2` are
allocated above the loop — outside every checkpoint — and the
in-place SGD updates mutate them without reallocating, so the weights
survive every rewind.

For fully working examples that only use present functions, see
`examples/training/ml_seq.vy` and the focused
`examples/test/vml_cpu_mlp_train_test.vy` smoke test.

---

## Limitations

### No autograd

`vml` provides forward primitives and the four activation derivatives.
It does not provide reverse-mode differentiation. The backward pass in
the worked example above is written by hand: the user computes `delta2`,
`dW2`, `delta1`, and `dW1` explicitly, using `vlin` operations. This is
the intended usage for 0.1.0. An autograd engine is planned for a
future release but is not present.

### CPU optimizer scope

`vml` includes `sgd_step` and a compact Adam helper (`vml.adam`,
`vml.adam_state`, `vml.adam_step`) for dense CPU models. The broader CPU
optimizer family lives in `voptim`, while GPU/device-resident AdamW lives in
`vnn`. Keep `vml` optimizer use focused on small dense examples; use `voptim`
or `vnn` when those layers better match the workload.

### No convolutional or recurrent layers

Only fully-connected layers (`Dense`) are provided. Sequential is a
list of `Dense`-shaped objects. There is no conv2d, no LSTM, no
attention.

### The `SGD` type is not wired to `sgd_step`

`vml.sgd(lr)` returns an `SGD` value. `vml.sgd_step` takes a bare
`Float64` learning rate, not an `SGD`. The two are not connected. If you
want to use the object, pass `opt.lr_value()` explicitly:

```vyne
opt :: vml.Types.SGD = vml.sgd(0.01);
vml.sgd_step(W, grad, opt.lr_value());
```

The `SGD` interface exists for API symmetry with the future optimizer
family. See **Roadmap**.

### `cross_entropy` averages per row, not per element

`vml.cross_entropy` divides the total loss by `pred.row`. For a `(N, 1)`
prediction matrix — the shape `vml.dense(..., 1, "sigmoid")` produces —
this is correct. For a `(N, K)` prediction matrix with `K > 1`, the loss
is the sum over all `N * K` elements divided by `N`, which is `K` times
larger than the conventional mean. Multi-class classification is not
supported in 0.1.0.

### The `Array` return of `forward_all` is boxed

`forward_all` returns a plain `Array`, not an `Array<Matrix>`. Each
element is a boxed `VyneValue` that dereferences to a matrix struct on
use. This is a deliberate workaround for a codegen limitation; see
**Design notes**. The cost is one box-unbox per intermediate, which is
negligible for a network of three to five layers but visible in a very
deep network with many tiny layers.

### `accuracy` trusts the caller's `n`

The `n` argument to `vml.accuracy` is the number of samples to evaluate.
Passing `n > pred.row * pred.col` reads past the end of the backing
buffer; passing `n < pred.row * pred.col` evaluates on a prefix. The
function does not check, because checking on every call would cost more
than the caller-facing convenience is worth. Pass `pred.row` if you
want to evaluate the whole matrix.

### RNG quality

`vml.dense` calls `vlin.xavier_init`, which calls `vmath.random_float`.
The same LCG caveat from `vlin`'s manual applies: adequate for weight
initialization, not adequate for statistical work.

### No GPU, no multithreading

Every operation is a scalar loop over `double`. Same caveat as `vlin`.

---

## Design notes

### Why `vml` is a layer over `vlin` instead of a single library

The split exists because the two libraries have different update
cadences. `vlin`'s public surface is stable and its numerical choices
(row-major layout, `Array<Float64>` storage, transposed weight
convention) are baked into the ABI. `vml`'s surface is expected to
change as the ML ecosystem matures — new optimizers, new layers, new
loss functions, autograd.

Keeping the numerical core in `vlin` and the composition logic in
`vml` means a change to `vml`'s API does not invalidate `vlin`'s, and a
performance fix in `vlin`'s kernels immediately benefits every `vml`
program. This is why `vml` re-exports `vlin`'s `Matrix` type rather
than defining its own: there is exactly one matrix type in the
ecosystem, and every layer above `Array<Float64>` uses it.

### Why `Dense` stores `W` as `(in, out)` rather than `(out, in)`

The conventional PyTorch shape for a dense layer is `(out, in)`,
following the `y = W @ x + b` formulation. `vml` uses `(in, out)`,
following `y = x @ W + b`.

Two reasons. First, it matches `vlin`'s `xavier_init(rows, cols)`
argument order and the row-major layout of the flat buffer: `W[r, c]`
for `r` in the input dimension and `c` in the output dimension, laid
out contiguously in `W.data[r * out + c]`. The forward-pass matmul
`x @ W` uses the standard `k_matmul` kernel with no transpose.

Second, `vml`'s forward pass computes `x @ W` and not `x @ W.T`. If `W`
were stored `(out, in)`, the forward pass would need
`vlin.multiply_trans_b`, which allocates a transposed copy of `W` on
every call. The current convention avoids that allocation.

The cost is a small mental-model shift: when you see `vml.dense(4, 3)`
you read the arguments as `(in, out)`, not `(out, in)`. The
`Dense.in_features()` and `Dense.out_features()` methods report `W.row`
and `W.col` respectively, which is what you want if you never think
about the storage order.

### Why `forward_all` returns a boxed `Array`

A `Array<Matrix>` return type would be the natural choice. In practice,
the current emitter's empty-array fast path defaults the element type
of `[]` to `Float64`, and a subsequent `.push` of a `Dense` value
coerces the struct through `.as.i64`, producing a garbage `double`
rather than a boxed struct. The workaround is to start the accumulator
with a one-element literal `[first]`; the element-type inference then
bails (`Unknown`) and the array stays boxed.

The user-visible consequence is that `forward_all` returns a
`VyneValue`-array, and `h[i]` yields a boxed value that dereferences on
use. For a network with `d` layers, that is `d` box/unbox round trips
per forward pass, each costing a few nanoseconds. Negligible for
typical depth; visible only in deep networks with many tiny layers.

This is a codegen issue and will be fixed at the emitter level, at
which point `forward_all`'s signature will tighten to `Array<Matrix>`.
The user-visible change will be source-compatible for callers that
already treat `h[i]` as an opaque handle.

### Why `apply_activation` uses string dispatch

A tagged union or an enum would be type-safe and marginally faster.
Neither is available in Vyne 0.1.0. The language has no enums with
methods, no sum types, and no function-pointer type. String dispatch
is the only mechanism that lets a `Dense` layer carry its activation
choice as data rather than as a compile-time constant.

The cost is one string comparison per activation application, which is
negligible next to the `vlin` matmul that precedes it. If activation
dispatch ever shows up in a profile — it will not for typical networks
— the fix is to resolve the string once at layer-construction time and
store a small integer tag alongside it.

### Why `SGD` is separate from `sgd_step`

The design intent is that optimizers will eventually be first-class
objects with their own state — momentum buffers, per-parameter
accumulators, learning-rate schedules. `SGD` is the smallest such
object: it carries one number and offers one accessor.

`sgd_step` was written before the optimizer abstraction settled, and
takes the learning rate as a bare `Float64`. Unifying the two — making
`sgd_step` a method on `SGD` — is a small change once the optimizer
family has two or more members. The current split is an honest
representation of where the API is: the object exists, but the call
sites do not yet consume it.

### Why no autograd

Autograd is a large subsystem: a tape, a reverse-mode traversal, node
types for every differentiable primitive, and a coherent story for
control flow and in-place mutation. Building it correctly is a
language-level project, not a library-level one. `vml` 0.1.0 provides
the primitives that a manual backward pass needs — forward activation
outputs, activation derivatives, matrix products — and leaves the
backward pass to user code. This is the same division of labour that
early versions of every ML framework started with.

An autograd engine, if and when it lands, will live above `vml`, not
inside it, and will consume `vml`'s forward primitives as its building
blocks.

---

## Roadmap

The following are planned or in progress.

### Autograd

A reverse-mode automatic differentiation engine. Every `vml` forward
primitive gains a backward counterpart, and `Sequential.backward(loss)`
becomes a single call. The tape will be arena-allocated, which means a
training loop with `vmem` checkpointing will reclaim the tape per
iteration.

This is the single largest planned addition to `vml` and is expected
to land in a 0.2 release, not 0.1.x.

### Optimizer family

Beyond `SGD`: `Adam`, `RMSprop`, `AdaGrad`, and momentum variants.
Each carries its own per-parameter state, which means `SGD`-style
holder objects with mutable arrays inside. The `sgd_step` free
function will be generalized to `opt.step(W, grad)` for all optimizers.

### Multi-class classification

A `softmax` activation and a categorical `cross_entropy` that averages
per element rather than per row. This is a small addition once the
average-vs-sum semantics of `cross_entropy` are made explicit — likely
by renaming the current function to `binary_cross_entropy` and adding
`categorical_cross_entropy` alongside it.

### Dropout and normalization

Element-wise stochastic masking (`Dropout`) and per-batch
normalization (`BatchNorm`, `LayerNorm`). Both are straightforward
under the current design — they are element-wise operations that fit
the existing forward/backward split — but neither is present in 0.1.0.

### Weight initialization variants

`vml.dense` uses `xavier_init` unconditionally. He initialization,
orthogonal initialization, and constant initialization are all
one-liner variants of the same constructor and will be added alongside
the current `dense` and `dense_scaled`.

### `Dense.forward` in the native ABI

The `Dense.forward` method currently boxes its `x` argument and returns
a boxed matrix, because the method is called through the interface
dispatch machinery. A specialized path for the case where the caller's
matrix is a proven `VyneArray_f64` would keep the entire forward pass
unboxed. Blocked on the same emitter issue that limits `vfft`'s
out-of-place variants.

### `relu_prime` fix

`vlin.relu_prime` produces a matrix of nulls rather than the correct
derivative, due to a codegen issue in `ForNode::getCExpr`. Until it is
fixed, `vml.dense(..., "relu")` is usable forward-only. The fix is in
`vlin`'s scope; `vml` will simply unblock when `vlin` does.

---

## Version history

**0.1.0** — initial release. `Types` (`Dense`, `Sequential`, `SGD`),
`Constructors` (`dense`, `dense_scaled`, `sequential`, `sgd`),
`Ops` (`apply_activation`, `apply_activation_prime`, `forward`,
`forward_all`, `sgd_step`), `Reductions` (`cross_entropy`, `mse`,
`accuracy`), `Kernels` (`k_bias_grad`). Manual backward pass; no
autograd. SGD only; no adaptive optimizers.

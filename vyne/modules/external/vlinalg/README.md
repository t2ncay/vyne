# vlinalg

Matrix and vector linear algebra for Vyne.

**Version:** 0.1.0
**Status:** unstable — public surface may change between releases
**Requires:** `vmath` (scalar math), `vcolors` (diagnostic output)

---

## Overview

`vlinalg` is a pure-Vyne linear algebra library. It provides a small,
predictable set of matrix and vector operations sufficient to build and
train dense neural networks, to solve small systems of linear equations,
and to prototype numerical algorithms directly in Vyne source.

The library deliberately omits the features that make NumPy and BLAS
fast — broadcasting, strided views, SIMD kernels, integer matrices. It
does exactly one thing well: it represents a matrix as a flat
`Array<Float64>` and provides the primitive operations over that
representation that machine-learning workloads actually call. That
flat representation is the same one the transpiler unboxes to native
C when the element type is statically known, so a well-typed `vlinalg`
program compiles to code that the C compiler can optimize.

The library is intentionally small. Everything it exposes is intended
to be read, not just called. The source files are the specification.

---

## Quick start

```vyne
use lib "vlinalg/vlinalg.vy";
module vlinalg;
module vmath;

# 2x3 matrix from a nested array
A :: vlinalg.Types.Matrix = vlinalg.from_array([
    [1.0, 2.0, 3.0],
    [4.0, 5.0, 6.0]
]);

# Or build one from a flat buffer
B :: vlinalg.Types.Matrix = vlinalg.from_flat(2, 3, [
    1.0, 0.0, 0.0,
    0.0, 1.0, 0.0
]);

# Element-wise and matrix operations
C :: vlinalg.Types.Matrix = vlinalg.add(A, B);
D :: vlinalg.Types.Matrix = vlinalg.transpose(A);
E :: vlinalg.Types.Matrix = vlinalg.multiply(A, D);   # 2x2

# Method form is available for most operations
println(A.sum());
println(A.shape());
println(A.row_at(0));
```

````

Two calling conventions coexist and are equivalent:

- **Function form** — `vlinalg.multiply(A, B)` — reads as "apply the
  multiply operation to A and B". This is the form used by the Ops,
  Activations, Losses, and Reductions modules.
- **Method form** — `A.transposed()`, `A.sum()`, `A.softmax()` — reads
  as "ask the matrix to do X". This is the form defined on the `Matrix`
  and `Vector` interfaces.

For operations that exist in both forms, use whichever reads better at
the call site. They dispatch to identical code.

---

## The `Matrix` type

```vyne
interface Matrix {
    row  :: Int64,
    col  :: Int64,
    data :: Array<Float64>
}
```

A `Matrix` is a triple of two `Int64` dimensions and a flat
`Array<Float64>`. There is no stride field, no offset, no view
mechanism. Row-major layout is the only layout.

### Storage layout

Element `(r, c)` lives at index `r * col + c` in the flat `data`
buffer.

```
A = [[a00, a01, a02],
     [a10, a11, a12]]     (2 rows, 3 cols)

A.data = [a00, a01, a02, a10, a11, a12]
             0    1    2    3    4    5
```

This is the same layout C uses for nested arrays and the same layout
BLAS uses for row-major matrices. It matters if you plan to
interoperate with `vlinalg` from native code, because the flat buffer
is the interop surface.

### The `Array<Float64>` annotation is load-bearing

The `data` field is annotated `Array<Float64>`, not `Array`. The
annotation is read by the Vyne C backend: when a struct field is
annotated with a typed-array type, the backend unboxes that field into
a native `VyneArray_f64` on every read, so `A.data[i]` compiles to a
direct `double` load instead of going through the boxed
`vyne_index_get` dispatch path.

Changing the annotation to plain `Array` still type-checks in Vyne
source but makes every element access in the library roughly 8×
slower. Do not change it.

---

## Importing

```vyne
use lib "vlinalg/vlinalg.vy";
module vlinalg;
module vmath;
```

`vlinalg.vy` is a facade. It pulls in six leaf modules:

| Module            | Contents                                                      |
| ----------------- | ------------------------------------------------------------- |
| `Types.vy`        | `Matrix` and `Vector` interfaces, in group `Types :: vlinalg` |
| `Constructors.vy` | Factories: zeros, identity, random initializers               |
| `Ops.vy`          | Arithmetic, matrix product, stacking, in-place SGD            |
| `Activations.vy`  | Non-linearities and their derivatives                         |
| `Reductions.vy`   | Sum, mean, min, max, trace, Frobenius norm                    |
| `Losses.vy`       | MSE and cross-entropy, with derivatives                       |

You may also import individual leaf modules if you want to avoid
pulling in the full surface. The facade is the recommended entry point.

---

## API reference

### Types

Constructors for the two interfaces. All matrices in the library are
instances of `vlinalg.Types.Matrix`; all 2-D geometric vectors are
instances of `vlinalg.Types.Vector`.

```vyne
m :: vlinalg.Types.Matrix = vlinalg.Types.Matrix(rows, cols, data);
v :: vlinalg.Types.Vector = vlinalg.Types.Vector(x, y);
```

The `Matrix` constructor takes a flat `Array` and does no shape
validation. If `rows * cols != data.size()`, the resulting matrix will
behave unpredictably. Use the factories below unless you have a
reason to construct a matrix from a pre-validated buffer.

### Constructors

```vyne
vlinalg.zeros(rows :: Int64, cols :: Int64) -> Matrix
vlinalg.ones(rows :: Int64, cols :: Int64) -> Matrix
vlinalg.full(rows :: Int64, cols :: Int64, value :: Float64) -> Matrix
vlinalg.identity(n :: Int64) -> Matrix
```

Fill matrices. `zeros` and `ones` allocate a fresh flat buffer of the
requested size and fill it element-wise; `full` fills with the given
scalar; `identity` produces an `n × n` diagonal matrix.

```vyne
vlinalg.from_array(data :: Array) -> Matrix
```

Build a matrix from a nested array. `data[r][c]` becomes element
`(r, c)`. Column count is inferred from `data[0]`. If the inner arrays
have inconsistent lengths, the result is undefined — the library does
not check.

```vyne
vlinalg.from_flat(rows :: Int64, cols :: Int64, data :: Array) -> Matrix
```

Build a matrix from a flat array. `data[r * cols + c]` becomes element
`(r, c)`. Same absence of length validation as the raw constructor.

```vyne
vlinalg.random_uniform(rows, cols, lo :: Float64, hi :: Float64) -> Matrix
vlinalg.xavier_init(rows :: Int64, cols :: Int64) -> Matrix
vlinalg.he_init(rows :: Int64, cols :: Int64) -> Matrix
vlinalg.random_init(rows :: Int64, cols :: Int64) -> Matrix
```

Weight initializers.

- `random_uniform` draws every element uniformly from `[lo, hi)`.
- `xavier_init` draws uniformly from `[-L, L]` where
  `L = sqrt(6 / (rows + cols))`. This is the Glorot uniform scheme,
  appropriate for `tanh` and `sigmoid` activations.
- `he_init` draws uniformly from `[-L, L]` where
  `L = sqrt(6 / cols)`. The `cols` parameter is treated as `fan_in`; this
  is the He uniform scheme for ReLU networks when the matrix is applied
  as `Y = X · W`.
- `random_init` draws uniformly from `[-0.5, 0.5)`. Rarely what you
  want; provided for symmetry with the reference implementations.

All initializers call `vmath.random_float` under the hood. Note that
`vmath.random_float` uses a small linear congruential generator whose
low-order bits are not well-distributed. See **Limitations** below.

### Ops — element-wise

```vyne
vlinalg.add(a :: Matrix, b :: Matrix) -> Matrix
vlinalg.subtract(a :: Matrix, b :: Matrix) -> Matrix
vlinalg.hadamard(a :: Matrix, b :: Matrix) -> Matrix
```

Element-wise binary operations. All three require `a.row == b.row` and
`a.col == b.col`. On mismatch they print a red diagnostic and return a
`0 × 0` matrix. They do not raise.

- `add` computes `a[i] + b[i]` element-wise.
- `subtract` computes `a[i] - b[i]` element-wise.
- `hadamard` computes `a[i] * b[i]` element-wise. Also known as the
  Schur product.

### Ops — matrix product

```vyne
vlinalg.multiply(a :: Matrix, b :: Matrix) -> Matrix
vlinalg.transpose(m :: Matrix) -> Matrix
```

`multiply` is the ordinary matrix product. It requires `a.col == b.row`
and returns a matrix of shape `(a.row, b.col)`. On mismatch it prints a
red diagnostic and returns `0 × 0`.

The implementation is a straightforward triple loop with no blocking,
no cache-aware traversal, and no SIMD. For matrices larger than a few
hundred elements per side, the constant factor will be significantly
worse than a tuned BLAS. That is acceptable for the library's target
use cases (small dense neural networks, small linear systems) but not
for large-scale numerical work.

`transpose` delegates to `Matrix.transposed()`. Same semantics, same
allocation profile.

### Ops — scalar

```vyne
vlinalg.add_scalar(m :: Matrix, s :: Float64) -> Matrix
vlinalg.multiply_scalar(m :: Matrix, s :: Float64) -> Matrix
vlinalg.clip(m :: Matrix, lo :: Float64, hi :: Float64) -> Matrix
```

`add_scalar` and `multiply_scalar` apply an affine transformation to
every element. `clip` applies `vmath.clamp(x, lo, hi)`, which silently
swaps the bounds if `lo > hi`.

### Ops — bias and dot products

```vyne
vlinalg.add_bias(m :: Matrix, b :: Array) -> Matrix
vlinalg.dot(a :: Matrix, b :: Matrix) -> Float64
vlinalg.outer(a :: Matrix, b :: Matrix) -> Matrix
```

`add_bias` takes a plain `Array`, not a `Matrix`. The bias vector is
broadcast row-wise: `out[r][c] = m[r][c] + b[c]`. The bias array must
have length `m.col`.

`dot` computes the inner product of two vectors. It handles four
shapes: `1 × n · 1 × n`, `m × 1 · m × 1`, `1 × n · m × 1`, and
`m × 1 · 1 × n`. It does not validate that the operand dimensions
match. Use it only when you are sure of the shapes.

`outer` computes the outer product of two vectors: `out[i][j] =
a[i] * b[j]`. `a` and `b` are treated as column vectors; only their
row counts matter.

### Ops — stacking

```vyne
vlinalg.vstack(a :: Matrix, b :: Matrix) -> Matrix
vlinalg.hstack(a :: Matrix, b :: Matrix) -> Matrix
```

`vstack` requires matching column counts and produces a matrix with
`a.row + b.row` rows. `hstack` requires matching row counts and
produces a matrix with `a.col + b.col` columns. Both return `0 × 0`
on mismatch with a red diagnostic.

### Ops — in-place SGD

```vyne
vlinalg.sgd_update_inplace(W :: Matrix, grad :: Matrix, lr :: Float64)
```

Applies the update `W[i] = W[i] - lr * grad[i]` directly to `W`'s
backing buffer. `W` and `grad` must have the same shape. Returns
`null`.

This is the one mutating operation in the library. It exists for two
reasons:

1. It eliminates the two allocations (`multiply_scalar` and `subtract`)
   that the equivalent functional form would produce. For a training
   loop, that is a real win.
2. It keeps the weight matrix at a fixed address across training
   iterations. This is required for the weight matrix to remain valid
   across a `vmem.rewind` boundary.

For the functional equivalent without the in-place semantics, use
`vlinalg.subtract(W, vlinalg.multiply_scalar(grad, lr))`. That form is
correct and unambiguous but allocates two intermediate matrices.

Because `W.data` is a shared reference to the caller's backing buffer,
`sgd_update_inplace` mutates the caller's matrix in place. See
**Reference semantics** below.

### Activations — forward

```vyne
vlinalg.apply_sigmoid(m :: Matrix) -> Matrix
vlinalg.apply_tanh(m :: Matrix) -> Matrix
vlinalg.apply_relu(m :: Matrix) -> Matrix
vlinalg.apply_exp(m :: Matrix) -> Matrix
vlinalg.apply_log(m :: Matrix) -> Matrix
```

Element-wise non-linearities. Each returns a fresh matrix of the same
shape as `m`. The scalar functions are dispatched through `vmath`, so
they use the same C library implementations as the rest of Vyne.

`apply_relu` is provided but cannot currently be used in training
because of a codegen issue that affects `relu_prime` (see
**Limitations**).

### Activations — derivatives

```vyne
vlinalg.sigmoid_prime(m :: Matrix) -> Matrix
vlinalg.tanh_prime(m :: Matrix) -> Matrix
vlinalg.relu_prime(m :: Matrix) -> Matrix
```

These functions compute the derivative of the corresponding activation
**evaluated at the activation's output**, not at its input. This is the
convention used by the library's own backpropagation examples and by
most textbook derivations.

Concretely:

- `sigmoid_prime(a)` computes `a * (1 - a)`. Pass the **output** of
  `apply_sigmoid`, not the pre-activation value.
- `tanh_prime(a)` computes `1 - a * a`. Pass the **output** of
  `apply_tanh`.
- `relu_prime(v)` returns 1 where `v > 0` and 0 elsewhere. Pass the
  **pre-activation** value here, since ReLU's derivative depends on the
  input's sign, not on the output's magnitude. This is inconsistent
  with the other two; it is a consequence of the fact that ReLU is not
  symmetric in its input/output relationship.

This post-activation convention is not universal. Some frameworks pass
the pre-activation value to `sigmoid_prime` and compute the sigmoid
inside. Both conventions are correct as long as you are consistent. If
you are integrating `vlinalg` into existing code, check which
convention that code uses before reusing the derivative functions.

### Activations — softmax and normalize

```vyne
vlinalg.softmax(m :: Matrix) -> Matrix
vlinalg.normalize(m :: Matrix) -> Matrix
```

`softmax` applies a numerically stable row-wise softmax: it subtracts
the row maximum before exponentiating, then divides by the sum. Only
the row-wise version is provided; for column-wise softmax, transpose
the input first, apply, and transpose the result.

`normalize` divides the entire matrix by its Frobenius norm. If the
norm is zero, the divisor is set to 1 and the matrix is returned
unchanged.

### Reductions — top-level

```vyne
vlinalg.sum(m :: Matrix) -> Float64
vlinalg.mean(m :: Matrix) -> Float64
vlinalg.minimum(m :: Matrix) -> Float64
vlinalg.maximum(m :: Matrix) -> Float64
vlinalg.norm_fro(m :: Matrix) -> Float64
vlinalg.trace(m :: Matrix) -> Float64
```

- `sum` — sum of all elements.
- `mean` — `sum / (row * col)`. Returns 0 for an empty matrix.
- `minimum`, `maximum` — scalar extremum over all elements. Return 0
  for an empty matrix.
- `norm_fro` — Frobenius norm, `sqrt(sum of squares)`. Never negative.
- `trace` — sum of the diagonal. For a non-square matrix, the smaller
  of `row` and `col` determines the diagonal length.

All of these also exist as methods on the `Matrix` interface, plus
`argmax` and `argmin`, which are method-only.

### Losses

```vyne
vlinalg.mse(pred :: Matrix, target :: Matrix) -> Float64
vlinalg.mse_prime(pred :: Matrix, target :: Matrix) -> Matrix
vlinalg.cross_entropy(pred :: Matrix, target :: Matrix) -> Float64
vlinalg.cross_entropy_prime(pred :: Matrix, target :: Matrix) -> Matrix
```

`mse` computes `(1/n) · Σ (pred[i] - target[i])²` where `n` is the total
number of elements. `mse_prime` returns the element-wise derivative
`2 · (pred[i] - target[i]) / n`, as a fresh matrix.

`cross_entropy` computes the binary cross-entropy
`-(1/r) · Σ (y · log(p) + (1 - y) · log(1 - p))` where `r` is the row
count and `p` is clamped to `[eps, 1 - eps]` for numerical stability,
with `eps = 1e-9`. The per-row normalization is the standard
convention for binary classification with a `(N, 1)` output matrix, and
coincides with per-element normalization when the target has one
column.

`cross_entropy_prime` returns `pred[i] - target[i]` element-wise. This
is the derivative of cross-entropy composed with sigmoid, and is the
form used in the demo backpropagation loops.

Both `cross_entropy` and `cross_entropy_prime` assume the prediction
matrix has already passed through `sigmoid`. They do not apply the
sigmoid internally.

---

## Method reference — Matrix

The `Matrix` interface provides the following methods. All are called
as `m.method(args)`. The list is organized by category; see the API
reference above for the top-level equivalents.

### Shape and structure

| Method                  | Returns   | Notes                                  |
| ----------------------- | --------- | -------------------------------------- |
| `m.shape()`             | `Array`   | `[row, col]`                           |
| `m.size()`              | `Int64`   | `row * col`                            |
| `m.is_square()`         | `Bool`    | `row == col`                           |
| `m.is_vector()`         | `Bool`    | `row == 1` or `col == 1`               |
| `m.copy()`              | `Matrix`  | Deep copy of the data buffer           |
| `m.flatten()`           | `Array`   | Returns `m.data` (shared, not copied)  |
| `m.row_at(r)`           | `Array`   | Row `r` as a fresh flat array          |
| `m.col_at(c)`           | `Array`   | Column `c` as a fresh flat array       |
| `m.get(r, c)`           | `Float64` | Element at `(r, c)`                    |
| `m.insert_row(new_row)` | `Matrix`  | Appends a row. `new_row.size() == col` |

### Reductions

| Method         | Returns   | Notes                         |
| -------------- | --------- | ----------------------------- |
| `m.sum()`      | `Float64` | Sum of all elements           |
| `m.mean()`     | `Float64` | Mean of all elements          |
| `m.minimum()`  | `Float64` | Scalar minimum                |
| `m.maximum()`  | `Float64` | Scalar maximum                |
| `m.trace()`    | `Float64` | Sum of diagonal               |
| `m.norm_fro()` | `Float64` | Frobenius norm                |
| `m.argmax()`   | `Int64`   | Index of maximum (flat index) |
| `m.argmin()`   | `Int64`   | Index of minimum (flat index) |

### Element-wise unary

| Method              | Returns  |
| ------------------- | -------- |
| `m.negate()`        | `Matrix` |
| `m.apply_sigmoid()` | `Matrix` |
| `m.apply_tanh()`    | `Matrix` |
| `m.apply_relu()`    | `Matrix` |
| `m.apply_exp()`     | `Matrix` |
| `m.apply_log()`     | `Matrix` |

### Element-wise scalar

| Method            | Returns  |
| ----------------- | -------- |
| `m.add_scalar(s)` | `Matrix` |
| `m.sub_scalar(s)` | `Matrix` |
| `m.mul_scalar(s)` | `Matrix` |
| `m.div_scalar(s)` | `Matrix` |
| `m.clip(lo, hi)`  | `Matrix` |

### Softmax and normalization

| Method          | Returns  |
| --------------- | -------- |
| `m.softmax()`   | `Matrix` |
| `m.normalize()` | `Matrix` |

### Shape manipulation

| Method                  | Returns  |
| ----------------------- | -------- |
| `m.reshape(rows, cols)` | `Matrix` |
| `m.transposed()`        | `Matrix` |

---

## Method reference — Vector

`vlinalg.Types.Vector` is a 2-D geometric vector, not an n-dimensional
container. Its fields are `x :: Int64` and `y :: Int64`, and its
methods operate on those two integers.

| Method                   | Returns   | Notes                                 |
| ------------------------ | --------- | ------------------------------------- |
| `v.magnitude()`          | `Float64` | Euclidean length                      |
| `v.slope()`              | `Float64` | `y / x`; undefined for `x == 0`       |
| `v.cross_product(other)` | `Int64`   | 2-D cross product `x · o.y - y · o.x` |
| `v.dot(other)`           | `Int64`   | Inner product `x · o.x + y · o.y`     |

`Vector` is provided for geometric computations. It is not used by the
neural-network code paths, which represent vectors as `Matrix`
instances with one dimension equal to 1.

---

## Reference semantics

Vyne arrays and matrices are passed to functions by reference, not by
value. This means:

```vyne
fn mutate(m :: vlinalg.Types.Matrix) {
    m.data[0] = 999.0;
}

A :: vlinalg.Types.Matrix = vlinalg.zeros(2, 2);
mutate(A);
println(A.data[0]);   # 999.0
```

The parameter `m` inside `mutate` aliases the caller's `A`. Any write
to `m.data` writes through to the caller's matrix. This is the same
semantics as Python lists, JavaScript arrays, and Lua tables.

This is why `sgd_update_inplace` works. It is also why you should be
cautious when writing functions that take a `Matrix` parameter and
mutate it: the mutation is not visible in the function signature.

If you want a defensive copy, call `m.copy()` at the top of the
function. That is the only way to obtain value semantics in Vyne.

---

## Memory model

Every operation in `vlinalg` returns a freshly allocated matrix, with
two exceptions:

- `m.flatten()` returns the internal `data` buffer itself, not a copy.
- `sgd_update_inplace` mutates in place and returns `null`.

The fresh allocations come from the arena allocator. Under the default
memory model, arena allocations live until process exit. In a training
loop this means a monotonic increase in memory unless you use a
`vmem` region:

```vyne
module vmem;

through epoch :: 1..N -> loop {
    cp = vmem.checkpoint();

    # Every vlinalg operation in here allocates.
    # All of it is dropped at the rewind.

    vmem.rewind(cp);
};
```

For a `64 → 16 → 12 → 1` MLP with batch size 240, that is roughly
1.5 MB of intermediate matrices per forward-plus-backward pass. Across
500 epochs, that would be 750 MB without a checkpoint and a few
hundred kilobytes with one. The `vmem` primitive is the intended tool
for this pattern; see the language runtime documentation for details.

The one thing a checkpoint cannot save you from is the setup phase.
If your sequence-generation or feature-extraction code allocates
heavily before the training loop starts, those allocations will not be
reclaimed by any checkpoint inside the loop. Structure your program so
that the setup phase has a bounded working set, or wrap it in its own
checkpoint that stays active for the entire run.

---

## Limitations

The library is small and does not attempt to be a general-purpose
numerical environment. The following are known constraints.

### No broadcasting

Binary operations require matching shapes. There is no NumPy-style
implicit expansion. `vlinalg.add` of a `(N, C)` matrix and a `(1, C)`
row vector will fail; use `vlinalg.add_bias`, which takes a plain array
of length `C` and broadcasts row-wise.

### No views or slicing at the library level

There is no `vlinalg.slice`. If you need a submatrix, you must copy the
elements into a fresh matrix yourself. Vyne's `SliceNode` produces a
fresh array from a `Matrix` only via `m.data[a:b]`, which returns a
flat `Array`, not a `Matrix`. Reconstructing a matrix from a flat slice
requires `vlinalg.from_flat`.

### No shape validation in `dot`

`vlinalg.dot` handles four operand shapes but does not check that the
inner dimensions match. Calling it with mismatched shapes produces
either a wrong answer or an out-of-bounds read. Use it only when you
are certain of the operand dimensions.

### `relu` cannot be used for training

`vlinalg.relu_prime` uses an `if/else` inside a `collect` block. The
current Vyne C backend emits both branches as statements and then
pushes `vyne_null()`, so `relu_prime` produces a matrix of nulls. This
is a codegen issue in `ForNode::getCExpr`, not a bug in `vlinalg`, and
will be fixed at the language level. Until then, use `tanh` or
`sigmoid` for hidden-layer activations.

### The random number generator is not strong

`vlinalg`'s weight initializers call `vmath.random_float`, which uses a
small linear congruential generator. The low-order bits of that
generator have short periods, and the output distribution has visible
correlation across consecutive calls. This is adequate for weight
initialization (any reasonably uniform spread of small values works)
but is not adequate for cryptographic or statistical use. A stronger
generator is a language-level change, not a library change.

### Performance

Every operation is a scalar loop with no blocking, no vectorization,
and no use of tuned kernels. The library compiles to code that the C
compiler can optimize, but it does not compile to code that uses SIMD
instructions or multithreaded BLAS. For small matrices (up to a few
thousand elements) the constant factor is negligible. For larger
matrices, the runtime will be dominated by memory traffic and will be
significantly slower than a tuned library.

If you need fast dense linear algebra, use a system BLAS through the
`extern` module system. `vlinalg` is not intended to replace one.

### No integer matrices

Every element is a `Float64`. If you need integer arithmetic, either
work directly with `Array<Int64>` or accept the floating-point
rounding that comes with representing integers as doubles.

---

## Design notes

A few choices that are worth knowing about if you plan to extend the
library.

### Why a flat buffer instead of nested arrays

A nested `Array<Array<Float64>>` would be more natural to write and
would allow each row to be independently allocated, which is sometimes
useful. The flat buffer is faster because the C backend unboxes the
entire `Array<Float64>` into a contiguous `double*`, and element
access becomes a single index computation. A nested array would have
each row as a separate boxed `VyneArray_f64`, doubling the number of
unboxing boundaries in the inner loops.

The flat representation is also the standard for interop. BLAS,
LAPACK, and most GPU libraries assume row-major or column-major flat
buffers, not nested arrays.

### Why both method and function forms

The method form on `Matrix` is the interface's natural expression: a
matrix "has" a shape, "can" be transposed, "can" be softmaxed. The
function form in `Ops`, `Activations`, and `Reductions` is what you
get when you want to treat the operations as first-class values,
compose them, or pass them to higher-order code.

Maintaining both costs one line per operation (the function simply
calls the method, or vice versa) and provides a significantly more
flexible surface. This is the same convention used by Kotlin's
collections library and by Swift's standard library.

### Why `sgd_update_inplace` is not part of the `Matrix` interface

The interface is a description of what a matrix _is_. SGD is not a
property of a matrix; it is an algorithm that happens to update one.
Keeping it as a top-level function in `Ops.vy` keeps the interface
clean and makes it obvious at the call site that the operation has
side effects.

A method `m.sgd_inplace(grad, lr)` would hide the mutation behind a
receiver call. That is a small ergonomic win and a large clarity loss.

### Why no broadcasting

Broadcasting is subtle to specify, subtle to implement, and subtle to
reason about when shapes interact across multiple operations. For a
library whose primary use case is small dense neural networks, the
explicit shapes are clearer and the additional code is negligible.
`add_bias` exists specifically because the bias-broadcast pattern is
common enough to justify its own function, but there is no general
mechanism.

If you want broadcasting, you can implement it in user code by
replicating the smaller operand into a shape-matched copy and calling
the binary operation. That gives you full control over which broadcast
rule applies.

---

## Related

- `vmath` — scalar math primitives (`sqrt`, `exp`, `tanh`, `sigmoid`,
  `random_float`, etc.). `vlinalg` dispatches every scalar function
  through this module.
- `vcolors` — ANSI color helpers. `vlinalg` uses this for its shape
  mismatch diagnostics.
- `vmem` — arena checkpoint primitive. Required for bounded memory
  usage in training loops.
- The `ml_seq` and `circle` training notes describe how `vlinalg` is
  used end-to-end in the current demo set.

---

## Version history

**0.1.0** — initial release. Types, Constructors, Ops, Activations,
Reductions, Losses. Method and function forms for the reductions and
activations. `sgd_update_inplace` added to support the `vmem`
checkpointing pattern in training loops.
````

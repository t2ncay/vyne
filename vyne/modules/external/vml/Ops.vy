# vml/Ops.vy — forward / activation / optimizer steps.
ruleset { dynamic_casting };

use "Types.vy";
use native vmath;
use external "vlin/vlin.vy";

module vml;

fn :: vml apply_activation(m :: vlin.Types.Matrix, kind :: String) -> vlin.Types.Matrix {
    if kind == "tanh"    { return vlin.apply_tanh(m); }
    if kind == "sigmoid" { return vlin.apply_sigmoid(m); }
    if kind == "relu"    { return vlin.apply_relu(m); }
    if kind == "linear"  { return m; }
    return m;
}

fn :: vml apply_activation_prime(a :: vlin.Types.Matrix, kind :: String) -> vlin.Types.Matrix {
    if kind == "tanh"    { return vlin.tanh_prime(a); }
    if kind == "sigmoid" { return vlin.sigmoid_prime(a); }
    if kind == "relu"    { return vlin.relu_prime(a); }
    if kind == "linear"  { return vlin.ones(a.row, a.col); }
    return vlin.ones(a.row, a.col);
}

fn :: vml forward(model :: vml.Types.Sequential, x :: vlin.Types.Matrix) -> vlin.Types.Matrix {
    return model.forward(x);
}

# Forward through every layer, returning all intermediate activations
# as an Array. h[0] is the first layer's output, etc.
fn :: vml forward_all(model :: vml.Types.Sequential, x :: vlin.Types.Matrix) -> Array {
    # We seed the accumulator from a one-element literal on purpose:
    # the emitter's `Array = []` empty-literal fast path defaults the
    # element type to Float64, which then tries to coerce each pushed
    # Dense struct through `.as.i64` — silently producing garbage
    # doubles instead of boxed matrices. Starting with `[first]`
    # forces element-type inference to bail (Unknown), so the
    # accumulator stays boxed and every push stores the struct
    # intact.

    n :: Int64 := model.layers.size();
    if n == 0 { return []; }

    first := model.layers[0].forward(x);
    results := [first];
    current := first;

    through i :: 1..n-1 -> loop {
        layer := model.layers[i];
        current = layer.forward(current);
        results.push(current);
    };

    return results;
}

# W -= lr * grad, in place.
# Fused forward through a single Dense layer. One pass, one allocation:
# the matmul, bias add, and activation are combined into a single kernel
# instead of chaining vlin.multiply -> vlin.add_bias -> activation.
#
# The intermediate `W_mat` binding is required, not stylistic.
# MemberAccessNode only unboxes a struct field into a native VyneArray_f64
# when the receiver is a plain VARIABLE. `m.W.data` is a nested member
# access — the receiver of `.data` is `m.W`, itself a MemberAccessNode —
# so it falls through to the boxed path and the native dispatch in
# tryEmitNativeCall is rejected silently. Binding `m.W` into a local
# first gives the emitter a VARIABLE receiver, which unboxes correctly.
#
# The `x.data` and `m.b` reads work directly because `x` is a param
# of a free function (so `emitFunctionBody` registers its struct type
# in localStructTypes) and `b` is registered as an Array<Float64>
# interface field (so getInterfaceArrayElem returns Float64).
fn :: vml dense_forward_fused(m :: vml.Types.Dense,
                               x :: vlin.Types.Matrix) -> vlin.Types.Matrix {
    W_mat  :: vlin.Types.Matrix := m.W;
    x_data :: Array<Float64>    := x.data;
    W_data :: Array<Float64>    := W_mat.data;
    bias   :: Array<Float64>    := m.b;

    M :: Int64 := x.row;
    K :: Int64 := x.col;
    N :: Int64 := W_mat.col;

    output :: Array<Float64> := vlin.zeros_f64(M * N);

    if      m.activation == "tanh"    { vml.k_matmul_bias_tanh(output, x_data, W_data, bias, M, K, N); }
    else if m.activation == "sigmoid" { vml.k_matmul_bias_sigmoid(output, x_data, W_data, bias, M, K, N); }
    else if m.activation == "relu"    { vml.k_matmul_bias_relu(output, x_data, W_data, bias, M, K, N); }
    else                              { vml.k_matmul_bias_linear(output, x_data, W_data, bias, M, K, N); }

    return vlin.from_flat(M, N, output);
}

# Same shape as forward_all, but every layer goes through the fused
# kernel. Intermediates are still returned for the backward pass.
#
# Seed the accumulator from a one-element literal — same reason as
# forward_all. The empty-literal fast path would default the element
# type to Float64 and coerce each pushed Dense struct through .as.i64.
fn :: vml forward_all_fused(model :: vml.Types.Sequential,
                             x :: vlin.Types.Matrix) -> Array {
    n :: Int64 := model.layers.size();
    if n == 0 { return []; }

    first := vml.dense_forward_fused(model.layers[0], x);
    results := [first];
    current := first;

    through i :: 1..n-1 -> loop {
        layer := model.layers[i];
        current = vml.dense_forward_fused(layer, current);
        results.push(current);
    };

    return results;
}

# W -= lr * grad, in place.
fn :: vml sgd_step(W :: vlin.Types.Matrix, grad :: vlin.Types.Matrix,
                   lr :: Float64) {
    vlin.sgd_update_inplace(W, grad, lr);
    return W;
}

# Fused Adam step. Mutates W, state.m, state.v in place. `t` is the
# global timestep, starting at 1 and incremented once per optimizer
# application (per batch, not per epoch).
fn :: vml adam_step(W :: vlin.Types.Matrix, grad :: vlin.Types.Matrix,
                    state :: vml.Types.AdamState, opt :: vml.Types.Adam,
                    t :: Int64) {
    W_data :: Array<Float64> := W.data;
    g_data :: Array<Float64> := grad.data;
    m_data :: Array<Float64> := state.m;
    v_data :: Array<Float64> := state.v;
    n :: Int64 := W.row * W.col;
    vml.k_adam_step(W_data, g_data, m_data, v_data,
                    opt.lr, opt.beta1, opt.beta2, opt.eps, t, n);
}

# Extract `batch_size` rows from `source` into a fresh matrix. The row
# indices come from a separate Array<Int64>, which the caller is
# expected to shuffle between epochs.
#
# `indices` is declared `Array` (untyped) rather than Array<Int64>
# on purpose: the parser assigns Array<Int64> parameters the CType
# `Array` with no element type (registerDeclaration sets the name but
# not the type), so a typed annotation would still box the arg at the
# call to k_gather_rows. Declaring it Array and reading each element
# through int64() costs one boxed read per row — negligible next to
# the row-sized inner loop, which runs native on src_data and out.
fn :: vml gather_rows(source :: vlin.Types.Matrix,
                      indices :: Array,
                      batch_size :: Int64) -> vlin.Types.Matrix {
    src_data :: Array<Float64> := source.data;
    cols     :: Int64 := source.col;
    n        :: Int64 := batch_size * cols;
    output      :: Array<Float64> := vlin.zeros_f64(n);

    through b :: 0..batch_size-1 -> loop {
        idx :: Int64 := int64(indices[b]);
        through j :: 0..cols-1 -> loop {
            output[b * cols + j] = src_data[idx * cols + j];
        };
    };
    return vlin.from_flat(batch_size, cols, output);
}

# In-place Fisher-Yates shuffle. `indices` must already be
# initialised to [0, 1, ..., n-1]. Uses vmath.random, so results are
# reproducible when the global RNG is seeded.
fn :: vml shuffle_indices(indices :: Array, n :: Int64) {
    through i :: 0..n-2 -> loop {
        j :: Int64 := int64(vmath.random(i, n - 1));
        tmp :: Int64 := int64(indices[i]);
        indices[i] = indices[j];
        indices[j] = tmp;
    };
    return;
}

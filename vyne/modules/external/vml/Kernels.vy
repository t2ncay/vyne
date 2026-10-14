# vml/Kernels.vy — the few ML-specific hot loops.
#
# Most numerics live in vlin/Kernels.vy. What belongs here is the
# handful of routines that only make sense in a training context.

ruleset { dynamic_casting };

use native vmath;

module vml;

# db[c] = sum over rows r of delta[r * cols + c].
# delta is row-major, shape [rows x cols].
fn :: vml k_bias_grad(output :: Array<Float64>, delta :: Array<Float64>,
                      rows :: Int64, cols :: Int64) -> Int64 {
    through c :: 0..cols-1 -> loop { output[c] = 0.0; };
    through r :: 0..rows-1 -> loop {
        through c :: 0..cols-1 -> loop {
            output[c] = output[c] + delta[r * cols + c];
        };
    };
    return 0;
}

# ============================================================================
# FUSED FORWARD: matmul + bias + activation
#
# One pass over the output, one allocation. Replaces the
# multiply -> add_bias -> activation chain in Dense.forward with a
# single kernel.
#
# All four array params are Array<Float64>; M, K, N are Int64. That
# shape makes the kernel native-ABI eligible — the call site passes
# .data and the body compiles to a bare C loop over double*.
#
# Four variants rather than an opcode, matching vlin's style
# (k_add / k_sub / k_mul / k_div are separate for the same reason).
# ============================================================================

fn :: vml k_matmul_bias_tanh(output :: Array<Float64>,
                              a :: Array<Float64>,
                              b :: Array<Float64>,
                              bias :: Array<Float64>,
                              M :: Int64, K :: Int64, N :: Int64) -> Int64 {
    through r :: 0..M-1 -> loop {
        through c :: 0..N-1 -> loop {
            acc :: Float64 := bias[c];
            through kk :: 0..K-1 -> loop {
                acc = acc + a[r * K + kk] * b[kk * N + c];
            };
            output[r * N + c] = vmath.tanh(acc);
        };
    };
    return 0;
}

fn :: vml k_matmul_bias_sigmoid(output :: Array<Float64>,
                                 a :: Array<Float64>,
                                 b :: Array<Float64>,
                                 bias :: Array<Float64>,
                                 M :: Int64, K :: Int64, N :: Int64) -> Int64 {
    through r :: 0..M-1 -> loop {
        through c :: 0..N-1 -> loop {
            acc :: Float64 := bias[c];
            through kk :: 0..K-1 -> loop {
                acc = acc + a[r * K + kk] * b[kk * N + c];
            };
            output[r * N + c] = vmath.sigmoid(acc);
        };
    };
    return 0;
}

fn :: vml k_matmul_bias_relu(output :: Array<Float64>,
                              a :: Array<Float64>,
                              b :: Array<Float64>,
                              bias :: Array<Float64>,
                              M :: Int64, K :: Int64, N :: Int64) -> Int64 {
    through r :: 0..M-1 -> loop {
        through c :: 0..N-1 -> loop {
            acc :: Float64 := bias[c];
            through kk :: 0..K-1 -> loop {
                acc = acc + a[r * K + kk] * b[kk * N + c];
            };
            output[r * N + c] = vmath.relu(acc);
        };
    };
    return 0;
}

fn :: vml k_matmul_bias_linear(output :: Array<Float64>,
                                a :: Array<Float64>,
                                b :: Array<Float64>,
                                bias :: Array<Float64>,
                                M :: Int64, K :: Int64, N :: Int64) -> Int64 {
    through r :: 0..M-1 -> loop {
        through c :: 0..N-1 -> loop {
            acc :: Float64 := bias[c];
            through kk :: 0..K-1 -> loop {
                acc = acc + a[r * K + kk] * b[kk * N + c];
            };
            output[r * N + c] = acc;
        };
    };
    return 0;
}

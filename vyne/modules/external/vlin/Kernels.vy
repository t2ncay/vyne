# vlin/Kernels.vy — hot loops.
#
# Every function here takes Array<Float64> parameters and returns
# either Int64 (a status code, always 0) or Float64. That shape makes
# them eligible for the native array ABI: the call site passes `.data`
# and the body compiles to a bare C loop over double*.
#
# Naming convention: k_ prefix. These are internal; users call the
# vlin.<name> wrappers in Ops.vy / Reductions.vy / Constructors.vy.

ruleset { dynamic_casting };

module vlin;
use native vmath;

# ============================================================================
# ELEMENT-WISE BINARY
# ============================================================================

fn :: vlin k_add(output :: Array<Float64>, a :: Array<Float64>,
                 b :: Array<Float64>, n :: Int64) -> Int64 {
    through i :: 0..n-1 -> loop { output[i] = a[i] + b[i]; };
    return 0;
}

fn :: vlin k_sub(output :: Array<Float64>, a :: Array<Float64>,
                 b :: Array<Float64>, n :: Int64) -> Int64 {
    through i :: 0..n-1 -> loop { output[i] = a[i] - b[i]; };
    return 0;
}

fn :: vlin k_mul(output :: Array<Float64>, a :: Array<Float64>,
                 b :: Array<Float64>, n :: Int64) -> Int64 {
    through i :: 0..n-1 -> loop { output[i] = a[i] * b[i]; };
    return 0;
}

fn :: vlin k_div(output :: Array<Float64>, a :: Array<Float64>,
                 b :: Array<Float64>, n :: Int64) -> Int64 {
    through i :: 0..n-1 -> loop { output[i] = a[i] / b[i]; };
    return 0;
}

# ============================================================================
# SCALAR AND UNARY
# ============================================================================

fn :: vlin k_scale(output :: Array<Float64>, a :: Array<Float64>,
                   s :: Float64, n :: Int64) -> Int64 {
    through i :: 0..n-1 -> loop { output[i] = a[i] * s; };
    return 0;
}

fn :: vlin k_add_scalar(output :: Array<Float64>, a :: Array<Float64>,
                        s :: Float64, n :: Int64) -> Int64 {
    through i :: 0..n-1 -> loop { output[i] = a[i] + s; };
    return 0;
}

fn :: vlin k_fill(output :: Array<Float64>, v :: Float64, n :: Int64) -> Int64 {
    through i :: 0..n-1 -> loop { output[i] = v; };
    return 0;
}

fn :: vlin k_copy(output :: Array<Float64>, a :: Array<Float64>, n :: Int64) -> Int64 {
    through i :: 0..n-1 -> loop { output[i] = a[i]; };
    return 0;
}

fn :: vlin k_neg(output :: Array<Float64>, a :: Array<Float64>, n :: Int64) -> Int64 {
    through i :: 0..n-1 -> loop { output[i] = 0.0 - a[i]; };
    return 0;
}

# ============================================================================
# REDUCTIONS
# ============================================================================

fn :: vlin k_sum(a :: Array<Float64>, n :: Int64) -> Float64 {
    total :: Float64 := 0.0;
    through i :: 0..n-1 -> loop { total = total + a[i]; };
    return total;
}

fn :: vlin k_dot(a :: Array<Float64>, b :: Array<Float64>, n :: Int64) -> Float64 {
    total :: Float64 := 0.0;
    through i :: 0..n-1 -> loop { total = total + a[i] * b[i]; };
    return total;
}

fn :: vlin k_norm_sq(a :: Array<Float64>, n :: Int64) -> Float64 {
    total :: Float64 := 0.0;
    through i :: 0..n-1 -> loop { total = total + a[i] * a[i]; };
    return total;
}

fn :: vlin k_max(a :: Array<Float64>, n :: Int64) -> Float64 {
    if n == 0 { return 0.0; }
    best :: Float64 := a[0];
    through i :: 1..n-1 -> loop {
        v :: Float64 := a[i];
        if v > best { best = v; }
    };
    return best;
}

fn :: vlin k_min(a :: Array<Float64>, n :: Int64) -> Float64 {
    if n == 0 { return 0.0; }
    best :: Float64 := a[0];
    through i :: 1..n-1 -> loop {
        v :: Float64 := a[i];
        if v < best { best = v; }
    };
    return best;
}

# ============================================================================
# MATRIX PRODUCTS
# ============================================================================

fn :: vlin k_matmul(output :: Array<Float64>,
                    a :: Array<Float64>, b :: Array<Float64>,
                    M :: Int64, K :: Int64, N :: Int64) -> Int64 {
    through r :: 0..M-1 -> loop {
        through c :: 0..N-1 -> loop {
            acc :: Float64 := 0.0;
            through kk :: 0..K-1 -> loop {
                acc = acc + a[r * K + kk] * b[kk * N + c];
            };
            output[r * N + c] = acc;
        };
    };
    return 0;
}

# output = a.T @ b. a is K x M (row-major), b is K x N, output is M x N.
# The a read strides by M, not by 1 — this is the non-contiguous operand.
fn :: vlin k_matmul_trans_a(output :: Array<Float64>,
                            a :: Array<Float64>, b :: Array<Float64>,
                            M :: Int64, K :: Int64, N :: Int64) -> Int64 {
    through m :: 0..M-1 -> loop {
        through n :: 0..N-1 -> loop {
            acc :: Float64 := 0.0;
            through kk :: 0..K-1 -> loop {
                acc = acc + a[kk * M + m] * b[kk * N + n];
            };
            output[m * N + n] = acc;
        };
    };
    return 0;
}

# output = a @ b.T. Both inner reads are contiguous in K.
fn :: vlin k_matmul_trans_b(output :: Array<Float64>,
                            a :: Array<Float64>, b :: Array<Float64>,
                            M :: Int64, K :: Int64, N :: Int64) -> Int64 {
    through r :: 0..M-1 -> loop {
        through c :: 0..N-1 -> loop {
            acc :: Float64 := 0.0;
            through kk :: 0..K-1 -> loop {
                acc = acc + a[r * K + kk] * b[c * K + kk];
            };
            output[r * N + c] = acc;
        };
    };
    return 0;
}

# ============================================================================
# TRANSPOSE
# ============================================================================

fn :: vlin k_transpose(output :: Array<Float64>, a :: Array<Float64>,
                       R :: Int64, C :: Int64) -> Int64 {
    through r :: 0..R-1 -> loop {
        through c :: 0..C-1 -> loop {
            output[c * R + r] = a[r * C + c];
        };
    };
    return 0;
}

# vlin/Ops.vy — matrix-level operations, one kernel call per op.

use "Types.vy";
use "Kernels.vy";
use "Constructors.vy";
use external "vcolors.vy";

ruleset { dynamic_casting };

module vlin;
use native vmath;

# ============================================================================
# ELEMENT-WISE BINARY
# ============================================================================

fn :: vlin add(a :: vlin.Types.Matrix, b :: vlin.Types.Matrix) -> vlin.Types.Matrix {
    if a.row != b.row || a.col != b.col {
        out(vcolors.red("vlin.add: shape mismatch"));
        return vlin.zeros(0, 0);
    }
    n :: Int64 := a.row * a.col;
    output_data :: Array<Float64> := vlin.zeros_f64(n);
    vlin.k_add(output_data, a.data, b.data, n);
    return vlin.Types.Matrix(a.row, a.col, output_data);
}

fn :: vlin subtract(a :: vlin.Types.Matrix, b :: vlin.Types.Matrix) -> vlin.Types.Matrix {
    if a.row != b.row || a.col != b.col {
        out(vcolors.red("vlin.subtract: shape mismatch"));
        return vlin.zeros(0, 0);
    }
    n :: Int64 := a.row * a.col;
    output_data :: Array<Float64> := vlin.zeros_f64(n);
    vlin.k_sub(output_data, a.data, b.data, n);
    return vlin.Types.Matrix(a.row, a.col, output_data);
}

fn :: vlin hadamard(a :: vlin.Types.Matrix, b :: vlin.Types.Matrix) -> vlin.Types.Matrix {
    if a.row != b.row || a.col != b.col {
        out(vcolors.red("vlin.hadamard: shape mismatch"));
        return vlin.zeros(0, 0);
    }
    n :: Int64 := a.row * a.col;
    output_data :: Array<Float64> := vlin.zeros_f64(n);
    vlin.k_mul(output_data, a.data, b.data, n);
    return vlin.Types.Matrix(a.row, a.col, output_data);
}

# ============================================================================
# MATRIX PRODUCT
# ============================================================================

fn :: vlin multiply(a :: vlin.Types.Matrix, b :: vlin.Types.Matrix) -> vlin.Types.Matrix {
    if a.col != b.row {
        out(vcolors.red("vlin.multiply: shape mismatch"));
        return vlin.zeros(0, 0);
    }
    ar :: Int64 := a.row;
    ac :: Int64 := a.col;
    bc :: Int64 := b.col;
    output_data :: Array<Float64> := vlin.zeros_f64(ar * bc);
    vlin.k_matmul(output_data, a.data, b.data, ar, ac, bc);
    return vlin.Types.Matrix(ar, bc, output_data);
}

# a.T @ b. The backward-pass form of the dense layer:
#   dW = h.T @ delta
# replaces `multiply(transpose(h), delta)`, which allocated the
# transposed copy and then ran a full NN dgemm on it.
fn :: vlin trans_a_multiply(a :: vlin.Types.Matrix,
                            b :: vlin.Types.Matrix) -> vlin.Types.Matrix {
    if a.row != b.row {
        out(vcolors.red("vlin.trans_a_multiply: shape mismatch"));
        return vlin.zeros(0, 0);
    }
    M :: Int64 := a.col;
    K :: Int64 := a.row;
    N :: Int64 := b.col;
    output_data :: Array<Float64> := vlin.zeros_f64(M * N);
    vlin.k_matmul_trans_a(output_data, a.data, b.data, M, K, N);
    return vlin.Types.Matrix(M, N, output_data);
}

# a @ b.T. Useful for the backward pass of a dense layer.
fn :: vlin multiply_trans_b(a :: vlin.Types.Matrix,
                            b :: vlin.Types.Matrix) -> vlin.Types.Matrix {
    if a.col != b.col {
        out(vcolors.red("vlin.multiply_trans_b: shape mismatch"));
        return vlin.zeros(0, 0);
    }
    M :: Int64 := a.row;
    K :: Int64 := a.col;
    N :: Int64 := b.row;
    output_data :: Array<Float64> := vlin.zeros_f64(M * N);
    vlin.k_matmul_trans_b(output_data, a.data, b.data, M, K, N);
    return vlin.Types.Matrix(M, N, output_data);
}

fn :: vlin transpose(m :: vlin.Types.Matrix) -> vlin.Types.Matrix {
    r :: Int64 := m.row;
    c :: Int64 := m.col;
    output_data :: Array<Float64> := vlin.zeros_f64(r * c);
    vlin.k_transpose(output_data, m.data, r, c);
    return vlin.Types.Matrix(c, r, output_data);
}

# ============================================================================
# SCALAR
# ============================================================================

fn :: vlin add_scalar(m :: vlin.Types.Matrix, s :: Float64) -> vlin.Types.Matrix {
    n :: Int64 := m.row * m.col;
    output_data :: Array<Float64> := vlin.zeros_f64(n);
    vlin.k_add_scalar(output_data, m.data, s, n);
    return vlin.Types.Matrix(m.row, m.col, output_data);
}

fn :: vlin multiply_scalar(m :: vlin.Types.Matrix, s :: Float64) -> vlin.Types.Matrix {
    n :: Int64 := m.row * m.col;
    output_data :: Array<Float64> := vlin.zeros_f64(n);
    vlin.k_scale(output_data, m.data, s, n);
    return vlin.Types.Matrix(m.row, m.col, output_data);
}

fn :: vlin clip(m :: vlin.Types.Matrix, lo :: Float64, hi :: Float64) -> vlin.Types.Matrix {
    r :: Int64 := m.row;
    c :: Int64 := m.col;
    n :: Int64 := r * c;
    output_data :: Array<Float64> := vlin.zeros_f64(n);
    through i :: 0..n-1 -> loop {
        output_data[i] = vmath.clamp(m.data[i], lo, hi);
    };
    return vlin.Types.Matrix(r, c, output_data);
}

fn :: vlin add_bias(m :: vlin.Types.Matrix, b :: Array<Float64>) -> vlin.Types.Matrix {
    r :: Int64 := m.row;
    c :: Int64 := m.col;
    n :: Int64 := r * c;
    output_data :: Array<Float64> := vlin.zeros_f64(n);
    through i :: 0..r-1 -> loop {
        through j :: 0..c-1 -> loop {
            output_data[i * c + j] = m.data[i * c + j] + b[j];
        };
    };
    return vlin.Types.Matrix(r, c, output_data);
}

# ============================================================================
# IN-PLACE VARIANTS (destination is pre-allocated)
# ============================================================================

fn :: vlin add_into(output :: vlin.Types.Matrix,
                    a :: vlin.Types.Matrix, b :: vlin.Types.Matrix) -> Int64 {
    if output.row != a.row || output.col != a.col ||
       a.row != b.row || a.col != b.col {
        out(vcolors.red("vlin.add_into: shape mismatch"));
        return -1;
    }
    n :: Int64 := output.row * output.col;
    vlin.k_add(output.data, a.data, b.data, n);
    return 0;
}

fn :: vlin multiply_into(output :: vlin.Types.Matrix,
                         a :: vlin.Types.Matrix, b :: vlin.Types.Matrix) -> Int64 {
    if output.row != a.row || output.col != b.col || a.col != b.row {
        out(vcolors.red("vlin.multiply_into: shape mismatch"));
        return -1;
    }
    M :: Int64 := output.row;
    K :: Int64 := a.col;
    N :: Int64 := output.col;
    vlin.k_matmul(output.data, a.data, b.data, M, K, N);
    return 0;
}

fn :: vlin add_bias_into(m :: vlin.Types.Matrix, b :: Array<Float64>) -> Int64 {
    r :: Int64 := m.row;
    c :: Int64 := m.col;
    through i :: 0..r-1 -> loop {
        through j :: 0..c-1 -> loop {
            m.data[i * c + j] = m.data[i * c + j] + b[j];
        };
    };
    return 0;
}

# ============================================================================
# STACKING
# ============================================================================

fn :: vlin vstack(a :: vlin.Types.Matrix, b :: vlin.Types.Matrix) -> vlin.Types.Matrix {
    if a.col != b.col {
        out(vcolors.red("vlin.vstack: column mismatch"));
        return vlin.zeros(0, 0);
    }
    c :: Int64 := a.col;
    n :: Int64 := a.row * c;
    m :: Int64 := b.row * c;
    output_data :: Array<Float64> := vlin.zeros_f64(n + m);
    through i :: 0..n-1 -> loop { output_data[i] = a.data[i]; };
    through i :: 0..m-1 -> loop { output_data[n + i] = b.data[i]; };
    return vlin.Types.Matrix(a.row + b.row, c, output_data);
}

fn :: vlin hstack(a :: vlin.Types.Matrix, b :: vlin.Types.Matrix) -> vlin.Types.Matrix {
    if a.row != b.row {
        out(vcolors.red("vlin.hstack: row mismatch"));
        return vlin.zeros(0, 0);
    }
    r :: Int64 := a.row;
    ac :: Int64 := a.col;
    bc :: Int64 := b.col;
    output_data :: Array<Float64> := vlin.zeros_f64(r * (ac + bc));
    through i :: 0..r-1 -> loop {
        through j :: 0..ac-1 -> loop {
            output_data[i * (ac + bc) + j] = a.data[i * ac + j];
        };
        through j :: 0..bc-1 -> loop {
            output_data[i * (ac + bc) + ac + j] = b.data[i * bc + j];
        };
    };
    return vlin.Types.Matrix(r, ac + bc, output_data);
}

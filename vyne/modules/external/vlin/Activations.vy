# vlin/Activations.vy — element-wise activation functions and their
# derivatives.
#
# The *_prime functions take the *post-activation* matrix A and return
# dA/dz evaluated pointwise. This matches the manual backprop that
# ml_seq.vy does: delta_k = delta_{k+1} ⊙ f'(A_k).

use "Types.vy";
use "Kernels.vy";
use "Constructors.vy";

ruleset { dynamic_casting };

module vlin;
use native vmath;

# ---- sigmoid --------------------------------------------------------------

fn :: vlin apply_sigmoid(m :: vlin.Types.Matrix) -> vlin.Types.Matrix {
    n :: Int64 := m.row * m.col;
    output :: Array<Float64> := vlin.zeros_f64(n);
    through i :: 0..n-1 -> loop {
        output[i] = vmath.sigmoid(m.data[i]);
    };
    return vlin.Types.Matrix(m.row, m.col, output);
}

fn :: vlin sigmoid_prime(m :: vlin.Types.Matrix) -> vlin.Types.Matrix {
    n :: Int64 := m.row * m.col;
    output :: Array<Float64> := vlin.zeros_f64(n);
    through i :: 0..n-1 -> loop {
        a :: Float64 := m.data[i];
        output[i] = a * (1.0 - a);
    };
    return vlin.Types.Matrix(m.row, m.col, output);
}

# ---- tanh -----------------------------------------------------------------

fn :: vlin apply_tanh(m :: vlin.Types.Matrix) -> vlin.Types.Matrix {
    n :: Int64 := m.row * m.col;
    output :: Array<Float64> := vlin.zeros_f64(n);
    through i :: 0..n-1 -> loop {
        output[i] = vmath.tanh(m.data[i]);
    };
    return vlin.Types.Matrix(m.row, m.col, output);
}

fn :: vlin tanh_prime(m :: vlin.Types.Matrix) -> vlin.Types.Matrix {
    n :: Int64 := m.row * m.col;
    output :: Array<Float64> := vlin.zeros_f64(n);
    through i :: 0..n-1 -> loop {
        a :: Float64 := m.data[i];
        output[i] = 1.0 - a * a;
    };
    return vlin.Types.Matrix(m.row, m.col, output);
}

# ---- relu -----------------------------------------------------------------

fn :: vlin apply_relu(m :: vlin.Types.Matrix) -> vlin.Types.Matrix {
    n :: Int64 := m.row * m.col;
    output :: Array<Float64> := vlin.zeros_f64(n);
    through i :: 0..n-1 -> loop {
        v :: Float64 := m.data[i];
        if v > 0.0 { output[i] = v; } else { output[i] = 0.0; }
    };
    return vlin.Types.Matrix(m.row, m.col, output);
}

fn :: vlin relu_prime(m :: vlin.Types.Matrix) -> vlin.Types.Matrix {
    n :: Int64 := m.row * m.col;
    output :: Array<Float64> := vlin.zeros_f64(n);
    through i :: 0..n-1 -> loop {
        v :: Float64 := m.data[i];
        if v > 0.0 { output[i] = 1.0; } else { output[i] = 0.0; }
    };
    return vlin.Types.Matrix(m.row, m.col, output);
}

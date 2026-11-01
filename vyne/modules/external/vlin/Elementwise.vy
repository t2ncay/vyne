# vlin/Elementwise.vy — element-wise math operators.
#
# Everything here takes a Matrix, applies an element-wise function to
# every element, and returns a fresh Matrix of the same shape. No
# shape validation is needed for the unary functions (input and output
# shapes are identical by construction); the binary ones check.
#
# The `*_into` forms live in Ops.vy alongside the other in-place ops,
# because they need a caller-provided destination matrix and belong
# with the rest of the allocation-free surface.

use "Types.vy";
use "Kernels.vy";
use "Constructors.vy";

ruleset { dynamic_casting };

module vlin;
use native vmath;

# ---- unary ---------------------------------------------------------------

fn :: vlin abs(m :: vlin.Types.Matrix) -> vlin.Types.Matrix {
    n :: Int64 := m.row * m.col;
    output_data :: Array<Float64> := vlin.zeros_f64(n);
    vlin.k_abs(output_data, m.data, n);
    return vlin.Types.Matrix(m.row, m.col, output_data);
}

fn :: vlin sqrt(m :: vlin.Types.Matrix) -> vlin.Types.Matrix {
    n :: Int64 := m.row * m.col;
    output_data :: Array<Float64> := vlin.zeros_f64(n);
    vlin.k_sqrt(output_data, m.data, n);
    return vlin.Types.Matrix(m.row, m.col, output_data);
}

fn :: vlin exp(m :: vlin.Types.Matrix) -> vlin.Types.Matrix {
    n :: Int64 := m.row * m.col;
    output_data :: Array<Float64> := vlin.zeros_f64(n);
    vlin.k_exp(output_data, m.data, n);
    return vlin.Types.Matrix(m.row, m.col, output_data);
}

fn :: vlin log(m :: vlin.Types.Matrix) -> vlin.Types.Matrix {
    n :: Int64 := m.row * m.col;
    output_data :: Array<Float64> := vlin.zeros_f64(n);
    vlin.k_log(output_data, m.data, n);
    return vlin.Types.Matrix(m.row, m.col, output_data);
}

fn :: vlin pow_scalar(m :: vlin.Types.Matrix, p :: Float64) -> vlin.Types.Matrix {
    n :: Int64 := m.row * m.col;
    output_data :: Array<Float64> := vlin.zeros_f64(n);
    vlin.k_pow_scalar(output_data, m.data, p, n);
    return vlin.Types.Matrix(m.row, m.col, output_data);
}

# ---- binary (shape-checked) ----------------------------------------------

fn :: vlin pow_elementwise(a :: vlin.Types.Matrix,
                           b :: vlin.Types.Matrix) -> vlin.Types.Matrix {
    if a.row != b.row || a.col != b.col {
        throw "vlin.pow_elementwise: shape mismatch: a is "
            + string(a.row) + "x" + string(a.col)
            + ", b is " + string(b.row) + "x" + string(b.col);
    }
    n :: Int64 := a.row * a.col;
    output_data :: Array<Float64> := vlin.zeros_f64(n);
    vlin.k_pow_binary(output_data, a.data, b.data, n);
    return vlin.Types.Matrix(a.row, a.col, output_data);
}

# Element-wise max/min. vmath.clamp needs both lo and hi; these are
# the "give me just the top / bottom of two operands" primitive,
# useful for leaky relu (max(x, alpha*x)) and numerical clipping.
fn :: vlin max_elementwise(a :: vlin.Types.Matrix,
                           b :: vlin.Types.Matrix) -> vlin.Types.Matrix {
    if a.row != b.row || a.col != b.col {
        throw "vlin.max_elementwise: shape mismatch: a is "
            + string(a.row) + "x" + string(a.col)
            + ", b is " + string(b.row) + "x" + string(b.col);
    }
    n :: Int64 := a.row * a.col;
    output_data :: Array<Float64> := vlin.zeros_f64(n);
    through i :: 0..n-1 -> loop {
        if a.data[i] > b.data[i] { output_data[i] = a.data[i]; }
        else                     { output_data[i] = b.data[i]; }
    };
    return vlin.Types.Matrix(a.row, a.col, output_data);
}

fn :: vlin min_elementwise(a :: vlin.Types.Matrix,
                           b :: vlin.Types.Matrix) -> vlin.Types.Matrix {
    if a.row != b.row || a.col != b.col {
        throw "vlin.min_elementwise: shape mismatch: a is "
            + string(a.row) + "x" + string(a.col)
            + ", b is " + string(b.row) + "x" + string(b.col);
    }
    n :: Int64 := a.row * a.col;
    output_data :: Array<Float64> := vlin.zeros_f64(n);
    through i :: 0..n-1 -> loop {
        if a.data[i] < b.data[i] { output_data[i] = a.data[i]; }
        else                     { output_data[i] = b.data[i]; }
    };
    return vlin.Types.Matrix(a.row, a.col, output_data);
}

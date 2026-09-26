# vlinalg/Activations.vy — non-linearities and their derivatives.

use "Types.vy";

ruleset {
    dynamic_casting
};

module vlinalg;
module vmath;

# ============================================================================
# FORWARD PASS
# ============================================================================

fn :: vlinalg apply_sigmoid(m :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    n :: Int64 = m.row * m.col;
    md :: Array<Float64> = m.data;
    out_data :: Array = [];
    through i :: 0..n-1 -> loop {
        out_data.push(vmath.sigmoid(md[i]));
    };
    return vlinalg.Types.Matrix(m.row, m.col, out_data);
}

fn :: vlinalg apply_tanh(m :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    n :: Int64 = m.row * m.col;
    md :: Array<Float64> = m.data;
    out_data :: Array = [];
    through i :: 0..n-1 -> loop {
        out_data.push(vmath.tanh(md[i]));
    };
    return vlinalg.Types.Matrix(m.row, m.col, out_data);
}

fn :: vlinalg apply_relu(m :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    n :: Int64 = m.row * m.col;
    md :: Array<Float64> = m.data;
    out_data :: Array = [];
    through i :: 0..n-1 -> loop {
        out_data.push(vmath.relu(md[i]));
    };
    return vlinalg.Types.Matrix(m.row, m.col, out_data);
}

fn :: vlinalg apply_exp(m :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    n :: Int64 = m.row * m.col;
    md :: Array<Float64> = m.data;
    out_data :: Array = [];
    through i :: 0..n-1 -> loop {
        out_data.push(vmath.exp(md[i]));
    };
    return vlinalg.Types.Matrix(m.row, m.col, out_data);
}

fn :: vlinalg apply_log(m :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    n :: Int64 = m.row * m.col;
    md :: Array<Float64> = m.data;
    out_data :: Array = [];
    through i :: 0..n-1 -> loop {
        out_data.push(vmath.log(md[i]));
    };
    return vlinalg.Types.Matrix(m.row, m.col, out_data);
}

# ============================================================================
# DERIVATIVES (arguments are POST-activation values)
# ============================================================================

fn :: vlinalg sigmoid_prime(m :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    n :: Int64 = m.row * m.col;
    md :: Array<Float64> = m.data;
    out_data :: Array = [];
    through i :: 0..n-1 -> loop {
        a :: Float64 = md[i];
        out_data.push(a * (1.0 - a));
    };
    return vlinalg.Types.Matrix(m.row, m.col, out_data);
}

fn :: vlinalg tanh_prime(m :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    n :: Int64 = m.row * m.col;
    md :: Array<Float64> = m.data;
    out_data :: Array = [];
    through i :: 0..n-1 -> loop {
        v :: Float64 = md[i];
        out_data.push(1.0 - v * v);
    };
    return vlinalg.Types.Matrix(m.row, m.col, out_data);
}

fn :: vlinalg relu_prime(m :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    n :: Int64 = m.row * m.col;
    md :: Array<Float64> = m.data;
    out_data :: Array = [];
    through i :: 0..n-1 -> loop {
        v :: Float64 = md[i];
        if v > 0.0 {
            out_data.push(1.0);
        } else {
            out_data.push(0.0);
        }
    };
    return vlinalg.Types.Matrix(m.row, m.col, out_data);
}

# ============================================================================
# SOFTMAX / NORMALIZE (top-level)
# ============================================================================

fn :: vlinalg softmax(m :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    r :: Int64 = m.row;
    c :: Int64 = m.col;
    md :: Array<Float64> = m.data;
    out_data :: Array = [];
    through i :: 0..r-1 -> loop {
        base :: Int64 = i * c;
        row_max :: Float64 = md[base];
        through j :: 1..c-1 -> loop {
            v :: Float64 = md[base + j];
            if v > row_max { row_max = v; }
        };
        start_idx :: Int64 = out_data.size();
        denom :: Float64 = 0.0;
        through j :: 0..c-1 -> loop {
            e :: Float64 = vmath.exp(md[base + j] - row_max);
            out_data.push(e);
            denom = denom + e;
        };
        through j :: 0..c-1 -> loop {
            out_data[start_idx + j] = out_data[start_idx + j] / denom;
        };
    };
    return vlinalg.Types.Matrix(r, c, out_data);
}

fn :: vlinalg normalize(m :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    nrm :: Float64 = vlinalg.norm_fro(m);
    if nrm == 0.0 { nrm = 1.0; }
    return vlinalg.multiply_scalar(m, 1.0 / nrm);
}
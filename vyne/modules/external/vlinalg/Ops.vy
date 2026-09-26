# vlinalg/Ops.vy — core matrix arithmetic and shape ops.

use "Types.vy";
use lib "vcolors.vy";

ruleset {
    dynamic_casting
};

module vlinalg;
module vmath;

# ============================================================================
# ELEMENT-WISE BINARY
# ============================================================================

fn :: vlinalg add(a :: vlinalg.Types.Matrix, b :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    if a.row != b.row || a.col != b.col {
        out(vcolors.red("Matrix Error: Dimensions must match for addition."));
        return vlinalg.Types.Matrix(0, 0, []);
    }
    n :: Int64 = a.row * a.col;
    ad :: Array<Float64> = a.data;
    bd :: Array<Float64> = b.data;
    out_data :: Array = [];
    through i :: 0..n-1 -> loop {
        out_data.push(ad[i] + bd[i]);
    };
    return vlinalg.Types.Matrix(a.row, a.col, out_data);
}

fn :: vlinalg subtract(a :: vlinalg.Types.Matrix, b :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    if a.row != b.row || a.col != b.col {
        out(vcolors.red("Matrix Error: Dimensions must match for subtraction."));
        return vlinalg.Types.Matrix(0, 0, []);
    }
    n :: Int64 = a.row * a.col;
    ad :: Array<Float64> = a.data;
    bd :: Array<Float64> = b.data;
    out_data :: Array = [];
    through i :: 0..n-1 -> loop {
        out_data.push(ad[i] - bd[i]);
    };
    return vlinalg.Types.Matrix(a.row, a.col, out_data);
}

fn :: vlinalg hadamard(a :: vlinalg.Types.Matrix, b :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    if a.row != b.row || a.col != b.col {
        out(vcolors.red("Vlinalg Error: Hadamard dimensions mismatch"));
        return vlinalg.Types.Matrix(0, 0, []);
    }
    n :: Int64 = a.row * a.col;
    ad :: Array<Float64> = a.data;
    bd :: Array<Float64> = b.data;
    out_data :: Array = [];
    through i :: 0..n-1 -> loop {
        out_data.push(ad[i] * bd[i]);
    };
    return vlinalg.Types.Matrix(a.row, a.col, out_data);
}

# ============================================================================
# MATRIX PRODUCT
# ============================================================================

fn :: vlinalg multiply(a :: vlinalg.Types.Matrix, b :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    if a.col != b.row {
        out(vcolors.red("Matrix Error: Multiplication impossible (a.col != b.row)"));
        return vlinalg.Types.Matrix(0, 0, []);
    }
    ar :: Int64 = a.row;
    ac :: Int64 = a.col;
    bc :: Int64 = b.col;
    ad :: Array<Float64> = a.data;
    bd :: Array<Float64> = b.data;
    out_data :: Array = [];
    through r :: 0..ar-1 -> loop {
        through c :: 0..bc-1 -> loop {
            acc :: Float64 = 0.0;
            through k :: 0..ac-1 -> loop {
                acc = acc + ad[r * ac + k] * bd[k * bc + c];
            };
            out_data.push(acc);
        };
    };
    return vlinalg.Types.Matrix(ar, bc, out_data);
}

fn :: vlinalg transpose(m :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    return m.transposed();
}

# ============================================================================
# SCALAR OPS
# ============================================================================

fn :: vlinalg add_scalar(m :: vlinalg.Types.Matrix, scalar :: Float64) -> vlinalg.Types.Matrix {
    n :: Int64 = m.row * m.col;
    md :: Array<Float64> = m.data;
    out_data :: Array = [];
    through i :: 0..n-1 -> loop {
        out_data.push(md[i] + scalar);
    };
    return vlinalg.Types.Matrix(m.row, m.col, out_data);
}

fn :: vlinalg multiply_scalar(m :: vlinalg.Types.Matrix, scalar :: Float64) -> vlinalg.Types.Matrix {
    n :: Int64 = m.row * m.col;
    md :: Array<Float64> = m.data;
    out_data :: Array = [];
    through i :: 0..n-1 -> loop {
        out_data.push(md[i] * scalar);
    };
    return vlinalg.Types.Matrix(m.row, m.col, out_data);
}

fn :: vlinalg clip(m :: vlinalg.Types.Matrix, lo :: Float64, hi :: Float64) -> vlinalg.Types.Matrix {
    n :: Int64 = m.row * m.col;
    md :: Array<Float64> = m.data;
    out_data :: Array = [];
    through i :: 0..n-1 -> loop {
        out_data.push(vmath.clamp(md[i], lo, hi));
    };
    return vlinalg.Types.Matrix(m.row, m.col, out_data);
}

fn :: vlinalg add_bias(m :: vlinalg.Types.Matrix, b :: Array) -> vlinalg.Types.Matrix {
    r :: Int64 = m.row;
    c :: Int64 = m.col;
    md :: Array<Float64> = m.data;
    out_data :: Array = [];
    through i :: 0..r-1 -> loop {
        through j :: 0..c-1 -> loop {
            out_data.push(md[i * c + j] + float64(b[j]));
        };
    };
    return vlinalg.Types.Matrix(r, c, out_data);
}

# ============================================================================
# VECTOR PRODUCTS
# ============================================================================

fn :: vlinalg dot(a :: vlinalg.Types.Matrix, b :: vlinalg.Types.Matrix) -> Float64 {
    ad :: Array<Float64> = a.data;
    bd :: Array<Float64> = b.data;

    if a.row == 1 && b.row == 1 {
        total :: Float64 = 0.0;
        through c :: 0..a.col-1 -> loop {
            total = total + ad[c] * bd[c];
        };
        return total;
    }
    if a.col == 1 && b.col == 1 {
        total :: Float64 = 0.0;
        through r :: 0..a.row-1 -> loop {
            total = total + ad[r] * bd[r];
        };
        return total;
    }
    if a.row == 1 && b.col == 1 {
        total :: Float64 = 0.0;
        through c :: 0..a.col-1 -> loop {
            total = total + ad[c] * bd[c * b.col];
        };
        return total;
    }
    if a.col == 1 && b.row == 1 {
        total :: Float64 = 0.0;
        through r :: 0..a.row-1 -> loop {
            total = total + ad[r] * bd[r];
        };
        return total;
    }
    out(vcolors.red("Matrix Error: dot() requires vectors"));
    return 0.0;
}

fn :: vlinalg outer(a :: vlinalg.Types.Matrix, b :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    n :: Int64 = a.row;
    m :: Int64 = b.row;
    ad :: Array<Float64> = a.data;
    bd :: Array<Float64> = b.data;
    out_data :: Array = [];
    through i :: 0..n-1 -> loop {
        through j :: 0..m-1 -> loop {
            out_data.push(ad[i] * bd[j]);
        };
    };
    return vlinalg.Types.Matrix(n, m, out_data);
}

# ============================================================================
# STACKING
# ============================================================================

fn :: vlinalg vstack(a :: vlinalg.Types.Matrix, b :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    if a.col != b.col {
        out(vcolors.red("Matrix Error: vstack column mismatch"));
        return vlinalg.Types.Matrix(0, 0, []);
    }
    c :: Int64 = a.col;
    n :: Int64 = a.row * c;
    m :: Int64 = b.row * c;
    ad :: Array<Float64> = a.data;
    bd :: Array<Float64> = b.data;
    out_data :: Array = [];
    through i :: 0..n-1 -> loop { out_data.push(ad[i]); };
    through i :: 0..m-1 -> loop { out_data.push(bd[i]); };
    return vlinalg.Types.Matrix(a.row + b.row, c, out_data);
}

fn :: vlinalg hstack(a :: vlinalg.Types.Matrix, b :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    if a.row != b.row {
        out(vcolors.red("Matrix Error: hstack row mismatch"));
        return vlinalg.Types.Matrix(0, 0, []);
    }
    r :: Int64 = a.row;
    ac :: Int64 = a.col;
    bc :: Int64 = b.col;
    ad :: Array<Float64> = a.data;
    bd :: Array<Float64> = b.data;
    out_data :: Array = [];
    through i :: 0..r-1 -> loop {
        through j :: 0..ac-1 -> loop {
            out_data.push(ad[i * ac + j]);
        };
        through j :: 0..bc-1 -> loop {
            out_data.push(bd[i * bc + j]);
        };
    };
    return vlinalg.Types.Matrix(r, ac + bc, out_data);
}

# inside vlinalg/Ops.vy
fn :: vlinalg sgd_update_inplace(W :: vlinalg.Types.Matrix, grad :: vlinalg.Types.Matrix, lr :: Float64) {
    n :: Int64 = W.row * W.col;
    wd :: Array<Float64> = W.data;
    gd :: Array<Float64> = grad.data;
    through i :: 0..n-1 -> loop {
        wd[i] = wd[i] - lr * gd[i];
    };
}
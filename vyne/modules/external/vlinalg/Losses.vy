# vlinalg/Losses.vy — mean squared error and cross-entropy.

use "Types.vy";

ruleset {
    dynamic_casting
};

module vlinalg;
module vmath;

# ============================================================================
# MSE
# ============================================================================

fn :: vlinalg mse(pred :: vlinalg.Types.Matrix,
                  target :: vlinalg.Types.Matrix) -> Float64 {
    n :: Int64 = pred.row * pred.col;
    if n == 0 { return 0.0; }
    pd :: Array<Float64> = pred.data;
    td :: Array<Float64> = target.data;
    total :: Float64 = 0.0;
    through i :: 0..n-1 -> loop {
        d :: Float64 = pd[i] - td[i];
        total = total + d * d;
    };
    return total / float64(n);
}

fn :: vlinalg mse_prime(pred :: vlinalg.Types.Matrix,
                        target :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    n :: Int64 = pred.row * pred.col;
    pd :: Array<Float64> = pred.data;
    td :: Array<Float64> = target.data;
    fn_n :: Float64 = float64(n);
    out_data :: Array = [];
    through i :: 0..n-1 -> loop {
        out_data.push(2.0 * (pd[i] - td[i]) / fn_n);
    };
    return vlinalg.Types.Matrix(pred.row, pred.col, out_data);
}

# ============================================================================
# CROSS-ENTROPY  (binary per-element)
# ============================================================================

fn :: vlinalg cross_entropy(pred :: vlinalg.Types.Matrix,
                            target :: vlinalg.Types.Matrix) -> Float64 {
    r :: Int64 = pred.row;
    n :: Int64 = r * pred.col;
    if r == 0 { return 0.0; }
    eps :: Float64 = 0.000000001;
    pd :: Array<Float64> = pred.data;
    td :: Array<Float64> = target.data;
    total :: Float64 = 0.0;
    through i :: 0..n-1 -> loop {
        p :: Float64 = vmath.clamp(pd[i], eps, 1.0 - eps);
        y :: Float64 = td[i];
        total = total - (y * vmath.log(p) + (1.0 - y) * vmath.log(1.0 - p));
    };
    return total / float64(r);
}

fn :: vlinalg cross_entropy_prime(pred :: vlinalg.Types.Matrix,
                                  target :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    n :: Int64 = pred.row * pred.col;
    pd :: Array<Float64> = pred.data;
    td :: Array<Float64> = target.data;
    out_data :: Array = [];
    through i :: 0..n-1 -> loop {
        out_data.push(pd[i] - td[i]);
    };
    return vlinalg.Types.Matrix(pred.row, pred.col, out_data);
}
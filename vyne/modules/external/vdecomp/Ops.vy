# vdecomp/Ops.vy - matrix-shaped convenience wrappers.
#
# The raw routines take flat arrays plus dimensions. These wrappers take
# Matrix/Series structs, validate shape, and return the same result types.
# They are the easiest entry point for code that uses vlin-style matrices:
#   vdecomp.Matrix(w.data, w.row, w.col)

use "Types.vy";
use "Util.vy";
use "LU.vy";
use "QR.vy";
use "Cholesky.vy";
use "Eigen.vy";

ruleset { dynamic_casting };

module vdecomp;

fn :: vdecomp lu_matrix(M :: vdecomp.Matrix) -> vdecomp.LUResult {
    if M.rows != M.cols { throw "vdecomp.lu_matrix: matrix must be square"; }
    return vdecomp.lu(M.data, M.rows);
}

fn :: vdecomp determinant(M :: vdecomp.Matrix) -> Float64 {
    return vdecomp.lu_matrix(M).det;
}

fn :: vdecomp solve(M :: vdecomp.Matrix,
                    b :: vdecomp.Series) -> vdecomp.Series {
    if M.rows != M.cols { throw "vdecomp.solve: matrix must be square"; }
    if b.n != M.rows { throw "vdecomp.solve: right-hand side length mismatch"; }
    res :: vdecomp.LUResult := vdecomp.lu(M.data, M.rows);
    x :: Array<Float64> := vdecomp.solve_lu(res, b.data, M.rows);
    return vdecomp.Series(x, M.rows);
}

fn :: vdecomp inverse(M :: vdecomp.Matrix) -> vdecomp.Matrix {
    if M.rows != M.cols { throw "vdecomp.inverse: matrix must be square"; }
    n :: Int64 := M.rows;
    res :: vdecomp.LUResult := vdecomp.lu(M.data, n);
    inv :: Array<Float64> := vdecomp.zeros_f64(n * n);
    through col :: 0..n-1 -> loop {
        e :: Array<Float64> := vdecomp.zeros_f64(n);
        e[col] = 1.0;
        x :: Array<Float64> := vdecomp.solve_lu(res, e, n);
        through row :: 0..n-1 -> loop {
            inv[row * n + col] = x[row];
        };
    };
    return vdecomp.Matrix(inv, n, n);
}

fn :: vdecomp qr_matrix(M :: vdecomp.Matrix) -> vdecomp.QRResult {
    return vdecomp.qr(M.data, M.rows, M.cols);
}

fn :: vdecomp least_squares_matrix(M :: vdecomp.Matrix,
                                   b :: vdecomp.Series) -> vdecomp.Series {
    return vdecomp.least_squares(M, b);
}

fn :: vdecomp cholesky_matrix(M :: vdecomp.Matrix) -> vdecomp.CholeskyResult {
    if M.rows != M.cols { throw "vdecomp.cholesky_matrix: matrix must be square"; }
    return vdecomp.cholesky(M.data, M.rows);
}

fn :: vdecomp solve_spd(M :: vdecomp.Matrix,
                        b :: vdecomp.Series) -> vdecomp.Series {
    if M.rows != M.cols { throw "vdecomp.solve_spd: matrix must be square"; }
    if b.n != M.rows { throw "vdecomp.solve_spd: right-hand side length mismatch"; }
    res :: vdecomp.CholeskyResult := vdecomp.cholesky(M.data, M.rows);
    x :: Array<Float64> := vdecomp.solve_cholesky(res, b.data, M.rows);
    return vdecomp.Series(x, M.rows);
}

fn :: vdecomp eigen_matrix(M :: vdecomp.Matrix) -> vdecomp.EigenResult {
    if M.rows != M.cols { throw "vdecomp.eigen_matrix: matrix must be square"; }
    return vdecomp.eigen_symmetric(M.data, M.rows);
}

fn :: vdecomp svd_values(M :: vdecomp.Matrix) -> Array<Float64> {
    return vdecomp.singular_values(M);
}

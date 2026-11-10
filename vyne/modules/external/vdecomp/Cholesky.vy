# vdecomp/Cholesky.vy - Cholesky factorization for symmetric positive-
# definite matrices, plus the triangular solves.
#
# `cholesky(A, n)` returns L with A = L*L^T, or `ok = false` when a
# non-positive pivot is encountered. `solve_cholesky` uses L and L^T.

use "Types.vy";
use "Util.vy";

ruleset { dynamic_casting };

module vdecomp;
use native vmath;

fn :: vdecomp cholesky(A :: Array<Float64>,
                       n :: Int64) -> vdecomp.CholeskyResult {
    l :: Array<Float64> := vdecomp.zeros_f64(n * n);
    ok :: Bool := true;

    through i :: 0..n-1 -> loop {
        through j :: 0..i -> loop {
            s :: Float64 := A[i * n + j];
            through k :: 0..j-1 -> loop {
                s = s - l[i * n + k] * l[j * n + k];
            };
            if i == j {
                if s <= 0.0 {
                    ok = false;
                } else {
                    l[i * n + j] = vmath.sqrt(s);
                }
            } else {
                if l[j * n + j] == 0.0 {
                    ok = false;
                } else {
                    l[i * n + j] = s / l[j * n + j];
                }
            }
        };
    };

    return vdecomp.CholeskyResult(l, n, ok);
}

fn :: vdecomp solve_cholesky(res :: vdecomp.CholeskyResult,
                             b :: Array<Float64>,
                             n :: Int64) -> Array<Float64> {
    if !res.ok { throw "vdecomp.solve_cholesky: matrix is not SPD"; }
    y :: Array<Float64> := vdecomp.zeros_f64(n);

    # Forward: L*y = b
    through i :: 0..n-1 -> loop {
        s :: Float64 := b[i];
        through j :: 0..i-1 -> loop {
            s = s - res.l[i * n + j] * y[j];
        };
        y[i] = s / res.l[i * n + i];
    };

    # Back: L^T*x = y
    x :: Array<Float64> := vdecomp.zeros_f64(n);
    idx :: Int64 := n - 1;
    while idx >= 0 {
        s2 :: Float64 := y[idx];
        through j2 :: idx+1..n-1 -> loop {
            s2 = s2 - res.l[j2 * n + idx] * x[j2];
        };
        x[idx] = s2 / res.l[idx * n + idx];
        idx = idx - 1;
    }
    return x;
}

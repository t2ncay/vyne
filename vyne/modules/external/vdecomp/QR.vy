# vdecomp/QR.vy - Householder QR and least squares.
#
# `qr(A, m, n)` returns a full m x m orthogonal Q and an m x n upper
# triangular R with A = Q*R. `least_squares` solves min ||A*x - b|| for
# m >= n by applying Q^T and back-substituting R.

use "Types.vy";
use "Util.vy";

ruleset { dynamic_casting };

module vdecomp;
use native vmath;

fn :: vdecomp qr(A :: Array<Float64>, m :: Int64,
                 n :: Int64) -> vdecomp.QRResult {
    r :: Array<Float64> := vdecomp.zeros_f64(m * n);
    through i :: 0..m*n-1 -> loop { r[i] = A[i]; };
    q :: Array<Float64> := vdecomp.identity_f64(m);

    limit :: Int64 := n;
    if m < limit { limit = m; }

    through k :: 0..limit-1 -> loop {
        norm2 :: Float64 := 0.0;
        through i :: k..m-1 -> loop {
            x :: Float64 := r[i * n + k];
            norm2 = norm2 + x * x;
        };
        alpha :: Float64 := vmath.sqrt(norm2);
        if alpha != 0.0 {
            if r[k * n + k] > 0.0 { alpha = 0.0 - alpha; }

            v :: Array<Float64> := vdecomp.zeros_f64(m);
            through i2 :: k..m-1 -> loop { v[i2] = r[i2 * n + k]; };
            v[k] = v[k] - alpha;

            vtv :: Float64 := 0.0;
            through i3 :: k..m-1 -> loop { vtv = vtv + v[i3] * v[i3]; };
            if vtv != 0.0 {
                beta :: Float64 := 2.0 / vtv;

                # R <- H*R
                through c :: k..n-1 -> loop {
                    dot :: Float64 := 0.0;
                    through i4 :: k..m-1 -> loop {
                        dot = dot + v[i4] * r[i4 * n + c];
                    };
                    factor :: Float64 := beta * dot;
                    through i5 :: k..m-1 -> loop {
                        r[i5 * n + c] = r[i5 * n + c] - factor * v[i5];
                    };
                };

                # Q <- Q*H
                through row :: 0..m-1 -> loop {
                    dotq :: Float64 := 0.0;
                    through j :: k..m-1 -> loop {
                        dotq = dotq + q[row * m + j] * v[j];
                    };
                    factorq :: Float64 := beta * dotq;
                    through j2 :: k..m-1 -> loop {
                        q[row * m + j2] = q[row * m + j2] - factorq * v[j2];
                    };
                };
            }
        }
    };

    return vdecomp.QRResult(q, r, m, n);
}

fn :: vdecomp least_squares(A :: vdecomp.Matrix,
                            b :: vdecomp.Series) -> vdecomp.Series {
    m :: Int64 := A.rows;
    n :: Int64 := A.cols;
    if m < n { throw "vdecomp.least_squares: need rows >= cols"; }
    if b.n != m { throw "vdecomp.least_squares: b length must equal rows"; }

    res :: vdecomp.QRResult := vdecomp.qr(A.data, m, n);

    # c = Q^T b
    c :: Array<Float64> := vdecomp.zeros_f64(m);
    through i :: 0..m-1 -> loop {
        s :: Float64 := 0.0;
        through j :: 0..m-1 -> loop {
            s = s + res.q[j * m + i] * b.data[j];
        };
        c[i] = s;
    };

    # Back solve R x = c using the first n rows.
    x :: Array<Float64> := vdecomp.zeros_f64(n);
    idx :: Int64 := n - 1;
    while idx >= 0 {
        s2 :: Float64 := c[idx];
        through j2 :: idx+1..n-1 -> loop {
            s2 = s2 - res.r[idx * n + j2] * x[j2];
        };
        x[idx] = s2 / res.r[idx * n + idx];
        idx = idx - 1;
    }
    return vdecomp.Series(x, n);
}

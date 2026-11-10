# vdecomp/Eigen.vy - symmetric Jacobi eigenvalues and singular values.
#
# `eigen_symmetric(A, n)` works on a copy of A and returns eigenvalues in
# `values` plus eigenvectors as the columns of the row-major `vectors`
# matrix. `singular_values(M)` forms A^T A, runs the symmetric solver, and
# returns the square roots in descending order. The A^T A route is
# numerically weaker than a one-sided Jacobi SVD for ill-conditioned
# matrices; that is documented, not hidden.

use "Types.vy";
use "Util.vy";

ruleset { dynamic_casting };

module vdecomp;
use native vmath;

fn :: vdecomp eigen_symmetric(A :: Array<Float64>,
                              n :: Int64) -> vdecomp.EigenResult {
    a :: Array<Float64> := vdecomp.zeros_f64(n * n);
    through i :: 0..n*n-1 -> loop { a[i] = A[i]; };
    v :: Array<Float64> := vdecomp.identity_f64(n);

    through sweep :: 0..99 -> loop {
        off :: Float64 := 0.0;
        through p :: 0..n-2 -> loop {
            through q :: p+1..n-1 -> loop {
                apq0 :: Float64 := a[p * n + q];
                off = off + apq0 * apq0;
            };
        };
        if off < 1e-24 { break; }

        through p2 :: 0..n-2 -> loop {
            through q2 :: p2+1..n-1 -> loop {
                apq :: Float64 := a[p2 * n + q2];
                if vdecomp.abs_f64(apq) >= 1e-15 {
                    app :: Float64 := a[p2 * n + p2];
                    aqq :: Float64 := a[q2 * n + q2];
                    theta :: Float64 := (aqq - app) / (2.0 * apq);
                    t :: Float64 := 0.0;
                    at :: Float64 := vdecomp.abs_f64(theta);
                    denom :: Float64 := at + vmath.sqrt(1.0 + theta * theta);
                    if theta >= 0.0 {
                        t = 1.0 / denom;
                    } else {
                        t = 0.0 - 1.0 / denom;
                    }
                    c :: Float64 := 1.0 / vmath.sqrt(1.0 + t * t);
                    s :: Float64 := t * c;

                    a[p2 * n + p2] = c * c * app - 2.0 * s * c * apq + s * s * aqq;
                    a[q2 * n + q2] = s * s * app + 2.0 * s * c * apq + c * c * aqq;
                    a[p2 * n + q2] = 0.0;
                    a[q2 * n + p2] = 0.0;

                    through i2 :: 0..n-1 -> loop {
                        if i2 != p2 {
                            if i2 != q2 {
                                aip := a[i2 * n + p2];
                                aiq := a[i2 * n + q2];
                                nip := c * aip - s * aiq;
                                niq := s * aip + c * aiq;
                                a[i2 * n + p2] = nip;
                                a[i2 * n + q2] = niq;
                                a[p2 * n + i2] = nip;
                                a[q2 * n + i2] = niq;
                            }
                        }
                    };

                    through i3 :: 0..n-1 -> loop {
                        vip := v[i3 * n + p2];
                        viq := v[i3 * n + q2];
                        v[i3 * n + p2] = c * vip - s * viq;
                        v[i3 * n + q2] = s * vip + c * viq;
                    };
                }
            };
        };
    };

    values :: Array<Float64> := vdecomp.zeros_f64(n);
    through i4 :: 0..n-1 -> loop { values[i4] = a[i4 * n + i4]; };
    return vdecomp.EigenResult(values, v, n);
}

fn :: vdecomp singular_values(M :: vdecomp.Matrix) -> Array<Float64> {
    m :: Int64 := M.rows;
    n :: Int64 := M.cols;
    ata :: Array<Float64> := vdecomp.zeros_f64(n * n);
    through i :: 0..n-1 -> loop {
        through j :: 0..n-1 -> loop {
            s :: Float64 := 0.0;
            through k :: 0..m-1 -> loop {
                s = s + M.data[k * n + i] * M.data[k * n + j];
            };
            ata[i * n + j] = s;
        };
    };
    eig :: vdecomp.EigenResult := vdecomp.eigen_symmetric(ata, n);
    sv :: Array<Float64> := vdecomp.zeros_f64(n);
    through i2 :: 0..n-1 -> loop {
        x :: Float64 := eig.values[i2];
        if x < 0.0 { x = 0.0; }
        sv[i2] = vmath.sqrt(x);
    };

    # Descending selection sort; n is small in the intended uses.
    through i3 :: 0..n-2 -> loop {
        best :: Int64 := i3;
        through j3 :: i3+1..n-1 -> loop {
            if sv[j3] > sv[best] { best = j3; }
        };
        if best != i3 {
            tmp :: Float64 := sv[i3];
            sv[i3] = sv[best];
            sv[best] = tmp;
        }
    };
    return sv;
}

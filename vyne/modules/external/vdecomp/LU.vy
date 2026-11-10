# vdecomp/LU.vy - LU factorization with partial pivoting.
#
# `lu(A, n)` copies A, stores the unit-lower factor in the strict lower
# triangle and U on and above the diagonal, and returns the row
# permutation in `piv`. `det` is already folded from the diagonal and the
# pivot parity. `solve_lu` uses the stored factors.

use "Types.vy";
use "Util.vy";

ruleset { dynamic_casting };

module vdecomp;

fn :: vdecomp lu(A :: Array<Float64>, n :: Int64) -> vdecomp.LUResult {
    lu_data :: Array<Float64> := vdecomp.zeros_f64(n * n);
    through i :: 0..n*n-1 -> loop { lu_data[i] = A[i]; };

    piv :: Array<Int64> := vdecomp.zeros_i64(n);
    through i :: 0..n-1 -> loop { piv[i] = i; };

    singular :: Bool := false;
    sign :: Float64 := 1.0;

    through col :: 0..n-1 -> loop {
        pivot :: Int64 := col;
        best :: Float64 := vdecomp.abs_f64(lu_data[col * n + col]);
        through r :: col+1..n-1 -> loop {
            v :: Float64 := vdecomp.abs_f64(lu_data[r * n + col]);
            if v > best {
                best = v;
                pivot = r;
            }
        };
        if best == 0.0 {
            singular = true;
        } else {
            if pivot != col {
                through c :: 0..n-1 -> loop {
                    tmp :: Float64 := lu_data[col * n + c];
                    lu_data[col * n + c] = lu_data[pivot * n + c];
                    lu_data[pivot * n + c] = tmp;
                };
                tmpi :: Int64 := piv[col];
                piv[col] = piv[pivot];
                piv[pivot] = tmpi;
                sign = 0.0 - sign;
            }
            pivval :: Float64 := lu_data[col * n + col];
            through r2 :: col+1..n-1 -> loop {
                factor :: Float64 := lu_data[r2 * n + col] / pivval;
                lu_data[r2 * n + col] = factor;
                through c2 :: col+1..n-1 -> loop {
                    lu_data[r2 * n + c2] =
                        lu_data[r2 * n + c2] - factor * lu_data[col * n + c2];
                };
            };
        }
    };

    det :: Float64 := sign;
    if singular {
        det = 0.0;
    } else {
        through i :: 0..n-1 -> loop { det = det * lu_data[i * n + i]; };
    }

    return vdecomp.LUResult(lu_data, piv, n, det, singular);
}

fn :: vdecomp solve_lu(res :: vdecomp.LUResult,
                       b :: Array<Float64>,
                       n :: Int64) -> Array<Float64> {
    if res.singular {
        throw "vdecomp.solve_lu: matrix is singular";
    }
    y :: Array<Float64> := vdecomp.zeros_f64(n);

    # Forward solve L*y = P*b.
    through i :: 0..n-1 -> loop {
        s :: Float64 := b[res.piv[i]];
        through j :: 0..i-1 -> loop {
            s = s - res.lu[i * n + j] * y[j];
        };
        y[i] = s;
    };

    # Back solve U*x = y.
    x :: Array<Float64> := vdecomp.zeros_f64(n);
    idx :: Int64 := n - 1;
    while idx >= 0 {
        s2 :: Float64 := y[idx];
        through j2 :: idx+1..n-1 -> loop {
            s2 = s2 - res.lu[idx * n + j2] * x[j2];
        };
        x[idx] = s2 / res.lu[idx * n + idx];
        idx = idx - 1;
    }
    return x;
}

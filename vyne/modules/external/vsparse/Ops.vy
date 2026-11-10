# vsparse/Ops.vy - SpMV, residuals, and the conjugate-gradient solver.
#
# Wrappers validate shapes and allocate the output vectors; the k_* leaves
# do the arithmetic. CG reuses one Ap buffer and updates x/r/p in place,
# so peak memory is O(n) independent of iteration count.

use "Types.vy";
use "Kernels.vy";
use "Constructors.vy";

ruleset { dynamic_casting };

module vsparse;
use native vmath;

fn :: vsparse spmv(A :: vsparse.CSR, x :: vsparse.Series) -> vsparse.Series {
    if x.n != A.cols {
        throw "vsparse.spmv: vector length does not match column count";
    }
    y :: Array<Float64> := vsparse.zeros_f64(A.rows);
    vsparse.k_spmv_csr(A.values, A.col_idx, A.row_ptr, x.data, y, A.rows);
    return vsparse.Series(y, A.rows);
}

fn :: vsparse frobenius(A :: vsparse.CSR) -> Float64 {
    return vmath.sqrt(vsparse.k_sum_sq(A.values, A.nnz));
}

fn :: vsparse residual(A :: vsparse.CSR, b :: vsparse.Series,
                       x :: vsparse.Series) -> Float64 {
    if b.n != A.rows || x.n != A.cols {
        throw "vsparse.residual: shape mismatch";
    }
    Ax :: Array<Float64> := vsparse.zeros_f64(A.rows);
    vsparse.k_spmv_csr(A.values, A.col_idx, A.row_ptr, x.data, Ax, A.rows);
    s :: Float64 := 0.0;
    through i :: 0..A.rows-1 -> loop {
        d :: Float64 := Ax[i] - b.data[i];
        s = s + d * d;
    };
    return vmath.sqrt(s);
}

fn :: vsparse dot(x :: vsparse.Series, y :: vsparse.Series) -> Float64 {
    if x.n != y.n { throw "vsparse.dot: length mismatch"; }
    return vsparse.k_dot(x.data, y.data, x.n);
}

# Conjugate gradient for symmetric positive-definite systems. Stops when
# the Euclidean residual is <= tol, or after max_iter iterations.
fn :: vsparse cg(A :: vsparse.CSR, b :: vsparse.Series,
                 tol :: Float64,
                 max_iter :: Int64) -> vsparse.SolverResult {
    if A.rows != A.cols {
        throw "vsparse.cg: matrix must be square";
    }
    if b.n != A.rows {
        throw "vsparse.cg: right-hand side length does not match rows";
    }
    n :: Int64 := A.rows;
    x :: Array<Float64> := vsparse.zeros_f64(n);
    r :: Array<Float64> := vsparse.zeros_f64(n);
    p :: Array<Float64> := vsparse.zeros_f64(n);
    Ap :: Array<Float64> := vsparse.zeros_f64(n);
    through i :: 0..n-1 -> loop {
        r[i] = b.data[i];
        p[i] = b.data[i];
    };

    rsold :: Float64 := vsparse.k_dot(r, r, n);
    res :: Float64 := vmath.sqrt(rsold);
    if res <= tol {
        return vsparse.SolverResult(x, 0, res, true);
    }

    converged :: Bool := false;
    iterations :: Int64 := 0;
    through it :: 0..max_iter-1 -> loop {
        vsparse.k_spmv_csr(A.values, A.col_idx, A.row_ptr, p, Ap, n);
        pAp :: Float64 := vsparse.k_dot(p, Ap, n);
        if pAp == 0.0 {
            iterations = it + 1;
            break;
        }
        alpha :: Float64 := rsold / pAp;
        through i :: 0..n-1 -> loop {
            x[i] = x[i] + alpha * p[i];
            r[i] = r[i] - alpha * Ap[i];
        };
        rsnew :: Float64 := vsparse.k_dot(r, r, n);
        res = vmath.sqrt(rsnew);
        iterations = it + 1;
        if res <= tol {
            converged = true;
            break;
        }
        beta :: Float64 := rsnew / rsold;
        through i :: 0..n-1 -> loop {
            p[i] = r[i] + beta * p[i];
        };
        rsold = rsnew;
    };

    return vsparse.SolverResult(x, iterations, res, converged);
}

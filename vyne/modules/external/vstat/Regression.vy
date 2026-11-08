# vstat/Regression.vy - linear least squares.
#
# Linear regression is the closed-form two-variable case. Multiple linear
# and ridge regression use the normal equations and a small partial-pivot
# Gaussian solve; X is row-major and must already include an intercept
# column if the caller wants one.
#
# The solver is intentionally local to vstat. vdecomp will provide the
# full LU/QR surface; vstat keeps only what its regressions need.

use "Types.vy";
use "Kernels.vy";
use "Reductions.vy";
use "Correlation.vy";

ruleset { dynamic_casting };

module vstat;
use native vmath;

fn :: vstat linear(x :: vstat.Series, y :: vstat.Series) -> vstat.LinearFit {
    if x.n != y.n {
        throw "vstat.linear: series differ in length";
    }
    if x.n < 2 { throw "vstat.linear: need at least 2 points"; }
    mx :: Float64 := vstat.mean(x);
    my :: Float64 := vstat.mean(y);
    sxx :: Float64 := vstat.k_devsq_sum(x.data, mx, x.n);
    sxy :: Float64 := vstat.k_cov_sum(x.data, y.data, mx, my, x.n);
    if sxx == 0.0 {
        throw "vstat.linear: all x values are identical";
    }
    slope :: Float64 := sxy / sxx;
    intercept :: Float64 := my - slope * mx;
    r2 :: Float64 := 0.0;
    if y.n > 1 {
        r :: Float64 := vstat.pearson(x, y);
        r2 = r * r;
    }
    return vstat.LinearFit(slope, intercept, r2);
}

fn :: vstat predict(fit :: vstat.LinearFit, x :: Float64) -> Float64 {
    return fit.slope * x + fit.intercept;
}

fn :: vstat residuals(x :: vstat.Series, y :: vstat.Series,
                      fit :: vstat.LinearFit) -> vstat.Series {
    if x.n != y.n {
        throw "vstat.residuals: series differ in length";
    }
    result :: Array<Float64> := [];
    through i :: 0..x.n-1 -> loop {
        result.push(y.data[i] - (fit.slope * x.data[i] + fit.intercept));
    };
    return vstat.Series(result, x.n);
}

# ---------------------------------------------------------------------------
# Multiple linear / ridge regression
# ---------------------------------------------------------------------------

fn :: vstat _gauss_solve(A :: vstat.Series, b :: vstat.Series,
                         n :: Int64) -> vstat.Series {
    through col :: 0..n-1 -> loop {
        pivot :: Int64 := col;
        best :: Float64 := A.data[col * n + col];
        if best < 0.0 { best = 0.0 - best; }

        if col + 1 < n {
            through r :: col+1..n-1 -> loop {
                v :: Float64 := A.data[r * n + col];
                if v < 0.0 { v = 0.0 - v; }
                if v > best { best = v; pivot = r; }
            };
        }
        if best == 0.0 { throw "vstat: singular normal-equation matrix"; }

        if pivot != col {
            through c :: 0..n-1 -> loop {
                tmp :: Float64 := A.data[col * n + c];
                A.data[col * n + c] = A.data[pivot * n + c];
                A.data[pivot * n + c] = tmp;
            };
            tmpb :: Float64 := b.data[col];
            b.data[col] = b.data[pivot];
            b.data[pivot] = tmpb;
        }

        piv :: Float64 := A.data[col * n + col];
        if col + 1 < n {
            through r2 :: col+1..n-1 -> loop {
                factor :: Float64 := A.data[r2 * n + col] / piv;
                if factor != 0.0 {
                    through c2 :: col..n-1 -> loop {
                        A.data[r2 * n + c2] =
                            A.data[r2 * n + c2] - factor * A.data[col * n + c2];
                    };
                    b.data[r2] = b.data[r2] - factor * b.data[col];
                }
            };
        }
    };

    x :: Array<Float64> := [];
    through i :: 0..n-1 -> loop { x.push(0.0); };

    i2 :: Int64 := n - 1;
    while i2 >= 0 {
        s :: Float64 := b.data[i2];
        if i2 + 1 < n {
            through j :: i2+1..n-1 -> loop {
                s = s - A.data[i2 * n + j] * x[j];
            };
        }
        x[i2] = s / A.data[i2 * n + i2];
        i2 = i2 - 1;
    }
    return vstat.Series(x, n);
}

# Build X^T X and X^T y, add alpha to the diagonal, and solve.
fn :: vstat _normal_equations(X :: Array<Float64>, y :: vstat.Series,
                              rows :: Int64, cols :: Int64,
                              alpha :: Float64) -> vstat.Series {
    if rows <= 0 || cols <= 0 { throw "vstat: empty regression system"; }
    XtX :: Array<Float64> := [];
    through i :: 0..cols*cols-1 -> loop { XtX.push(0.0); };
    Xty :: Array<Float64> := [];
    through i2 :: 0..cols-1 -> loop { Xty.push(0.0); };

    through r :: 0..rows-1 -> loop {
        through j :: 0..cols-1 -> loop {
            xij :: Float64 := X[r * cols + j];
            Xty[j] = Xty[j] + xij * y.data[r];
            through k :: 0..cols-1 -> loop {
                XtX[j * cols + k] = XtX[j * cols + k] + xij * X[r * cols + k];
            };
        }
    };

    if alpha != 0.0 {
        through d :: 0..cols-1 -> loop {
            XtX[d * cols + d] = XtX[d * cols + d] + alpha;
        };
    }
    return vstat._gauss_solve(vstat.Series(XtX, cols * cols),
                              vstat.Series(Xty, cols), cols);
}

# Boxed entry point: a typed X is boxed at the call boundary by the
# compiler, then copied into a typed local here. That keeps the native
# solver `_normal_equations` a leaf (it calls no other array-parameter
# function), which is what the current native-call gate requires.
fn :: vstat multiple(X :: Array, y :: vstat.Series,
                     rows :: Int64, cols :: Int64) -> vstat.Series {
    if rows != y.n {
        throw "vstat.multiple: X rows and y length differ";
    }
    typed :: Array<Float64> := [];
    through i :: 0..rows*cols-1 -> loop { typed.push(X[i]); };
    return vstat._normal_equations(typed, y, rows, cols, 0.0);
}

fn :: vstat ridge(X :: Array, y :: vstat.Series,
                  rows :: Int64, cols :: Int64,
                  alpha :: Float64) -> vstat.Series {
    if rows != y.n {
        throw "vstat.ridge: X rows and y length differ";
    }
    if alpha < 0.0 { throw "vstat.ridge: alpha must be non-negative"; }
    typed :: Array<Float64> := [];
    through i :: 0..rows*cols-1 -> loop { typed.push(X[i]); };
    return vstat._normal_equations(typed, y, rows, cols, alpha);
}

# Predict a row-major design vector with a coefficient Series.
fn :: vstat predict_row(beta :: vstat.Series, row :: vstat.Series) -> Float64 {
    if beta.n != row.n {
        throw "vstat.predict_row: coefficient and row lengths differ";
    }
    return vstat.k_dot(beta.data, row.data, beta.n);
}

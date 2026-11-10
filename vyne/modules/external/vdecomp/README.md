# vdecomp - matrix decompositions and direct solves

`vdecomp` is the dense factorisation layer for Vyne: LU with partial
pivoting, Householder QR and least squares, Cholesky for SPD matrices,
symmetric Jacobi eigen, and singular values via A^T A. It is pure Vyne over
row-major `Array<Float64>` buffers.

```vyne
use external "vdecomp/vdecomp.vy";

A :: vdecomp.Matrix := vdecomp.Matrix(
    [4.0, 1.0,
     1.0, 3.0], 2, 2);
b :: vdecomp.Series := vdecomp.Series([1.0, 2.0], 2);

x :: vdecomp.Series := vdecomp.solve(A, b);
out(x.data[0] + ", " + x.data[1]);
out("det = " + string(vdecomp.determinant(A)));
```

## Layout

| File | Role |
| :--- | :--- |
| `Types.vy` | `Matrix`, `Series`, `LUResult`, `QRResult`, `CholeskyResult`, `EigenResult` |
| `Util.vy` | zeros, identity, scalar abs |
| `LU.vy` | partial-pivot LU, determinant metadata, triangular solves |
| `QR.vy` | Householder QR, least squares |
| `Cholesky.vy` | SPD factorisation and solve |
| `Eigen.vy` | symmetric Jacobi eigen, singular values via A^T A |
| `Ops.vy` | Matrix/Series convenience wrappers (`solve`, `inverse`, `solve_spd`, ...) |
| `vdecomp.vy` | facade |

## Matrices

All raw routines use row-major flat arrays: element `(r,c)` is
`data[r * cols + c]`.

```vyne
# Zero-copy view over a flat buffer:
M :: vdecomp.Matrix := vdecomp.Matrix(data, rows, cols);

# Raw routines:
lu_res :: vdecomp.LUResult := vdecomp.lu(data, n);
x := vdecomp.solve_lu(lu_res, b, n);
```

## LU

| Function | Result |
| :--- | :--- |
| `lu(A, n)` | `LUResult` with factors, pivot order, determinant, singular flag |
| `solve_lu(res, b, n)` | solution vector |
| `lu_matrix(M)` | LU of a `Matrix` |
| `determinant(M)` | determinant from the LU factors |
| `solve(M, b)` | solve A x = b |
| `inverse(M)` | full inverse, one triangular solve per column |

Partial pivoting is used. A zero pivot sets `singular = true`; `solve_lu`
throws instead of returning a meaningless result.

## QR and least squares

| Function | Result |
| :--- | :--- |
| `qr(A, m, n)` | full m x m Q and m x n R |
| `least_squares(A, b)` | least-squares solution for m >= n |
| `qr_matrix(M)` / `least_squares_matrix(M, b)` | Matrix/Series wrappers |

`least_squares` applies Q^T to b and back-substitutes the first n rows of R.

## Cholesky

| Function | Result |
| :--- | :--- |
| `cholesky(A, n)` | lower triangular L with A = L L^T; `ok = false` on a non-positive pivot |
| `solve_cholesky(res, b, n)` | solve using L and L^T |
| `cholesky_matrix(M)` / `solve_spd(M, b)` | wrappers |

Use `solve_spd` for symmetric positive-definite systems; it is roughly half
the work of LU.

## Eigen and singular values

| Function | Result |
| :--- | :--- |
| `eigen_symmetric(A, n)` | eigenvalues and eigenvectors of a symmetric matrix |
| `eigen_matrix(M)` | wrapper |
| `singular_values(M)` | singular values in descending order |
| `svd_values(M)` | alias for `singular_values` |

`eigen_symmetric` uses cyclic Jacobi rotations and returns eigenvectors as
the columns of the row-major `vectors` matrix. `singular_values` forms A^T A
and takes the square roots of its eigenvalues. That route is simple and
adequate for well-conditioned matrices, but it squares the condition number;
a one-sided Jacobi SVD is the planned follow-up for ill-conditioned inputs.

## Validation

`examples/libs/vdecomp_test.vy` checks:

- LU solve against a hand-computed 3x3 system,
- determinant and inverse,
- QR orthogonality and least squares,
- Cholesky solve on an SPD matrix,
- Jacobi eigenpairs on a symmetric 2x2 and 3x3,
- singular values of a small rectangular matrix.

`examples/libs/vdecomp_bench.vy` times 64x64 and 128x128 factorisations.

# vsparse - sparse matrices and iterative solvers

`vsparse` is the sparse linear-algebra layer for Vyne: compressed sparse row
matrices, SpMV, residuals, and conjugate gradient. It operates on typed arrays,
so the kernels lower to native loops with no per-element boxing.

```vyne
use external "vsparse/vsparse.vy";

# 3x3 SPD matrix, row-major dense input.
A :: vsparse.CSR := vsparse.from_dense(
    [4.0, 1.0, 0.0,
     1.0, 3.0, 1.0,
     0.0, 1.0, 2.0], 3, 3);

b :: vsparse.Series := vsparse.Series([1.0, 2.0, 3.0], 3);
r :: vsparse.SolverResult := vsparse.cg(A, b, 1e-10, 100);
out(r.converged);
out(r.residual);
out(r.x[0] + ", " + r.x[1] + ", " + r.x[2]);
```

## Layout

| File | Role |
| :--- | :--- |
| `Types.vy` | `CSR`, `Series`, `SolverResult` |
| `Kernels.vy` | SpMV, dot, axpy, sum-of-squares leaves |
| `Constructors.vy` | zeros, identity, diagonal, from_dense, to_dense, transpose, copy |
| `Ops.vy` | SpMV wrapper, residual, Frobenius norm, conjugate gradient |
| `vsparse.vy` | facade |

## CSR

`values[k]` is the entry at column `col_idx[k]`; row `r` covers the half-open
range `row_ptr[r] .. row_ptr[r+1]`. `row_ptr` has `rows + 1` entries and
`nnz == row_ptr[rows]`.

| Function | Result |
| :--- | :--- |
| `zeros(rows, cols)` | empty CSR |
| `identity(n)` | n x n identity |
| `diagonal(v)` | diagonal matrix from a boxed Array |
| `from_dense(dense, rows, cols)` | CSR with explicit zeros removed |
| `to_dense(A)` | row-major `Array<Float64>` |
| `transpose(A)` | CSR of A^T, O(nnz + cols) |
| `copy(A)` | independent CSR |
| `row_count(A)` / `col_count(A)` / `nonzero_count(A)` | shape introspection |

Direct construction is zero-copy:

```vyne
A :: vsparse.CSR := vsparse.CSR(values, col_idx, row_ptr,
                                rows, cols, values.size());
```

## Operations

| Function | Result |
| :--- | :--- |
| `spmv(A, x)` | fresh `Series` equal to `A*x` |
| `residual(A, b, x)` | Euclidean norm of `A*x - b` |
| `frobenius(A)` | Frobenius norm of the stored values |
| `dot(x, y)` | Euclidean inner product |
| `cg(A, b, tol, max_iter)` | `SolverResult` for symmetric positive-definite A |

CG allocates `x`, `r`, `p`, and one `Ap` buffer, then reuses them for every
iteration. Peak memory is O(rows) independent of `max_iter`.

## Validation

`examples/libs/vsparse_test.vy` checks:

- CSR round-trip through `to_dense`,
- SpMV against a hand-computed product,
- transpose SpMV,
- identity and diagonal construction,
- Frobenius norm,
- CG on a small SPD system to residual <= 1e-8.

`examples/libs/vsparse_bench.vy` times SpMV and CG on a tridiagonal system.

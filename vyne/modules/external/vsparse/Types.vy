# vsparse/Types.vy - compressed sparse row matrix and solver result.
#
# CSR stores one entry per nonzero: `values[k]` at column `col_idx[k]`,
# with row `r` covering the half-open slice
# `row_ptr[r] .. row_ptr[r+1]`. `row_ptr` has `rows + 1` entries and
# `nnz == row_ptr[rows]`.
#
# `Series` is the typed-vector view used by SpMV and the iterative
# solvers. Passing a Series to a wrapper keeps the pointer and length
# together so the k_* kernels see unboxed payloads.

ruleset { dynamic_casting };

module vsparse;

interface CSR {
    values  :: Array<Float64>,
    col_idx :: Array<Int64>,
    row_ptr :: Array<Int64>,
    rows    :: Int64,
    cols    :: Int64,
    nnz     :: Int64,
}

interface Series {
    data :: Array<Float64>,
    n    :: Int64,
}

interface SolverResult {
    x          :: Array<Float64>,
    iterations :: Int64,
    residual   :: Float64,
    converged  :: Bool,
}

# vdecomp/Types.vy - factorization results and flat-matrix views.
#
# All matrices are row-major: element (r, c) is data[r * cols + c].
# `Matrix` and `Series` keep a typed payload and its shape together so
# wrappers can call the native-ABI routines. The raw routines take
# `Array<Float64>` plus explicit dimensions instead.

ruleset { dynamic_casting };

module vdecomp;

interface Matrix {
    data :: Array<Float64>,
    rows :: Int64,
    cols :: Int64,
}

interface Series {
    data :: Array<Float64>,
    n    :: Int64,
}

interface LUResult {
    lu       :: Array<Float64>,
    piv      :: Array<Int64>,
    n        :: Int64,
    det      :: Float64,
    singular :: Bool,
}

interface QRResult {
    q :: Array<Float64>,
    r :: Array<Float64>,
    m :: Int64,
    n :: Int64,
}

interface CholeskyResult {
    l  :: Array<Float64>,
    n  :: Int64,
    ok :: Bool,
}

interface EigenResult {
    values  :: Array<Float64>,
    vectors :: Array<Float64>,
    n       :: Int64,
}

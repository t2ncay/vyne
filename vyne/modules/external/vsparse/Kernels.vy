# vsparse/Kernels.vy - native-ABI leaves over compressed rows.
#
# Every function takes typed arrays plus explicit lengths and is a leaf,
# so the native array ABI lowers the call to raw pointers and the loop to
# a bare C loop.

ruleset { dynamic_casting };

module vsparse;

fn :: vsparse k_spmv_csr(values :: Array<Float64>,
                         col_idx :: Array<Int64>,
                         row_ptr :: Array<Int64>,
                         x :: Array<Float64>,
                         y :: Array<Float64>,
                         rows :: Int64) -> Int64 {
    through r :: 0..rows-1 -> loop {
        s :: Float64 := 0.0;
        start :: Int64 := row_ptr[r];
        stop :: Int64 := row_ptr[r + 1];
        through k :: start..stop-1 -> loop {
            s = s + values[k] * x[col_idx[k]];
        };
        y[r] = s;
    };
    return 0;
}

fn :: vsparse k_dot(a :: Array<Float64>, b :: Array<Float64>,
                    n :: Int64) -> Float64 {
    s :: Float64 := 0.0;
    through i :: 0..n-1 -> loop { s = s + a[i] * b[i]; };
    return s;
}

fn :: vsparse k_axpy(y :: Array<Float64>, alpha :: Float64,
                     x :: Array<Float64>, n :: Int64) -> Int64 {
    through i :: 0..n-1 -> loop { y[i] = y[i] + alpha * x[i]; };
    return 0;
}

fn :: vsparse k_sum_sq(a :: Array<Float64>, n :: Int64) -> Float64 {
    s :: Float64 := 0.0;
    through i :: 0..n-1 -> loop { s = s + a[i] * a[i]; };
    return s;
}

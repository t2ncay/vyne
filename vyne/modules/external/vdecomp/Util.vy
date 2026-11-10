# vdecomp/Util.vy - small allocation and scalar helpers.
#
# Only helpers that take scalars are safe to call from the native-ABI
# routines below; array-parameter helpers would break the leaf rule.

ruleset { dynamic_casting };

module vdecomp;

fn :: vdecomp zeros_f64(n :: Int64) -> Array<Float64> {
    data :: Array<Float64> := [];
    through i :: 0..n-1 -> loop { data.push(0.0); };
    return data;
}

fn :: vdecomp zeros_i64(n :: Int64) -> Array<Int64> {
    data :: Array<Int64> := [];
    through i :: 0..n-1 -> loop { data.push(0); };
    return data;
}

fn :: vdecomp abs_f64(x :: Float64) -> Float64 {
    if x < 0.0 { return 0.0 - x; }
    return x;
}

fn :: vdecomp identity_f64(n :: Int64) -> Array<Float64> {
    data :: Array<Float64> := vdecomp.zeros_f64(n * n);
    through i :: 0..n-1 -> loop { data[i * n + i] = 1.0; };
    return data;
}

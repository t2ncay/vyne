# vcomplex/Kernels.vy - element-wise loops over split (re, im) arrays.
#
# These are leaves: each takes typed arrays and returns Int64. Wrappers in
# Arrays.vy allocate the output and call one kernel.

ruleset { dynamic_casting };

module vcomplex;
use native vmath;

fn :: vcomplex k_add(ore :: Array<Float64>, oim :: Array<Float64>,
                     are :: Array<Float64>, aim :: Array<Float64>,
                     bre :: Array<Float64>, bim :: Array<Float64>,
                     n :: Int64) -> Int64 {
    through i :: 0..n-1 -> loop {
        ore[i] = are[i] + bre[i];
        oim[i] = aim[i] + bim[i];
    };
    return 0;
}

fn :: vcomplex k_sub(ore :: Array<Float64>, oim :: Array<Float64>,
                     are :: Array<Float64>, aim :: Array<Float64>,
                     bre :: Array<Float64>, bim :: Array<Float64>,
                     n :: Int64) -> Int64 {
    through i :: 0..n-1 -> loop {
        ore[i] = are[i] - bre[i];
        oim[i] = aim[i] - bim[i];
    };
    return 0;
}

fn :: vcomplex k_mul(ore :: Array<Float64>, oim :: Array<Float64>,
                     are :: Array<Float64>, aim :: Array<Float64>,
                     bre :: Array<Float64>, bim :: Array<Float64>,
                     n :: Int64) -> Int64 {
    through i :: 0..n-1 -> loop {
        ar :: Float64 := are[i];
        ai :: Float64 := aim[i];
        br :: Float64 := bre[i];
        bi :: Float64 := bim[i];
        ore[i] = ar * br - ai * bi;
        oim[i] = ar * bi + ai * br;
    };
    return 0;
}

fn :: vcomplex k_scale(ore :: Array<Float64>, oim :: Array<Float64>,
                       are :: Array<Float64>, aim :: Array<Float64>,
                       s :: Float64, n :: Int64) -> Int64 {
    through i :: 0..n-1 -> loop {
        ore[i] = are[i] * s;
        oim[i] = aim[i] * s;
    };
    return 0;
}

fn :: vcomplex k_conj(ore :: Array<Float64>, oim :: Array<Float64>,
                      are :: Array<Float64>, aim :: Array<Float64>,
                      n :: Int64) -> Int64 {
    through i :: 0..n-1 -> loop {
        ore[i] = are[i];
        oim[i] = 0.0 - aim[i];
    };
    return 0;
}

fn :: vcomplex k_abs(result :: Array<Float64>,
                     are :: Array<Float64>, aim :: Array<Float64>,
                     n :: Int64) -> Int64 {
    through i :: 0..n-1 -> loop {
        result[i] = vmath.hypot(are[i], aim[i]);
    };
    return 0;
}

fn :: vcomplex k_abs_sq(result :: Array<Float64>,
                        are :: Array<Float64>, aim :: Array<Float64>,
                        n :: Int64) -> Int64 {
    through i :: 0..n-1 -> loop {
        result[i] = are[i] * are[i] + aim[i] * aim[i];
    };
    return 0;
}

fn :: vcomplex k_arg(result :: Array<Float64>,
                     are :: Array<Float64>, aim :: Array<Float64>,
                     n :: Int64) -> Int64 {
    through i :: 0..n-1 -> loop {
        result[i] = vmath.atan2(aim[i], are[i]);
    };
    return 0;
}

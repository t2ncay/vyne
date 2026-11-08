# vcomplex/Arrays.vy - element-wise operations over split complex arrays.
#
# A ComplexArray is (re[], im[], n). Wrappers allocate the output and call
# one element-wise kernel; this is the shape FFT and signal pipelines want.

use "Types.vy";
use "Kernels.vy";
use "Ops.vy";

ruleset { dynamic_casting };

module vcomplex;
use native vmath;

fn :: vcomplex zeros(n :: Int64) -> vcomplex.ComplexArray {
    if n < 0 { throw "vcomplex.zeros: negative length"; }
    re :: Array<Float64> := [];
    im :: Array<Float64> := [];
    through i :: 0..n-1 -> loop {
        re.push(0.0);
        im.push(0.0);
    };
    return vcomplex.ComplexArray(re, im, n);
}

# Boxed convenience entry point. Construct `vcomplex.ComplexArray(re, im, n)`
# directly when you already have typed arrays and want zero-copy.
fn :: vcomplex from_parts(re :: Array, im :: Array,
                          n :: Int64) -> vcomplex.ComplexArray {
    if n < 0 { throw "vcomplex.from_parts: negative length"; }
    red :: Array<Float64> := [];
    imd :: Array<Float64> := [];
    through i :: 0..n-1 -> loop {
        red.push(re[i]);
        imd.push(im[i]);
    };
    return vcomplex.ComplexArray(red, imd, n);
}

fn :: vcomplex copy(a :: vcomplex.ComplexArray) -> vcomplex.ComplexArray {
    result :: vcomplex.ComplexArray := vcomplex.zeros(a.n);
    through i :: 0..a.n-1 -> loop {
        result.re[i] = a.re[i];
        result.im[i] = a.im[i];
    };
    return result;
}

fn :: vcomplex a_add(a :: vcomplex.ComplexArray,
                     b :: vcomplex.ComplexArray) -> vcomplex.ComplexArray {
    if a.n != b.n { throw "vcomplex.a_add: length mismatch"; }
    result :: vcomplex.ComplexArray := vcomplex.zeros(a.n);
    vcomplex.k_add(result.re, result.im, a.re, a.im, b.re, b.im, a.n);
    return result;
}

fn :: vcomplex a_sub(a :: vcomplex.ComplexArray,
                     b :: vcomplex.ComplexArray) -> vcomplex.ComplexArray {
    if a.n != b.n { throw "vcomplex.a_sub: length mismatch"; }
    result :: vcomplex.ComplexArray := vcomplex.zeros(a.n);
    vcomplex.k_sub(result.re, result.im, a.re, a.im, b.re, b.im, a.n);
    return result;
}

fn :: vcomplex a_mul(a :: vcomplex.ComplexArray,
                     b :: vcomplex.ComplexArray) -> vcomplex.ComplexArray {
    if a.n != b.n { throw "vcomplex.a_mul: length mismatch"; }
    result :: vcomplex.ComplexArray := vcomplex.zeros(a.n);
    vcomplex.k_mul(result.re, result.im, a.re, a.im, b.re, b.im, a.n);
    return result;
}

fn :: vcomplex a_scale(a :: vcomplex.ComplexArray,
                       s :: Float64) -> vcomplex.ComplexArray {
    result :: vcomplex.ComplexArray := vcomplex.zeros(a.n);
    vcomplex.k_scale(result.re, result.im, a.re, a.im, s, a.n);
    return result;
}

fn :: vcomplex a_conj(a :: vcomplex.ComplexArray) -> vcomplex.ComplexArray {
    result :: vcomplex.ComplexArray := vcomplex.zeros(a.n);
    vcomplex.k_conj(result.re, result.im, a.re, a.im, a.n);
    return result;
}

fn :: vcomplex a_abs(a :: vcomplex.ComplexArray) -> Array<Float64> {
    result :: Array<Float64> := [];
    through i :: 0..a.n-1 -> loop { result.push(0.0); };
    vcomplex.k_abs(result, a.re, a.im, a.n);
    return result;
}

fn :: vcomplex a_abs_sq(a :: vcomplex.ComplexArray) -> Array<Float64> {
    result :: Array<Float64> := [];
    through i :: 0..a.n-1 -> loop { result.push(0.0); };
    vcomplex.k_abs_sq(result, a.re, a.im, a.n);
    return result;
}

fn :: vcomplex a_arg(a :: vcomplex.ComplexArray) -> Array<Float64> {
    result :: Array<Float64> := [];
    through i :: 0..a.n-1 -> loop { result.push(0.0); };
    vcomplex.k_arg(result, a.re, a.im, a.n);
    return result;
}

fn :: vcomplex a_sum(a :: vcomplex.ComplexArray) -> vcomplex.Complex {
    re :: Float64 := 0.0;
    im :: Float64 := 0.0;
    through i :: 0..a.n-1 -> loop {
        re = re + a.re[i];
        im = im + a.im[i];
    };
    return vcomplex.Complex(re, im);
}

fn :: vcomplex a_dot(a :: vcomplex.ComplexArray,
                     b :: vcomplex.ComplexArray) -> vcomplex.Complex {
    if a.n != b.n { throw "vcomplex.a_dot: length mismatch"; }
    re :: Float64 := 0.0;
    im :: Float64 := 0.0;
    through i :: 0..a.n-1 -> loop {
        re = re + a.re[i] * b.re[i] - a.im[i] * b.im[i];
        im = im + a.re[i] * b.im[i] + a.im[i] * b.re[i];
    };
    return vcomplex.Complex(re, im);
}

fn :: vcomplex a_from_polar(rs :: Array<Float64>,
                            thetas :: Array<Float64>,
                            n :: Int64) -> vcomplex.ComplexArray {
    result :: vcomplex.ComplexArray := vcomplex.zeros(n);
    through i :: 0..n-1 -> loop {
        result.re[i] = rs[i] * vmath.cos(thetas[i]);
        result.im[i] = rs[i] * vmath.sin(thetas[i]);
    };
    return result;
}

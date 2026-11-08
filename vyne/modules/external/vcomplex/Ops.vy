# vcomplex/Ops.vy - scalar complex arithmetic.

use "Types.vy";

ruleset { dynamic_casting };

module vcomplex;
use native vmath;

fn :: vcomplex make(re :: Float64, im :: Float64) -> vcomplex.Complex {
    return vcomplex.Complex(re, im);
}

fn :: vcomplex add(a :: vcomplex.Complex,
                   b :: vcomplex.Complex) -> vcomplex.Complex {
    return vcomplex.Complex(a.re + b.re, a.im + b.im);
}

fn :: vcomplex sub(a :: vcomplex.Complex,
                   b :: vcomplex.Complex) -> vcomplex.Complex {
    return vcomplex.Complex(a.re - b.re, a.im - b.im);
}

fn :: vcomplex mul(a :: vcomplex.Complex,
                   b :: vcomplex.Complex) -> vcomplex.Complex {
    return vcomplex.Complex(
        a.re * b.re - a.im * b.im,
        a.re * b.im + a.im * b.re
    );
}

fn :: vcomplex div(a :: vcomplex.Complex,
                   b :: vcomplex.Complex) -> vcomplex.Complex {
    den :: Float64 := b.re * b.re + b.im * b.im;
    if den == 0.0 { throw "vcomplex.div: division by zero"; }
    return vcomplex.Complex(
        (a.re * b.re + a.im * b.im) / den,
        (a.im * b.re - a.re * b.im) / den
    );
}

fn :: vcomplex scale(a :: vcomplex.Complex, s :: Float64) -> vcomplex.Complex {
    return vcomplex.Complex(a.re * s, a.im * s);
}

fn :: vcomplex neg(a :: vcomplex.Complex) -> vcomplex.Complex {
    return vcomplex.Complex(0.0 - a.re, 0.0 - a.im);
}

fn :: vcomplex conj(a :: vcomplex.Complex) -> vcomplex.Complex {
    return vcomplex.Complex(a.re, 0.0 - a.im);
}

fn :: vcomplex abs(a :: vcomplex.Complex) -> Float64 {
    return vmath.hypot(a.re, a.im);
}

fn :: vcomplex abs_sq(a :: vcomplex.Complex) -> Float64 {
    return a.re * a.re + a.im * a.im;
}

fn :: vcomplex arg(a :: vcomplex.Complex) -> Float64 {
    return vmath.atan2(a.im, a.re);
}

fn :: vcomplex equals(a :: vcomplex.Complex, b :: vcomplex.Complex,
                      eps :: Float64) -> Bool {
    dr :: Float64 := a.re - b.re;
    di :: Float64 := a.im - b.im;
    if dr < 0.0 { dr = 0.0 - dr; }
    if di < 0.0 { di = 0.0 - di; }
    return dr <= eps && di <= eps;
}

fn :: vcomplex to_string(a :: vcomplex.Complex) -> String {
    if a.im >= 0.0 {
        return string(a.re) + "+" + string(a.im) + "i";
    }
    return string(a.re) + string(a.im) + "i";
}

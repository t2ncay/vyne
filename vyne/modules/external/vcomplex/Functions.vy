# vcomplex/Functions.vy - elementary complex functions.
#
# exp/log/sqrt/pow use the polar form; sin/cos use the real/imaginary
# identities. Everything is expressed through vmath's scalar functions.

use "Types.vy";
use "Ops.vy";

ruleset { dynamic_casting };

module vcomplex;
use native vmath;

fn :: vcomplex exp(z :: vcomplex.Complex) -> vcomplex.Complex {
    er :: Float64 := vmath.exp(z.re);
    return vcomplex.Complex(er * vmath.cos(z.im), er * vmath.sin(z.im));
}

fn :: vcomplex log(z :: vcomplex.Complex) -> vcomplex.Complex {
    r :: Float64 := vcomplex.abs(z);
    if r == 0.0 { throw "vcomplex.log: log(0) is undefined"; }
    return vcomplex.Complex(vmath.log(r), vcomplex.arg(z));
}

fn :: vcomplex sqrt(z :: vcomplex.Complex) -> vcomplex.Complex {
    r :: Float64 := vcomplex.abs(z);
    if r == 0.0 { return vcomplex.Complex(0.0, 0.0); }
    sr :: Float64 := vmath.sqrt(r);
    half :: Float64 := vcomplex.arg(z) / 2.0;
    return vcomplex.Complex(sr * vmath.cos(half), sr * vmath.sin(half));
}

fn :: vcomplex pow(z :: vcomplex.Complex,
                   w :: vcomplex.Complex) -> vcomplex.Complex {
    return vcomplex.exp(vcomplex.mul(w, vcomplex.log(z)));
}

fn :: vcomplex sin(z :: vcomplex.Complex) -> vcomplex.Complex {
    return vcomplex.Complex(
        vmath.sin(z.re) * vmath.cosh(z.im),
        vmath.cos(z.re) * vmath.sinh(z.im)
    );
}

fn :: vcomplex cos(z :: vcomplex.Complex) -> vcomplex.Complex {
    return vcomplex.Complex(
        vmath.cos(z.re) * vmath.cosh(z.im),
        0.0 - vmath.sin(z.re) * vmath.sinh(z.im)
    );
}

fn :: vcomplex tan(z :: vcomplex.Complex) -> vcomplex.Complex {
    return vcomplex.div(vcomplex.sin(z), vcomplex.cos(z));
}

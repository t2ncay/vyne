# vcomplex/Polar.vy - polar conversion and rotation.

use "Types.vy";
use "Ops.vy";

ruleset { dynamic_casting };

module vcomplex;
use native vmath;

fn :: vcomplex polar(r :: Float64, theta :: Float64) -> vcomplex.Polar {
    return vcomplex.Polar(r, theta);
}

fn :: vcomplex from_polar(r :: Float64, theta :: Float64) -> vcomplex.Complex {
    return vcomplex.Complex(r * vmath.cos(theta), r * vmath.sin(theta));
}

fn :: vcomplex to_polar(z :: vcomplex.Complex) -> vcomplex.Polar {
    return vcomplex.Polar(vcomplex.abs(z), vcomplex.arg(z));
}

fn :: vcomplex rotate(z :: vcomplex.Complex,
                      theta :: Float64) -> vcomplex.Complex {
    p :: vcomplex.Polar := vcomplex.to_polar(z);
    return vcomplex.from_polar(p.r, p.theta + theta);
}

fn :: vcomplex scale_polar(p :: vcomplex.Polar,
                           factor :: Float64) -> vcomplex.Polar {
    return vcomplex.Polar(p.r * factor, p.theta);
}

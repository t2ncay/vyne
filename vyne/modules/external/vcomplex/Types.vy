# vcomplex/Types.vy - scalar, array, and polar complex types.

ruleset { dynamic_casting };

module vcomplex;

interface Complex {
    re :: Float64,
    im :: Float64,
}

interface ComplexArray {
    re :: Array<Float64>,
    im :: Array<Float64>,
    n  :: Int64,
}

interface Polar {
    r     :: Float64,
    theta :: Float64,
}

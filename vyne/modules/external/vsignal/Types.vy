# vsignal/Types.vy - filter and streaming state.
#
# `Series` pairs a typed sample buffer with its length so wrappers can
# call the k_* kernels with unboxed `double*` parameters.
#
# Streaming state lives in an `Array<Float64>` field. Array element
# writes are visible through every reference to the interface, which is
# what lets a filter step update the caller's state without returning a
# new struct. Scalar fields are configuration and stay immutable during
# a step.

ruleset { dynamic_casting };

module vsignal;

interface Series {
    data :: Array<Float64>,
    n    :: Int64,
}

# Coefficients are b0,b1,b2,a1,a2 (already normalized by a0).
# state = [z1, z2]  (Direct Form II Transposed delay elements)
interface Biquad {
    b0    :: Float64,
    b1    :: Float64,
    b2    :: Float64,
    a1    :: Float64,
    a2    :: Float64,
    state :: Array<Float64>,
}

# state = [y, initialized]
interface Exponential {
    alpha :: Float64,
    state :: Array<Float64>,
}

# buffer is the ring buffer; state = [head, count, sum].
interface MovingAverage {
    buffer :: Array<Float64>,
    window :: Int64,
    state  :: Array<Float64>,
}

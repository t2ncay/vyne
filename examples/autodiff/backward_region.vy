# backward_region.vy — Forms 1 + 2: a `@differentiable` primal and a
# `@backward` region that owns the adjoint pass.
#
# The tape for the reverse sweep is a set of ordinary C locals inside the
# region block. It is reclaimed at the region's closing brace, so the
# `@bounded` budget accounts for it exactly like any other region memory.
# `@recompute` and `@mixed` are parsed and recorded here; their lowering is
# a later phase and the emitted C says so.

use native vmath;

@differentiable
fn loss(w :: Float64, b :: Float64) -> Float64 {
    z :: Float64 := w * 1.0 + b;
    p :: Float64 := 1.0 / (1.0 + vmath.exp(-z));
    return (p - 0.0) * (p - 0.0);
}

w :: Float64 := 0.0;
b :: Float64 := 0.0;

@backward
@bounded<64_000_000>
@recompute<saved_peak: 8_000_000>
@mixed<compute: Float16, accumulate: Float32>
region train_step {
    l :: Float64 := loss(w, b);
    backward(l);

    out("loss   = " + string(l));
    out("dL/dw  = " + string(w.grad));
    out("dL/db  = " + string(b.grad));
    out("expect = 0.25 0.25");
};

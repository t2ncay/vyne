# vcontrol/StateSpace.vy - two-state discrete-time state-space model.
#
#     x[k+1] = A x[k] + B u[k]
#     y[k]   = C x[k] + D u[k]
# with a 2x2 A, 2x1 B, 1x2 C, and scalar D. The state is mutable and
# shared by reference.

use "Types.vy";

ruleset { dynamic_casting };

module vcontrol;

fn :: vcontrol ss2_new(a00 :: Float64, a01 :: Float64,
                       a10 :: Float64, a11 :: Float64,
                       b0 :: Float64, b1 :: Float64,
                       c0 :: Float64, c1 :: Float64,
                       d :: Float64) -> vcontrol.StateSpace2 {
    state :: Array<Float64> := [0.0, 0.0];
    return vcontrol.StateSpace2(a00, a01, a10, a11, b0, b1, c0, c1, d, state);
}

fn :: vcontrol ss2_step(ss ^^ :: vcontrol.StateSpace2,
                        u :: Float64) -> Float64 {
    y :: Float64 := ss.c0 * ss.state[0] + ss.c1 * ss.state[1] + ss.d * u;
    x0 :: Float64 := ss.a00 * ss.state[0] + ss.a01 * ss.state[1] + ss.b0 * u;
    x1 :: Float64 := ss.a10 * ss.state[0] + ss.a11 * ss.state[1] + ss.b1 * u;
    ss.state[0] = x0;
    ss.state[1] = x1;
    return y;
}

fn :: vcontrol ss2_reset(ss ^^ :: vcontrol.StateSpace2,
                         x0 :: Float64, x1 :: Float64) {
    ss.state[0] = x0;
    ss.state[1] = x1;
}

fn :: vcontrol ss2_x0(ss :: vcontrol.StateSpace2) -> Float64 {
    return ss.state[0];
}

fn :: vcontrol ss2_x1(ss :: vcontrol.StateSpace2) -> Float64 {
    return ss.state[1];
}

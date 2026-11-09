# vcontrol/Types.vy - controller and estimator state.
#
# Mutable state lives in an `Array<Float64>` so updates are visible
# through every reference to the controller. Scalar fields are
# configuration (gains, time constants, model matrices) and stay fixed
# for the lifetime of the object.

ruleset { dynamic_casting };

module vcontrol;

# state = [integral, prev_measurement, deriv_filtered, last_output,
#          initialized]
interface Pid {
    kp      :: Float64,
    ki      :: Float64,
    kd      :: Float64,
    dt      :: Float64,
    out_min :: Float64,
    out_max :: Float64,
    tau_d   :: Float64,
    state   :: Array<Float64>,
}

# state = [angle, initialized]
interface ComplementaryFilter {
    alpha :: Float64,
    state :: Array<Float64>,
}

# state = [x, p, initialized]
interface Kalman1D {
    q     :: Float64,
    r     :: Float64,
    state :: Array<Float64>,
}

# Constant-velocity model. state = [x0, x1, p00, p01, p11, initialized]
interface Kalman2 {
    q     :: Float64,
    r     :: Float64,
    state :: Array<Float64>,
}

# Two-state discrete state-space model. state = [x0, x1]
interface StateSpace2 {
    a00   :: Float64,
    a01   :: Float64,
    a10   :: Float64,
    a11   :: Float64,
    b0    :: Float64,
    b1    :: Float64,
    c0    :: Float64,
    c1    :: Float64,
    d     :: Float64,
    state :: Array<Float64>,
}

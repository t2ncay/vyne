# vcontrol/Filters.vy - complementary filter for attitude estimation.
#
# The complementary filter fuses a noisy absolute angle (accelerometer)
# with a smooth rate (gyroscope):
#     angle = alpha * (angle + gyro_rate * dt) + (1 - alpha) * accel_angle
# alpha near 1 trusts the gyro over short horizons and the accelerometer
# over long horizons. For sensor pre-filtering, compose with `vsignal`.

use "Types.vy";

ruleset { dynamic_casting };

module vcontrol;

fn :: vcontrol cf_new(alpha :: Float64) -> vcontrol.ComplementaryFilter {
    if alpha < 0.0 || alpha > 1.0 {
        throw "vcontrol.cf_new: alpha must be in [0, 1]";
    }
    state :: Array<Float64> := [0.0, 0.0];
    return vcontrol.ComplementaryFilter(alpha, state);
}

fn :: vcontrol cf_step(cf ^^ :: vcontrol.ComplementaryFilter,
                       accel_angle :: Float64,
                       gyro_rate :: Float64,
                       dt :: Float64) -> Float64 {
    if dt <= 0.0 { throw "vcontrol.cf_step: dt must be positive"; }
    if cf.state[1] == 0.0 {
        cf.state[0] = accel_angle;
        cf.state[1] = 1.0;
    } else {
        cf.state[0] = cf.alpha * (cf.state[0] + gyro_rate * dt) +
                      (1.0 - cf.alpha) * accel_angle;
    }
    return cf.state[0];
}

fn :: vcontrol cf_reset(cf ^^ :: vcontrol.ComplementaryFilter) {
    cf.state[0] = 0.0;
    cf.state[1] = 0.0;
}

fn :: vcontrol cf_angle(cf :: vcontrol.ComplementaryFilter) -> Float64 {
    return cf.state[0];
}

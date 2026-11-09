# vcontrol/Kalman.vy - scalar and constant-velocity Kalman filters.
#
# Kalman1D estimates a constant scalar from noisy measurements. Kalman2
# estimates position and velocity with a constant-velocity model:
#     F = [[1, dt], [0, 1]], H = [1, 0]
# and discrete white-noise Q:
#     Q = q * [[dt^3/3, dt^2/2], [dt^2/2, dt]]
#
# Both are small enough to keep in scalar state; no matrix library is
# required, which keeps the library portable to the MCU target.

use "Types.vy";

ruleset { dynamic_casting };

module vcontrol;

# ---------------------------------------------------------------------------
# Scalar Kalman
# state = [x, p, initialized]
# ---------------------------------------------------------------------------

fn :: vcontrol kf1_new(q :: Float64, r :: Float64) -> vcontrol.Kalman1D {
    if q < 0.0 { throw "vcontrol.kf1_new: q must be non-negative"; }
    if r <= 0.0 { throw "vcontrol.kf1_new: r must be positive"; }
    state :: Array<Float64> := [0.0, 0.0, 0.0];
    return vcontrol.Kalman1D(q, r, state);
}

fn :: vcontrol kf1_reset(kf ^^ :: vcontrol.Kalman1D, x0 :: Float64) {
    kf.state[0] = x0;
    kf.state[1] = kf.r;
    kf.state[2] = 1.0;
}

fn :: vcontrol kf1_predict(kf ^^ :: vcontrol.Kalman1D) -> Float64 {
    if kf.state[2] != 0.0 {
        kf.state[1] = kf.state[1] + kf.q;
    }
    return kf.state[0];
}

fn :: vcontrol kf1_update(kf ^^ :: vcontrol.Kalman1D,
                          z :: Float64) -> Float64 {
    if kf.state[2] == 0.0 {
        kf.state[0] = z;
        kf.state[1] = kf.r;
        kf.state[2] = 1.0;
        return kf.state[0];
    }
    gain :: Float64 := kf.state[1] / (kf.state[1] + kf.r);
    kf.state[0] = kf.state[0] + gain * (z - kf.state[0]);
    kf.state[1] = (1.0 - gain) * kf.state[1];
    return kf.state[0];
}

fn :: vcontrol kf1_step(kf ^^ :: vcontrol.Kalman1D,
                        z :: Float64) -> Float64 {
    # predict
    if kf.state[2] != 0.0 {
        kf.state[1] = kf.state[1] + kf.q;
    }
    # update
    if kf.state[2] == 0.0 {
        kf.state[0] = z;
        kf.state[1] = kf.r;
        kf.state[2] = 1.0;
        return kf.state[0];
    }
    gain :: Float64 := kf.state[1] / (kf.state[1] + kf.r);
    kf.state[0] = kf.state[0] + gain * (z - kf.state[0]);
    kf.state[1] = (1.0 - gain) * kf.state[1];
    return kf.state[0];
}

fn :: vcontrol kf1_x(kf :: vcontrol.Kalman1D) -> Float64 {
    return kf.state[0];
}

fn :: vcontrol kf1_p(kf :: vcontrol.Kalman1D) -> Float64 {
    return kf.state[1];
}

# ---------------------------------------------------------------------------
# Constant-velocity Kalman
# state = [x0, x1, p00, p01, p11, initialized]
# ---------------------------------------------------------------------------

fn :: vcontrol kf2_new(q :: Float64, r :: Float64) -> vcontrol.Kalman2 {
    if q < 0.0 { throw "vcontrol.kf2_new: q must be non-negative"; }
    if r <= 0.0 { throw "vcontrol.kf2_new: r must be positive"; }
    state :: Array<Float64> := [0.0, 0.0, 0.0, 0.0, 0.0, 0.0];
    return vcontrol.Kalman2(q, r, state);
}

fn :: vcontrol kf2_reset(kf ^^ :: vcontrol.Kalman2,
                         x0 :: Float64, x1 :: Float64) {
    kf.state[0] = x0;
    kf.state[1] = x1;
    kf.state[2] = kf.r;
    kf.state[3] = 0.0;
    kf.state[4] = kf.r;
    kf.state[5] = 1.0;
}

fn :: vcontrol kf2_predict(kf ^^ :: vcontrol.Kalman2,
                           dt :: Float64) -> Int64 {
    if dt <= 0.0 { throw "vcontrol.kf2_predict: dt must be positive"; }
    if kf.state[5] == 0.0 { return 0; }

    dt2 :: Float64 := dt * dt;
    dt3 :: Float64 := dt2 * dt;
    x0 :: Float64 := kf.state[0] + dt * kf.state[1];
    x1 :: Float64 := kf.state[1];
    p00 :: Float64 := kf.state[2] + 2.0 * dt * kf.state[3] +
                      dt2 * kf.state[4] + kf.q * dt3 / 3.0;
    p01 :: Float64 := kf.state[3] + dt * kf.state[4] + kf.q * dt2 / 2.0;
    p11 :: Float64 := kf.state[4] + kf.q * dt;

    kf.state[0] = x0;
    kf.state[1] = x1;
    kf.state[2] = p00;
    kf.state[3] = p01;
    kf.state[4] = p11;
    return 0;
}

fn :: vcontrol kf2_update(kf ^^ :: vcontrol.Kalman2,
                          z :: Float64) -> Float64 {
    if kf.state[5] == 0.0 {
        kf.state[0] = z;
        kf.state[1] = 0.0;
        kf.state[2] = kf.r;
        kf.state[3] = 0.0;
        kf.state[4] = kf.r;
        kf.state[5] = 1.0;
        return kf.state[0];
    }

    s :: Float64 := kf.state[2] + kf.r;
    k0 :: Float64 := kf.state[2] / s;
    k1 :: Float64 := kf.state[3] / s;
    y :: Float64 := z - kf.state[0];
    x0 :: Float64 := kf.state[0] + k0 * y;
    x1 :: Float64 := kf.state[1] + k1 * y;
    p00 :: Float64 := (1.0 - k0) * kf.state[2];
    p01 :: Float64 := (1.0 - k0) * kf.state[3];
    p11 :: Float64 := kf.state[4] - k1 * kf.state[3];

    kf.state[0] = x0;
    kf.state[1] = x1;
    kf.state[2] = p00;
    kf.state[3] = p01;
    kf.state[4] = p11;
    return kf.state[0];
}

fn :: vcontrol kf2_step(kf ^^ :: vcontrol.Kalman2,
                        z :: Float64, dt :: Float64) -> Float64 {
    if dt <= 0.0 { throw "vcontrol.kf2_step: dt must be positive"; }

    if kf.state[5] != 0.0 {
        dt2 :: Float64 := dt * dt;
        dt3 :: Float64 := dt2 * dt;
        x0p :: Float64 := kf.state[0] + dt * kf.state[1];
        p00p :: Float64 := kf.state[2] + 2.0 * dt * kf.state[3] +
                           dt2 * kf.state[4] + kf.q * dt3 / 3.0;
        p01p :: Float64 := kf.state[3] + dt * kf.state[4] + kf.q * dt2 / 2.0;
        p11p :: Float64 := kf.state[4] + kf.q * dt;

        kf.state[0] = x0p;
        kf.state[2] = p00p;
        kf.state[3] = p01p;
        kf.state[4] = p11p;
    }

    if kf.state[5] == 0.0 {
        kf.state[0] = z;
        kf.state[1] = 0.0;
        kf.state[2] = kf.r;
        kf.state[3] = 0.0;
        kf.state[4] = kf.r;
        kf.state[5] = 1.0;
        return kf.state[0];
    }

    s :: Float64 := kf.state[2] + kf.r;
    k0 :: Float64 := kf.state[2] / s;
    k1 :: Float64 := kf.state[3] / s;
    y :: Float64 := z - kf.state[0];
    x0 :: Float64 := kf.state[0] + k0 * y;
    x1 :: Float64 := kf.state[1] + k1 * y;
    p00 :: Float64 := (1.0 - k0) * kf.state[2];
    p01 :: Float64 := (1.0 - k0) * kf.state[3];
    p11 :: Float64 := kf.state[4] - k1 * kf.state[3];

    kf.state[0] = x0;
    kf.state[1] = x1;
    kf.state[2] = p00;
    kf.state[3] = p01;
    kf.state[4] = p11;
    return kf.state[0];
}

fn :: vcontrol kf2_x0(kf :: vcontrol.Kalman2) -> Float64 {
    return kf.state[0];
}

fn :: vcontrol kf2_x1(kf :: vcontrol.Kalman2) -> Float64 {
    return kf.state[1];
}

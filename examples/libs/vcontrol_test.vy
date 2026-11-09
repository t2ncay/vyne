# vcontrol_test.vy - PID, complementary filter, Kalman, state-space.
#
#   ./vynec --compile examples/libs/vcontrol_test.vy
#   ./examples/libs/vcontrol_test

use external "vcontrol/vcontrol.vy";
use native vmath;

failures :: Array<Int64> := [0];

fn check(name :: String, cond :: Bool) {
    if cond { out("PASS " + name); }
    else {
        out("FAIL " + name);
        failures[0] = failures[0] + 1;
    }
}

fn close(a :: Float64, b :: Float64, eps :: Float64) -> Bool {
    d :: Float64 := a - b;
    if d < 0.0 { d = 0.0 - d; }
    return d <= eps;
}

out("=== vcontrol ===");

# ---- PID closed loop on a first-order plant ------------------------
# Plant: tau * dy/dt = u - y, integrated with dt = 0.01, tau = 0.5.
pid :: vcontrol.Pid := vcontrol.pid_new(2.0, 5.0, 0.05, 0.01,
                                        -10.0, 10.0, 0.05);
y :: Float64 := 0.0;
through k :: 0..999 -> loop {
    u :: Float64 := vcontrol.pid_step(pid, 1.0, y);
    y = y + 0.01 * ((u - y) / 0.5);
};
check("pid-converges", close(y, 1.0, 0.02));
check("pid-output-near-setpoint", close(vcontrol.pid_output(pid), 1.0, 0.05));

# ---- Anti-windup: output respects the limit ------------------------
pid_aw :: vcontrol.Pid := vcontrol.pid_new(2.0, 20.0, 0.0, 0.01,
                                           -1.0, 0.5, 0.0);
y_aw :: Float64 := 0.0;
through k :: 0..499 -> loop {
    u_aw :: Float64 := vcontrol.pid_step(pid_aw, 10.0, y_aw);
    y_aw = y_aw + 0.01 * ((u_aw - y_aw) / 1.0);
};
check("anti-windup-limit", vcontrol.pid_output(pid_aw) <= 0.5 + 1e-12);
check("anti-windup-integral-bounded", vcontrol.pid_integral(pid_aw) < 10.0);

# ---- Bumpless transfer ---------------------------------------------
pid_bl :: vcontrol.Pid := vcontrol.pid_new(2.0, 5.0, 0.05, 0.01,
                                           -10.0, 10.0, 0.0);
vcontrol.pid_bumpless(pid_bl, 1.0, 0.0, 0.3);
u_bl :: Float64 := vcontrol.pid_step(pid_bl, 1.0, 0.0);
check("bumpless-output", close(u_bl, 0.3, 0.06));

# ---- Complementary filter ------------------------------------------
cf :: vcontrol.ComplementaryFilter := vcontrol.cf_new(0.98);
angle :: Float64 := 0.0;
through k :: 0..999 -> loop {
    angle = vcontrol.cf_step(cf, 10.0, 0.0, 0.01);
};
check("cf-converges", close(angle, 10.0, 0.01));

# Gyro integration: with a perfect accelerometer, the angle tracks.
cf2 :: vcontrol.ComplementaryFilter := vcontrol.cf_new(0.99);
a2 :: Float64 := 0.0;
expected :: Float64 := 0.0;
through k :: 0..499 -> loop {
    a2 = vcontrol.cf_step(cf2, expected, 1.0, 0.01);
    expected = expected + 1.0 * 0.01;
};
check("cf-gyro-track", close(a2, expected, 0.05));

# ---- Kalman1D -------------------------------------------------------
kf :: vcontrol.Kalman1D := vcontrol.kf1_new(0.01, 1.0);
est :: Float64 := 0.0;
through k :: 0..499 -> loop {
    noise :: Float64 := (float64(k % 5) - 2.0) * 0.1;
    est = vcontrol.kf1_step(kf, 5.0 + noise);
};
check("kf1-tracks", close(est, 5.0, 0.1));

# ---- Kalman2 (constant velocity) -----------------------------------
kf2 :: vcontrol.Kalman2 := vcontrol.kf2_new(0.1, 1.0);
dt :: Float64 := 0.01;
true_x :: Float64 := 0.0;
through k :: 0..999 -> loop {
    true_x = 2.0 * float64(k) * dt;
    noise2 :: Float64 := (float64(k % 7) - 3.0) * 0.05;
    vcontrol.kf2_step(kf2, true_x + noise2, dt);
};
check("kf2-position", close(vcontrol.kf2_x0(kf2), true_x, 0.2));
check("kf2-velocity", close(vcontrol.kf2_x1(kf2), 2.0, 0.2));

# ---- StateSpace2 ----------------------------------------------------
ss :: vcontrol.StateSpace2 := vcontrol.ss2_new(1.0, 0.01, 0.0, 1.0,
                                               0.0, 0.01, 1.0, 0.0, 0.0);
through k :: 0..99 -> loop {
    ss2_y :: Float64 := vcontrol.ss2_step(ss, 1.0);
};
check("ss2-velocity", close(vcontrol.ss2_x1(ss), 1.0, 1e-9));
check("ss2-position", close(vcontrol.ss2_x0(ss), 0.495, 1e-9));

if failures[0] > 0 {
    out("vcontrol: " + string(failures[0]) + " check(s) failed");
    exit(1);
}
out("vcontrol: all checks passed");

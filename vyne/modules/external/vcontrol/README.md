# vcontrol — control systems for Vyne

`vcontrol` is the control-and-estimation layer for Vyne: PID, a complementary
filter, scalar and constant-velocity Kalman filters, and a two-state
state-space model. It is pure Vyne — no matrix library is required — and
follows the same interface/state style as the other external libraries.

> **Portability:** the public surface uses interface types, so it runs on the
> desktop runtime today. AVR support follows once the MCU runtime implements
> boxed structs; the controller arithmetic itself is plain scalar code.

```vyne
use external "vcontrol/vcontrol.vy";

pid :: vcontrol.Pid := vcontrol.pid_new(2.0, 5.0, 0.05, 0.01,
                                        -10.0, 10.0, 0.05);
y :: Float64 := 0.0;
through k :: 0..999 -> loop {
    u :: Float64 := vcontrol.pid_step(pid, 1.0, y);
    y = y + 0.01 * ((u - y) / 0.5);   # first-order plant
};
```

## Layout

| File | Role |
| :--- | :--- |
| `Types.vy` | `Pid`, `ComplementaryFilter`, `Kalman1D`, `Kalman2`, `StateSpace2` |
| `Pid.vy` | PID with anti-windup, derivative filtering, bumpless transfer |
| `Filters.vy` | complementary filter |
| `Kalman.vy` | scalar and constant-velocity Kalman filters |
| `StateSpace.vy` | two-state discrete state-space model |
| `vcontrol.vy` | facade |

## State model

Systems that evolve between calls keep their state in an `Array<Float64>` field
so updates are visible through every reference. Scalar fields are
configuration (gains, time constants, matrices) and stay fixed.

| Type | State layout |
| :--- | :--- |
| `Pid` | `[integral, prev_measurement, deriv_filtered, last_output, initialized]` |
| `ComplementaryFilter` | `[angle, initialized]` |
| `Kalman1D` | `[x, p, initialized]` |
| `Kalman2` | `[x0, x1, p00, p01, p11, initialized]` |
| `StateSpace2` | `[x0, x1]` |

## PID

| Function | Result |
| :--- | :--- |
| `pid_new(kp, ki, kd, dt, out_min, out_max, tau_d)` | construct |
| `pid_step(pid, setpoint, measurement)` | one control step, returns the clamped command |
| `pid_bumpless(pid, setpoint, measurement, manual_output)` | transfer from manual to automatic |
| `pid_reset(pid)` | clear state |
| `pid_output(pid)` / `pid_integral(pid)` | introspection |

The derivative is on the **measurement**, not the error, so a setpoint change
does not produce a derivative kick. `tau_d` is the derivative low-pass time
constant in seconds (`0` disables it). Anti-windup is conditional integration:
the integral is held when the unclamped command is saturated and the error
would push it further into the limit.

The first automatic step after `pid_bumpless` still integrates for one `dt`,
so it can differ from `manual_output` by `ki * error * dt`. Pass `ki = 0` if you
need an exact first-step match.

## Complementary filter

| Function | Result |
| :--- | :--- |
| `cf_new(alpha)` | construct; `alpha` in `[0,1]` |
| `cf_step(cf, accel_angle, gyro_rate, dt)` | fused angle |
| `cf_reset(cf)` / `cf_angle(cf)` | reset / introspect |

`angle = alpha * (angle + gyro_rate * dt) + (1 - alpha) * accel_angle`.
`alpha` near 1 trusts the gyro over short horizons and the accelerometer over
long horizons.

## Kalman filters

| Function | Result |
| :--- | :--- |
| `kf1_new(q, r)` | scalar Kalman |
| `kf1_predict(kf)` / `kf1_update(kf, z)` | separate phases |
| `kf1_step(kf, z)` | predict + update |
| `kf1_reset(kf, x0)` | reset |
| `kf1_x(kf)` / `kf1_p(kf)` | state / covariance |
| `kf2_new(q, r)` | constant-velocity Kalman |
| `kf2_predict(kf, dt)` / `kf2_update(kf, z)` | separate phases |
| `kf2_step(kf, z, dt)` | predict + update |
| `kf2_reset(kf, x0, x1)` | reset |
| `kf2_x0(kf)` / `kf2_x1(kf)` | position / velocity |

`Kalman2` uses the constant-velocity model `F = [[1, dt], [0, 1]]`,
`H = [1, 0]`, and discrete white-noise
`Q = q * [[dt³/3, dt²/2], [dt²/2, dt]]`.

## State space

| Function | Result |
| :--- | :--- |
| `ss2_new(a00,a01,a10,a11,b0,b1,c0,c1,d)` | construct |
| `ss2_step(ss, u)` | `y = C x + D u`, then `x = A x + B u` |
| `ss2_reset(ss, x0, x1)` | set state |
| `ss2_x0(ss)` / `ss2_x1(ss)` | state |

## Composing with vsignal

`vcontrol` is intentionally independent of `vsignal` so it stays small and
AVR-friendly. Filter sensor readings with `vsignal` before feeding the
controller:

```vyne
use external "vsignal/vsignal.vy";
use external "vcontrol/vcontrol.vy";

raw :: Array<Float64> := [/* noisy samples */];
s   :: vsignal.Series := vsignal.Series(raw, raw.size());
smoothed :: Array<Float64> := vsignal.moving_average(s, 8);
u :: Float64 := vcontrol.pid_step(pid, 1.0, smoothed[smoothed.size()-1]);
```

## Verification

```bash
vynec --compile examples/libs/vcontrol_test.vy
./examples/libs/vcontrol_test

vynec --compile examples/libs/vcontrol_bench.vy
./examples/libs/vcontrol_bench
```

The test covers a closed-loop first-order plant, anti-windup saturation,
bumpless transfer, complementary-filter convergence and gyro tracking, scalar
and constant-velocity Kalman tracking, and a state-space hand value.

# vcontrol/Pid.vy - PID controller with anti-windup, derivative
# filtering, and bumpless transfer.
#
# The derivative is taken on the measurement, not the error, so changing
# the setpoint does not produce a derivative kick. `tau_d` is the
# derivative low-pass time constant in seconds; 0 disables filtering.
#
# Anti-windup is conditional integration: the integral is held when the
# unclamped command is already saturated and the error would push it
# further into the limit.

use "Types.vy";

ruleset { dynamic_casting };

module vcontrol;

fn :: vcontrol pid_new(kp :: Float64, ki :: Float64, kd :: Float64,
                       dt :: Float64, out_min :: Float64,
                       out_max :: Float64, tau_d :: Float64) -> vcontrol.Pid {
    if dt <= 0.0 { throw "vcontrol.pid_new: dt must be positive"; }
    if out_min >= out_max {
        throw "vcontrol.pid_new: out_min must be less than out_max";
    }
    if tau_d < 0.0 { throw "vcontrol.pid_new: tau_d must be non-negative"; }
    state :: Array<Float64> := [0.0, 0.0, 0.0, 0.0, 0.0];
    return vcontrol.Pid(kp, ki, kd, dt, out_min, out_max, tau_d, state);
}

fn :: vcontrol pid_step(pid ^^ :: vcontrol.Pid,
                        setpoint :: Float64,
                        measurement :: Float64) -> Float64 {
    if pid.state[4] == 0.0 {
        pid.state[1] = measurement;
        pid.state[2] = 0.0;
        pid.state[4] = 1.0;
    }

    err :: Float64 := setpoint - measurement;
    dmeas :: Float64 := (measurement - pid.state[1]) / pid.dt;

    if pid.tau_d > 0.0 {
        a :: Float64 := pid.tau_d / (pid.tau_d + pid.dt);
        pid.state[2] = a * pid.state[2] + (1.0 - a) * dmeas;
    } else {
        pid.state[2] = dmeas;
    }

    p_term :: Float64 := pid.kp * err;
    d_term :: Float64 := 0.0 - pid.kd * pid.state[2];
    tentative_i :: Float64 := pid.state[0] + pid.ki * err * pid.dt;
    raw :: Float64 := p_term + tentative_i + d_term;

    # Conditional integration: hold the integral while saturated and
    # pushing further into the limit.
    if (raw > pid.out_max && err > 0.0) ||
       (raw < pid.out_min && err < 0.0) {
        # keep pid.state[0]
    } else {
        pid.state[0] = tentative_i;
    }

    cmd :: Float64 := p_term + pid.state[0] + d_term;
    if cmd > pid.out_max { cmd = pid.out_max; }
    if cmd < pid.out_min { cmd = pid.out_min; }
    pid.state[3] = cmd;
    pid.state[1] = measurement;
    return cmd;
}

# Move from manual to automatic without a step in the command. The next
# pid_step starts from `manual_output` under the current error.
fn :: vcontrol pid_bumpless(pid ^^ :: vcontrol.Pid,
                            setpoint :: Float64,
                            measurement :: Float64,
                            manual_output :: Float64) {
    err :: Float64 := setpoint - measurement;
    pid.state[0] = manual_output - pid.kp * err;
    pid.state[1] = measurement;
    pid.state[2] = 0.0;
    pid.state[3] = manual_output;
    pid.state[4] = 1.0;
}

fn :: vcontrol pid_reset(pid ^^ :: vcontrol.Pid) {
    pid.state[0] = 0.0;
    pid.state[1] = 0.0;
    pid.state[2] = 0.0;
    pid.state[3] = 0.0;
    pid.state[4] = 0.0;
}

fn :: vcontrol pid_output(pid :: vcontrol.Pid) -> Float64 {
    return pid.state[3];
}

fn :: vcontrol pid_integral(pid :: vcontrol.Pid) -> Float64 {
    return pid.state[0];
}

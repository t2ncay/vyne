# voptim/Schedules.vy - learning-rate schedules.
#
# Every function is pure: pass the step index and the schedule parameters,
# get the learning rate. `step` is zero-based.

ruleset { dynamic_casting };

module voptim;
use native vmath;

fn :: voptim constant(step :: Int64, lr :: Float64) -> Float64 {
    return lr;
}

fn :: voptim step_decay(step :: Int64, lr :: Float64,
                        drop_every :: Int64, gamma :: Float64) -> Float64 {
    if drop_every <= 0 { throw "voptim.step_decay: drop_every must be positive"; }
    drops :: Int64 := step / drop_every;
    return lr * vmath.pow(gamma, float64(drops));
}

fn :: voptim exponential(step :: Int64, lr :: Float64,
                         gamma :: Float64) -> Float64 {
    return lr * vmath.pow(gamma, float64(step));
}

fn :: voptim cosine(step :: Int64, lr :: Float64, total :: Int64) -> Float64 {
    if total <= 1 { return lr; }
    s :: Int64 := step;
    if s > total { s = total; }
    phase :: Float64 := 3.141592653589793 * float64(s) / float64(total);
    return lr * 0.5 * (1.0 + vmath.cos(phase));
}

# Linear warmup to `lr` over `warmup` steps, then cosine decay to zero
# over the remaining `total - warmup` steps.
fn :: voptim warmup_cosine(step :: Int64, lr :: Float64,
                           warmup :: Int64, total :: Int64) -> Float64 {
    if warmup < 0 { throw "voptim.warmup_cosine: warmup must be non-negative"; }
    if total <= 0 { throw "voptim.warmup_cosine: total must be positive"; }
    if warmup == 0 { return voptim.cosine(step, lr, total); }
    if step < warmup {
        return lr * float64(step) / float64(warmup);
    }
    if step >= total { return 0.0; }
    remaining :: Int64 := total - warmup;
    local :: Int64 := step - warmup;
    if remaining <= 1 { return 0.0; }
    phase :: Float64 := 3.141592653589793 * float64(local) /
                        float64(remaining);
    return lr * 0.5 * (1.0 + vmath.cos(phase));
}

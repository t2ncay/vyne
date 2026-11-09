# vsignal/Fir.vy - FIR, moving average, exponential, and median filters.
#
# Offline (whole-buffer) forms over a Series. For a real-time loop use
# the streaming states in Types.vy: MovingAverage, Exponential, and the
# biquad path in Iir.vy.

use "Types.vy";
use "Kernels.vy";

ruleset { dynamic_casting };

module vsignal;

fn :: vsignal _alloc(n :: Int64) -> Array<Float64> {
    dst :: Array<Float64> := [];
    through i :: 0..n-1 -> loop { dst.push(0.0); };
    return dst;
}

fn :: vsignal moving_average(s :: vsignal.Series,
                             window :: Int64) -> Array<Float64> {
    if s.n <= 0 { throw "vsignal.moving_average: empty series"; }
    if window <= 0 { throw "vsignal.moving_average: window must be positive"; }
    dst :: Array<Float64> := vsignal._alloc(s.n);
    vsignal.k_move_avg(s.data, s.n, window, dst);
    return dst;
}

fn :: vsignal exponential(s :: vsignal.Series,
                          alpha :: Float64) -> Array<Float64> {
    if s.n <= 0 { throw "vsignal.exponential: empty series"; }
    dst :: Array<Float64> := vsignal._alloc(s.n);
    vsignal.k_exp(s.data, s.n, alpha, dst);
    return dst;
}

fn :: vsignal fir(s :: vsignal.Series,
                  coeffs :: Array<Float64>) -> Array<Float64> {
    if s.n <= 0 { throw "vsignal.fir: empty series"; }
    m :: Int64 := coeffs.size();
    if m <= 0 { throw "vsignal.fir: empty coefficient list"; }
    dst :: Array<Float64> := vsignal._alloc(s.n);
    vsignal.k_fir(s.data, s.n, coeffs, m, dst);
    return dst;
}

# Causal median filter with edge clamping. O(n * w^2); intended for
# small windows.
fn :: vsignal median(s :: vsignal.Series, window :: Int64) -> Array<Float64> {
    if s.n <= 0 { throw "vsignal.median: empty series"; }
    if window <= 0 { throw "vsignal.median: window must be positive"; }
    dst :: Array<Float64> := vsignal._alloc(s.n);
    vsignal.k_median(s.data, s.n, window, dst);
    return dst;
}

# Streaming first-order smoother.
fn :: vsignal exp_new(alpha :: Float64) -> vsignal.Exponential {
    a :: Float64 := alpha;
    if a < 0.0 { a = 0.0; }
    if a > 1.0 { a = 1.0; }
    state :: Array<Float64> := [0.0, 0.0];
    return vsignal.Exponential(a, state);
}

fn :: vsignal exp_step(e ^^ :: vsignal.Exponential, x :: Float64) -> Float64 {
    if e.state[1] == 0.0 {
        e.state[0] = x;
        e.state[1] = 1.0;
    } else {
        e.state[0] = e.alpha * x + (1.0 - e.alpha) * e.state[0];
    }
    return e.state[0];
}

# Streaming moving average. The ring buffer is allocated once and mutated
# in place; `count` saturates at `window` so startup ramps cleanly.
fn :: vsignal ma_new(window :: Int64) -> vsignal.MovingAverage {
    if window <= 0 { throw "vsignal.ma_new: window must be positive"; }
    buf :: Array<Float64> := [];
    through i :: 0..window-1 -> loop { buf.push(0.0); };
    state :: Array<Float64> := [0.0, 0.0, 0.0];
    return vsignal.MovingAverage(buf, window, state);
}

fn :: vsignal ma_step(m ^^ :: vsignal.MovingAverage, x :: Float64) -> Float64 {
    head :: Int64 := int64(m.state[0]);
    old :: Float64 := m.buffer[head];
    m.buffer[head] = x;
    m.state[2] = m.state[2] - old + x;
    m.state[0] = float64(head + 1);
    if m.state[0] >= float64(m.window) { m.state[0] = 0.0; }
    if m.state[1] < float64(m.window) { m.state[1] = m.state[1] + 1.0; }
    return m.state[2] / m.state[1];
}

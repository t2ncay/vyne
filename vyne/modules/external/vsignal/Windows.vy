# vsignal/Windows.vy - spectral window functions.
#
# Windows are returned as a fresh Array<Float64> of length n. They are
# used before an FFT to reduce spectral leakage.

use native vmath;

ruleset { dynamic_casting };

module vsignal;

fn :: vsignal rectangular(n :: Int64) -> Array<Float64> {
    if n <= 0 { throw "vsignal.rectangular: n must be positive"; }
    w :: Array<Float64> := [];
    through i :: 0..n-1 -> loop { w.push(1.0); };
    return w;
}

fn :: vsignal hann(n :: Int64) -> Array<Float64> {
    if n <= 0 { throw "vsignal.hann: n must be positive"; }
    w :: Array<Float64> := [];
    if n == 1 { w.push(1.0); return w; }
    two_pi :: Float64 := 2.0 * vmath.pi;
    through i :: 0..n-1 -> loop {
        w.push(0.5 - 0.5 * vmath.cos(two_pi * float64(i) / float64(n - 1)));
    };
    return w;
}

fn :: vsignal hamming(n :: Int64) -> Array<Float64> {
    if n <= 0 { throw "vsignal.hamming: n must be positive"; }
    w :: Array<Float64> := [];
    if n == 1 { w.push(1.0); return w; }
    two_pi :: Float64 := 2.0 * vmath.pi;
    through i :: 0..n-1 -> loop {
        w.push(0.54 - 0.46 * vmath.cos(two_pi * float64(i) / float64(n - 1)));
    };
    return w;
}

fn :: vsignal blackman(n :: Int64) -> Array<Float64> {
    if n <= 0 { throw "vsignal.blackman: n must be positive"; }
    w :: Array<Float64> := [];
    if n == 1 { w.push(1.0); return w; }
    two_pi :: Float64 := 2.0 * vmath.pi;
    through i :: 0..n-1 -> loop {
        t :: Float64 := two_pi * float64(i) / float64(n - 1);
        w.push(0.42 - 0.5 * vmath.cos(t) + 0.08 * vmath.cos(2.0 * t));
    };
    return w;
}

fn :: vsignal apply_window(s :: vsignal.Series,
                           w :: Array<Float64>) -> Array<Float64> {
    if s.n <= 0 { throw "vsignal.apply_window: empty series"; }
    m :: Int64 := w.size();
    if m != s.n { throw "vsignal.apply_window: window length mismatch"; }
    dst :: Array<Float64> := [];
    through i :: 0..s.n-1 -> loop {
        dst.push(s.data[i] * w[i]);
    };
    return dst;
}

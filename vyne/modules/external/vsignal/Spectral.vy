# vsignal/Spectral.vy - magnitude spectrum helpers.
#
# Desktop-oriented: this file depends on `vfft`, which uses the desktop
# `vmath` native module. The main `vsignal.vy` facade does not import it
# so the AVR-safe filters stay importable from a sketch. On desktop use
# `use external "vsignal/Spectral.vy";` (or the facade, which does include
# it) to opt in.
#
# Inputs are zero-padded to the next power of two before the transform.

use "Types.vy";
use external "vfft/vfft.vy";
use native vmath;

ruleset { dynamic_casting };

module vsignal;

# Magnitude of the positive-frequency bins: N/2 + 1 values for an
# N-point transform.
fn :: vsignal magnitude(xs :: Array, n :: Int64) -> Array<Float64> {
    if n <= 0 { throw "vsignal.magnitude: n must be positive"; }
    big_n :: Int64 := vfft.next_pow2(n);
    re :: Array<Float64> := [];
    im :: Array<Float64> := [];
    through i :: 0..big_n-1 -> loop {
        if i < n { re.push(float64(xs[i])); } else { re.push(0.0); }
        im.push(0.0);
    };
    vfft.forward(re, im);
    dst :: Array<Float64> := [];
    half :: Int64 := big_n / 2 + 1;
    through i :: 0..half-1 -> loop {
        dst.push(vmath.hypot(re[i], im[i]));
    };
    return dst;
}

fn :: vsignal magnitude_series(s :: vsignal.Series) -> Array<Float64> {
    return vsignal.magnitude(s.data, s.n);
}

fn :: vsignal power(xs :: Array, n :: Int64) -> Array<Float64> {
    mags :: Array<Float64> := vsignal.magnitude(xs, n);
    dst :: Array<Float64> := [];
    through i :: 0..mags.size()-1 -> loop {
        dst.push(mags[i] * mags[i]);
    };
    return dst;
}

# Frequency of the largest bin, in Hz. Not interpolated; resolution is
# fs / next_pow2(n).
fn :: vsignal dominant_frequency(xs :: Array, n :: Int64,
                                 fs :: Float64) -> Float64 {
    if fs <= 0.0 {
        throw "vsignal.dominant_frequency: fs must be positive";
    }
    mags :: Array<Float64> := vsignal.magnitude(xs, n);
    if mags.size() == 0 { return 0.0; }
    big_n :: Int64 := vfft.next_pow2(n);
    best :: Int64 := 0;
    best_v :: Float64 := mags[0];
    through i :: 1..mags.size()-1 -> loop {
        if mags[i] > best_v {
            best_v = mags[i];
            best = i;
        }
    };
    return float64(best) * fs / float64(big_n);
}

# vsignal/Iir.vy - biquad (IIR) filters, streaming and offline.
#
# The RBJ audio-EQ cookbook form is used for the designers: a0 is
# normalized out, so `a1`/`a2` are the feedback coefficients in
#     y[n] = b0 x[n] + b1 x[n-1] + b2 x[n-2] - a1 y[n-1] - a2 y[n-2].
# The state update is Direct Form II Transposed, which is the numerically
# better form for floating point and matches k_biquad.

use "Types.vy";
use "Kernels.vy";
use native vmath;

ruleset { dynamic_casting };

module vsignal;

fn :: vsignal _from_coeffs(b0 :: Float64, b1 :: Float64, b2 :: Float64,
                           a1 :: Float64, a2 :: Float64) -> vsignal.Biquad {
    state :: Array<Float64> := [0.0, 0.0];
    return vsignal.Biquad(b0, b1, b2, a1, a2, state);
}

# Build a biquad from explicit coefficients (already normalized by a0).
fn :: vsignal biquad_new(b0 :: Float64, b1 :: Float64, b2 :: Float64,
                         a1 :: Float64, a2 :: Float64) -> vsignal.Biquad {
    return vsignal._from_coeffs(b0, b1, b2, a1, a2);
}

# One streaming sample. Mutates z1/z2 and returns the output.
fn :: vsignal biquad_step(f ^^ :: vsignal.Biquad, x :: Float64) -> Float64 {
    y :: Float64 := f.b0 * x + f.state[0];
    f.state[0] = f.b1 * x - f.a1 * y + f.state[1];
    f.state[1] = f.b2 * x - f.a2 * y;
    return y;
}

fn :: vsignal biquad_reset(f ^^ :: vsignal.Biquad) {
    f.state[0] = 0.0;
    f.state[1] = 0.0;
}

# Offline application over a Series. The filter state is local to the
# call; use biquad_step for a continuous stream.
fn :: vsignal biquad_apply(s :: vsignal.Series,
                           f :: vsignal.Biquad) -> Array<Float64> {
    dst :: Array<Float64> := [];
    through i :: 0..s.n-1 -> loop { dst.push(0.0); };
    vsignal.k_biquad(s.data, s.n, f.b0, f.b1, f.b2, f.a1, f.a2, dst);
    return dst;
}

# Magnitude response |H(e^{jw})| at `freq` Hz for sample rate `fs`.
fn :: vsignal biquad_response(f :: vsignal.Biquad, freq :: Float64,
                              fs :: Float64) -> Float64 {
    if fs <= 0.0 { throw "vsignal.biquad_response: fs must be positive"; }
    w :: Float64 := 2.0 * vmath.pi * freq / fs;
    cw :: Float64 := vmath.cos(w);
    sw :: Float64 := vmath.sin(w);
    c2 :: Float64 := vmath.cos(2.0 * w);
    s2 :: Float64 := vmath.sin(2.0 * w);

    nr :: Float64 := f.b0 + f.b1 * cw + f.b2 * c2;
    ni :: Float64 := 0.0 - (f.b1 * sw + f.b2 * s2);
    dr :: Float64 := 1.0 + f.a1 * cw + f.a2 * c2;
    di :: Float64 := 0.0 - (f.a1 * sw + f.a2 * s2);

    num :: Float64 := nr * nr + ni * ni;
    den :: Float64 := dr * dr + di * di;
    if den == 0.0 { return 0.0; }
    return vmath.sqrt(num / den);
}

# ---------------------------------------------------------------------------
# RBJ designers. `q` is the quality factor; for a Butterworth response
# use q = 1/sqrt(2) (~0.7071).
# ---------------------------------------------------------------------------

fn :: vsignal lowpass(cutoff :: Float64, fs :: Float64,
                      q :: Float64) -> vsignal.Biquad {
    if fs <= 0.0 { throw "vsignal.lowpass: fs must be positive"; }
    if cutoff <= 0.0 || cutoff >= fs / 2.0 {
        throw "vsignal.lowpass: cutoff must be in (0, fs/2)";
    }
    if q <= 0.0 { throw "vsignal.lowpass: q must be positive"; }
    w0 :: Float64 := 2.0 * vmath.pi * cutoff / fs;
    cw :: Float64 := vmath.cos(w0);
    alpha :: Float64 := vmath.sin(w0) / (2.0 * q);
    a0 :: Float64 := 1.0 + alpha;
    b0 :: Float64 := ((1.0 - cw) / 2.0) / a0;
    b1 :: Float64 := (1.0 - cw) / a0;
    b2 :: Float64 := ((1.0 - cw) / 2.0) / a0;
    a1 :: Float64 := (0.0 - 2.0 * cw) / a0;
    a2 :: Float64 := (1.0 - alpha) / a0;
    return vsignal._from_coeffs(b0, b1, b2, a1, a2);
}

fn :: vsignal highpass(cutoff :: Float64, fs :: Float64,
                       q :: Float64) -> vsignal.Biquad {
    if fs <= 0.0 { throw "vsignal.highpass: fs must be positive"; }
    if cutoff <= 0.0 || cutoff >= fs / 2.0 {
        throw "vsignal.highpass: cutoff must be in (0, fs/2)";
    }
    if q <= 0.0 { throw "vsignal.highpass: q must be positive"; }
    w0 :: Float64 := 2.0 * vmath.pi * cutoff / fs;
    cw :: Float64 := vmath.cos(w0);
    alpha :: Float64 := vmath.sin(w0) / (2.0 * q);
    a0 :: Float64 := 1.0 + alpha;
    b0 :: Float64 := ((1.0 + cw) / 2.0) / a0;
    b1 :: Float64 := (0.0 - (1.0 + cw)) / a0;
    b2 :: Float64 := ((1.0 + cw) / 2.0) / a0;
    a1 :: Float64 := (0.0 - 2.0 * cw) / a0;
    a2 :: Float64 := (1.0 - alpha) / a0;
    return vsignal._from_coeffs(b0, b1, b2, a1, a2);
}

# Band-pass with 0 dB peak gain.
fn :: vsignal bandpass(cutoff :: Float64, fs :: Float64,
                       q :: Float64) -> vsignal.Biquad {
    if fs <= 0.0 { throw "vsignal.bandpass: fs must be positive"; }
    if cutoff <= 0.0 || cutoff >= fs / 2.0 {
        throw "vsignal.bandpass: cutoff must be in (0, fs/2)";
    }
    if q <= 0.0 { throw "vsignal.bandpass: q must be positive"; }
    w0 :: Float64 := 2.0 * vmath.pi * cutoff / fs;
    cw :: Float64 := vmath.cos(w0);
    alpha :: Float64 := vmath.sin(w0) / (2.0 * q);
    a0 :: Float64 := 1.0 + alpha;
    b0 :: Float64 := alpha / a0;
    b1 :: Float64 := 0.0;
    b2 :: Float64 := (0.0 - alpha) / a0;
    a1 :: Float64 := (0.0 - 2.0 * cw) / a0;
    a2 :: Float64 := (1.0 - alpha) / a0;
    return vsignal._from_coeffs(b0, b1, b2, a1, a2);
}

fn :: vsignal notch(cutoff :: Float64, fs :: Float64,
                    q :: Float64) -> vsignal.Biquad {
    if fs <= 0.0 { throw "vsignal.notch: fs must be positive"; }
    if cutoff <= 0.0 || cutoff >= fs / 2.0 {
        throw "vsignal.notch: cutoff must be in (0, fs/2)";
    }
    if q <= 0.0 { throw "vsignal.notch: q must be positive"; }
    w0 :: Float64 := 2.0 * vmath.pi * cutoff / fs;
    cw :: Float64 := vmath.cos(w0);
    alpha :: Float64 := vmath.sin(w0) / (2.0 * q);
    a0 :: Float64 := 1.0 + alpha;
    b0 :: Float64 := 1.0 / a0;
    b1 :: Float64 := (0.0 - 2.0 * cw) / a0;
    b2 :: Float64 := 1.0 / a0;
    a1 :: Float64 := (0.0 - 2.0 * cw) / a0;
    a2 :: Float64 := (1.0 - alpha) / a0;
    return vsignal._from_coeffs(b0, b1, b2, a1, a2);
}

# vsignal_test.vy - filter and DSP checks.
#
#   ./vynec --compile examples/libs/vsignal_test.vy
#   ./examples/libs/vsignal_test

use external "vsignal/vsignal.vy";
use external "vsignal/Spectral.vy";
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

out("=== vsignal ===");

# ---- moving average -------------------------------------------------
data :: Array<Float64> := [1.0, 2.0, 3.0, 4.0, 5.0];
s :: vsignal.Series := vsignal.Series(data, data.size());

ma :: Array<Float64> := vsignal.moving_average(s, 3);
check("ma-size", ma.size() == 5);
check("ma-0", close(ma[0], 1.0, 1e-12));
check("ma-1", close(ma[1], 1.5, 1e-12));
check("ma-2", close(ma[2], 2.0, 1e-12));
check("ma-3", close(ma[3], 3.0, 1e-12));
check("ma-4", close(ma[4], 4.0, 1e-12));

# ---- exponential smoothing -----------------------------------------
ex :: Array<Float64> := vsignal.exponential(s, 0.5);
check("exp-0", close(ex[0], 1.0, 1e-12));
check("exp-1", close(ex[1], 1.5, 1e-12));
check("exp-2", close(ex[2], 2.25, 1e-12));
check("exp-3", close(ex[3], 3.125, 1e-12));
check("exp-4", close(ex[4], 4.0625, 1e-12));

# ---- FIR ------------------------------------------------------------
xs4 :: Array<Float64> := [1.0, 2.0, 3.0, 4.0];
s4 :: vsignal.Series := vsignal.Series(xs4, xs4.size());
coeffs :: Array<Float64> := [0.5, 0.5];
fir :: Array<Float64> := vsignal.fir(s4, coeffs);
check("fir-0", close(fir[0], 0.5, 1e-12));
check("fir-1", close(fir[1], 1.5, 1e-12));
check("fir-2", close(fir[2], 2.5, 1e-12));
check("fir-3", close(fir[3], 3.5, 1e-12));

# ---- median ---------------------------------------------------------
xm :: Array<Float64> := [1.0, 10.0, 2.0, 3.0, 100.0, 4.0];
sm :: vsignal.Series := vsignal.Series(xm, xm.size());
med :: Array<Float64> := vsignal.median(sm, 3);
check("median-0", close(med[0], 1.0, 1e-12));
check("median-1", close(med[1], 5.5, 1e-12));
check("median-2", close(med[2], 2.0, 1e-12));
check("median-3", close(med[3], 3.0, 1e-12));
check("median-4", close(med[4], 3.0, 1e-12));
check("median-5", close(med[5], 4.0, 1e-12));

# ---- biquad design and response ------------------------------------
q_butter :: Float64 := vmath.sqrt(0.5);
lp :: vsignal.Biquad := vsignal.lowpass(100.0, 1000.0, q_butter);
check("lp-dc", close(vsignal.biquad_response(lp, 0.0, 1000.0), 1.0, 1e-9));
check("lp-cutoff", close(vsignal.biquad_response(lp, 100.0, 1000.0),
                         0.70710678, 0.01));
check("lp-stop", vsignal.biquad_response(lp, 450.0, 1000.0) < 0.05);

hp :: vsignal.Biquad := vsignal.highpass(100.0, 1000.0, q_butter);
check("hp-dc", vsignal.biquad_response(hp, 0.0, 1000.0) < 1e-9);
check("hp-high", close(vsignal.biquad_response(hp, 450.0, 1000.0), 1.0, 0.05));

# ---- biquad impulse / hand values ----------------------------------
simple :: vsignal.Biquad := vsignal.biquad_new(0.5, 0.5, 0.0, 0.0, 0.0);
imp :: Array<Float64> := [1.0, 0.0, 0.0, 0.0];
simp_s :: vsignal.Series := vsignal.Series(imp, imp.size());
simp_out :: Array<Float64> := vsignal.biquad_apply(simp_s, simple);
check("biquad-imp-0", close(simp_out[0], 0.5, 1e-12));
check("biquad-imp-1", close(simp_out[1], 0.5, 1e-12));
check("biquad-imp-2", close(simp_out[2], 0.0, 1e-12));

# ---- streaming filters ---------------------------------------------
bq :: vsignal.Biquad := vsignal.biquad_new(0.5, 0.5, 0.0, 0.0, 0.0);
check("biquad-step-0", close(vsignal.biquad_step(bq, 1.0), 0.5, 1e-12));
check("biquad-step-1", close(vsignal.biquad_step(bq, 0.0), 0.5, 1e-12));
check("biquad-step-2", close(vsignal.biquad_step(bq, 0.0), 0.0, 1e-12));

e :: vsignal.Exponential := vsignal.exp_new(0.5);
check("exp-step-0", close(vsignal.exp_step(e, 1.0), 1.0, 1e-12));
check("exp-step-1", close(vsignal.exp_step(e, 3.0), 2.0, 1e-12));
check("exp-step-2", close(vsignal.exp_step(e, 5.0), 3.5, 1e-12));

m :: vsignal.MovingAverage := vsignal.ma_new(3);
check("ma-step-0", close(vsignal.ma_step(m, 1.0), 1.0, 1e-12));
check("ma-step-1", close(vsignal.ma_step(m, 2.0), 1.5, 1e-12));
check("ma-step-2", close(vsignal.ma_step(m, 3.0), 2.0, 1e-12));
check("ma-step-3", close(vsignal.ma_step(m, 4.0), 3.0, 1e-12));
check("ma-step-4", close(vsignal.ma_step(m, 5.0), 4.0, 1e-12));

# ---- windows --------------------------------------------------------
hann5 :: Array<Float64> := vsignal.hann(5);
check("hann-0", close(hann5[0], 0.0, 1e-12));
check("hann-mid", close(hann5[2], 1.0, 1e-12));
check("hann-4", close(hann5[4], 0.0, 1e-12));

hamm5 :: Array<Float64> := vsignal.hamming(5);
check("hamming-0", close(hamm5[0], 0.08, 1e-12));
check("hamming-mid", close(hamm5[2], 1.0, 1e-12));

black5 :: Array<Float64> := vsignal.blackman(5);
check("blackman-0", close(black5[0], 0.0, 1e-9));
check("blackman-mid", close(black5[2], 1.0, 1e-12));

# ---- resampling -----------------------------------------------------
xr :: Array<Float64> := [0.0, 10.0];
sr :: vsignal.Series := vsignal.Series(xr, xr.size());
lin :: Array<Float64> := vsignal.linear(sr, 3);
check("resample-0", close(lin[0], 0.0, 1e-12));
check("resample-1", close(lin[1], 5.0, 1e-12));
check("resample-2", close(lin[2], 10.0, 1e-12));

xd :: Array<Float64> := [0.0, 1.0, 2.0, 3.0, 4.0, 5.0];
sd :: vsignal.Series := vsignal.Series(xd, xd.size());
dec :: Array<Float64> := vsignal.decimate(sd, 2);
check("decimate-size", dec.size() == 3);
check("decimate-2", close(dec[2], 4.0, 1e-12));

# ---- spectral -------------------------------------------------------
sine :: Array<Float64> := [];
through i :: 0..15 -> loop {
    sine.push(vmath.sin(2.0 * vmath.pi * 2.0 * float64(i) / 16.0));
};
df :: Float64 := vsignal.dominant_frequency(sine, 16, 16.0);
check("spectrum-dominant", close(df, 2.0, 1e-9));

mags :: Array<Float64> := vsignal.magnitude(sine, 16);
check("spectrum-bins", mags.size() == 9);
check("spectrum-peak", close(mags[2], 8.0, 0.5));

if failures[0] > 0 {
    out("vsignal: " + string(failures[0]) + " check(s) failed");
    exit(1);
}
out("vsignal: all checks passed");

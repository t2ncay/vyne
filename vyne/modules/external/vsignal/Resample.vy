# vsignal/Resample.vy - sample-rate conversion.
#
# `linear` interpolates onto an arbitrary output length. `decimate` and
# `upsample` are the common integer-factor cases; decimate picks every
# factor-th sample and does not apply an anti-alias filter, so low-pass
# the signal first when that matters.

use "Types.vy";
use "Kernels.vy";

ruleset { dynamic_casting };

module vsignal;

fn :: vsignal linear(s :: vsignal.Series,
                     out_n :: Int64) -> Array<Float64> {
    if s.n <= 0 { throw "vsignal.linear: empty series"; }
    if out_n <= 0 { throw "vsignal.linear: out_n must be positive"; }
    dst :: Array<Float64> := [];
    through i :: 0..out_n-1 -> loop { dst.push(0.0); };
    vsignal.k_resample_linear(s.data, s.n, out_n, dst);
    return dst;
}

fn :: vsignal decimate(s :: vsignal.Series,
                       factor :: Int64) -> Array<Float64> {
    if s.n <= 0 { throw "vsignal.decimate: empty series"; }
    if factor <= 0 { throw "vsignal.decimate: factor must be positive"; }
    out_n :: Int64 := s.n / factor;
    if out_n <= 0 { throw "vsignal.decimate: factor larger than series"; }
    dst :: Array<Float64> := [];
    through i :: 0..out_n-1 -> loop {
        dst.push(s.data[i * factor]);
    };
    return dst;
}

fn :: vsignal upsample(s :: vsignal.Series,
                       factor :: Int64) -> Array<Float64> {
    if s.n <= 0 { throw "vsignal.upsample: empty series"; }
    if factor <= 0 { throw "vsignal.upsample: factor must be positive"; }
    out_n :: Int64 := (s.n - 1) * factor + 1;
    return vsignal.linear(s, out_n);
}

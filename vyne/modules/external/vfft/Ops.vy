# vfft/Ops.vy — user-facing transforms with cached plan per N.
#
# The two costs that dominated the original kernel were:
#   1. Twiddle table rebuilt on every call (N/2 cos + N/2 sin).
#   2. bit_reverse computed in O(k²) per output index.
# Both are now built once per N and stored in _plan.

ruleset { dynamic_casting };

use "Kernels.vy";
use native vmath;

module vfft;

# A Plan is the precomputed state for a given FFT length. Built once
# on the first call with a new N, reused on every subsequent call.
interface Plan {
    n      :: Int64,
    bits   :: Int64,
    tw_re  :: Array<Float64>,
    tw_im  :: Array<Float64>,
    bitrev :: Array<Int64>,
}

fn :: vfft make_plan(n :: Int64) -> vfft.Plan {
    bits :: Int64 := vfft.log2_exact(n);
    half :: Int64 := n / 2;
    two_pi :: Float64 := 6.283185307179586;

    tw_re :: Array<Float64> := [];
    tw_im :: Array<Float64> := [];
    through k :: 0..half-1 -> loop {
        theta :: Float64 := 0.0 - two_pi * float64(k) / float64(n);
        tw_re.push(vmath.cos(theta));
        tw_im.push(vmath.sin(theta));
    };

    bitrev :: Array<Int64> := [];
    through i :: 0..n-1 -> loop {
        bitrev.push(vfft.bit_reverse(i, bits));
    };

    return vfft.Plan(n, bits, tw_re, tw_im, bitrev);
}

# Cached plan. Invalid when _plan_n doesn't match the requested N.
_plan   :: vfft.Plan := null;
_plan_n :: Int64     := 0;

fn :: vfft ensure_plan(n :: Int64) -> Int64 {
    if _plan_n == n { return 0; }
    _plan   = vfft.make_plan(n);
    _plan_n = n;
    return 0;
}

fn :: vfft forward(re :: Array, im :: Array) -> Int64 {
    n :: Int64 := re.size();
    if n <= 1 { return 0; }
    vfft.ensure_plan(n);

    re_n :: Array<Float64> := [];
    im_n :: Array<Float64> := [];
    through i :: 0..n-1 -> loop {
        re_n.push(re[i]);
        im_n.push(im[i]);
    };

    vfft.fft_kernel(re_n, im_n, _plan.n, _plan.bits,
                    _plan.tw_re, _plan.tw_im, _plan.bitrev);

    through i :: 0..n-1 -> loop {
        re[i] = re_n[i];
        im[i] = im_n[i];
    };
    return 0;
}

fn :: vfft inverse(re :: Array, im :: Array) -> Int64 {
    n :: Int64 := re.size();
    if n <= 1 { return 0; }
    vfft.ensure_plan(n);

    # Conjugate input, forward, conjugate output, scale by 1/N.
    re_n :: Array<Float64> := [];
    im_n :: Array<Float64> := [];
    through i :: 0..n-1 -> loop {
        re_n.push(re[i]);
        im_n.push(0.0 - im[i]);
    };

    vfft.fft_kernel(re_n, im_n, _plan.n, _plan.bits,
                    _plan.tw_re, _plan.tw_im, _plan.bitrev);

    inv_n :: Float64 := 1.0 / float64(n);
    through i :: 0..n-1 -> loop {
        re[i] = re_n[i] * inv_n;
        im[i] = (0.0 - im_n[i]) * inv_n;
    };
    return 0;
}

fn :: vfft next_pow2(n :: Int64) -> Int64 {
    if n <= 1 { return 1; }
    r :: Int64 := 1;
    while r < n { r = r * 2; }
    return r;
}

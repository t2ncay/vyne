# vfft/Kernels.vy — the numerical hot loops.
#
# Everything else in vfft is bookkeeping. These functions dominate
# runtime and are written to run under the native (unboxed) ABI:
# `Array<Float64>` params become bare `double*` in the emitted C, and
# `Array<Int64>` becomes `int64_t*`.
#
# Native-to-native calls with array params are NOT supported by the
# current codegen (tryEmitNativeCall rejects RawArrayPtr args). So
# fft_kernel is a leaf — it calls nothing that takes an array.
#
# Version 0.2.0: bitwise operators (>> << & | ^ ~) are available now
# that the parser propagates declared types onto plain variable reads.
# pow2, log2_exact, and bit_reverse no longer fall back to arithmetic.

ruleset { dynamic_casting };

use native vmath;

module vfft;

# 2^e for e >= 0. Returns 1 for e <= 0, matching the historical
# arithmetic-loop behaviour for out-of-range inputs.
fn :: vfft pow2(e :: Int64) -> Int64 {
    if e <= 0 { return 1; }
    return 1 << e;
}

# log2(n) for n a power of two, or -1 otherwise. Doubling becomes
# a left shift; the walk is unchanged.
fn :: vfft log2_exact(n :: Int64) -> Int64 {
    if n <= 0 { return -1; }
    b :: Int64 = 0;
    r :: Int64 = 1;
    while r < n {
        r = r << 1;
        b = b + 1;
    }
    if r != n { return -1; }
    return b;
}

# Reverse the low k bits of i.
#
# Previous version walked pow2 twice per bit and did an integer
# division and modulo — O(k²) per call, and the pow2 calls were
# function invocations in the unoptimised build. The shift/mask
# form is O(k), calls nothing, and is what the arithmetic version
# was approximating.
#
# Guard against k = 0: the loop body never runs, result stays 0.
# That matches the old behaviour (0..(-1) is an empty range).
fn :: vfft bit_reverse(i :: Int64, k :: Int64) -> Int64 {
    result :: Int64 = 0;
    through b :: 0..k-1 -> loop {
        bit :: Int64 = (i >> b) & 1;
        result = result | (bit << (k - 1 - b));
    };
    return result;
}

# In-place radix-2 Cooley-Tukey DIT FFT. n must be a power of two;
# bits = log2(n). Modifies re and im.
#
# Unchanged from 0.1.0. The butterfly does not touch bitwise ops.
fn :: vfft fft_kernel(re :: Array<Float64>, im :: Array<Float64>,
                      n :: Int64, bits :: Int64,
                      tw_re :: Array<Float64>,
                      tw_im :: Array<Float64>,
                      bitrev :: Array<Int64>) -> Int64 {
    # Bit-reversal permutation via the cached table. O(N) instead of
    # O(N·k²) — the pow2 and integer-division costs are gone.
    through i :: 0..n-1 -> loop {
        j :: Int64 = bitrev[i];
        if i < j {
            tr :: Float64 = re[i];
            re[i] = re[j];
            re[j] = tr;
            ti :: Float64 = im[i];
            im[i] = im[j];
            im[j] = ti;
        }
    };

    length :: Int64 = 2;
    through stage :: 0..bits-1 -> loop {
        half :: Int64 = length / 2;
        step :: Int64 = n / length;
        n_blocks :: Int64 = n / length;

        base :: Int64 = 0;
        through blk :: 0..n_blocks-1 -> loop {
            through k :: 0..half-1 -> loop {
                ti_idx :: Int64 = k * step;
                wr :: Float64 = tw_re[ti_idx];
                wi :: Float64 = tw_im[ti_idx];

                i1 :: Int64 = base + k;
                i2 :: Int64 = i1 + half;

                ur :: Float64 = re[i1];
                ui :: Float64 = im[i1];
                vr :: Float64 = re[i2] * wr - im[i2] * wi;
                vi :: Float64 = re[i2] * wi + im[i2] * wr;

                re[i1] = ur + vr;
                im[i1] = ui + vi;
                re[i2] = ur - vr;
                im[i2] = ui - vi;
            };
            base = base + length;
        };

        length = length * 2;
    };

    return 0;
}

# vsignal — filters and DSP for Vyne

`vsignal` is the shared signal-processing layer for Vyne. It follows the same
kernel/wrapper structure as `vstat` and `vinterp`: `Kernels.vy` holds the
unboxed loops, the other leaves validate and allocate, and `vsignal.vy`
re-exports the core surface.

```vyne
use external "vsignal/vsignal.vy";

data :: Array<Float64> := [1.0, 2.0, 3.0, 4.0, 5.0];
s :: vsignal.Series := vsignal.Series(data, data.size());

out(vsignal.moving_average(s, 3));   # [1.0, 1.5, 2.0, 3.0, 4.0]

# RBJ low-pass at 100 Hz for a 1 kHz sample rate (Butterworth Q).
f :: vsignal.Biquad := vsignal.lowpass(100.0, 1000.0, 0.70710678);
out(vsignal.biquad_response(f, 100.0, 1000.0));  # ~0.707 (-3 dB)
```

## Layout

| File | Role |
| :--- | :--- |
| `Types.vy` | `Series`, `Biquad`, `Exponential`, `MovingAverage` |
| `Constructors.vy` | `from_array`, `to_array`, `zeros` |
| `Kernels.vy` | native-ABI loops: moving average, exp, FIR, biquad, median, resample |
| `Iir.vy` | biquad constructors, streaming step, RBJ low/high/band/notch |
| `Fir.vy` | offline FIR / moving average / exp / median, streaming states |
| `Windows.vy` | Hann, Hamming, Blackman, rectangular |
| `Resample.vy` | linear resampling, decimate, upsample |
| `Spectral.vy` | magnitude spectrum, dominant frequency (desktop, `vfft`) |
| `vsignal.vy` | facade for the core filtering surface |

## Portability

The library is pure Vyne, but the public surface uses interface types. The
desktop runtime implements boxed structs; the AVR runtime does not yet, so
these APIs are desktop-only until the MCU struct runtime lands. The kernels
themselves are plain array loops and are ready for that backend.

## `Series` and streaming state

`Series` pairs a typed `Array<Float64>` with its length so wrappers can call the
`k_*` kernels with unboxed `double*` arguments. Construct one directly with
`vsignal.Series(data, n)` (zero-copy) or use `vsignal.from_array(xs)` when you
have a boxed `Array`.

Streaming filters keep state in an `Array<Float64>` field so updates are shared
through every reference:

- `Biquad` — `state = [z1, z2]` (Direct Form II Transposed)
- `Exponential` — `state = [y, initialized]`
- `MovingAverage` — `buffer` plus `state = [head, count, sum]`

```vyne
f := vsignal.biquad_new(0.5, 0.5, 0.0, 0.0, 0.0);
out(vsignal.biquad_step(f, 1.0));   # 0.5
out(vsignal.biquad_step(f, 0.0));   # 0.5
out(vsignal.biquad_step(f, 0.0));   # 0.0
```

## Offline filters

| Function | Result |
| :--- | :--- |
| `moving_average(s, window)` | trailing average, edge-clamped startup |
| `exponential(s, alpha)` | first-order smoother, `alpha` in `[0,1]` |
| `fir(s, coeffs)` | causal FIR, zero initial history |
| `median(s, window)` | causal median, edge-clamped, O(n·w²) |

## Biquad IIR

| Function | Result |
| :--- | :--- |
| `biquad_new(b0,b1,b2,a1,a2)` | explicit normalized coefficients |
| `biquad_step(f, x)` | one streaming sample |
| `biquad_apply(s, f)` | offline whole-buffer application |
| `biquad_response(f, freq, fs)` | magnitude response at `freq` |
| `biquad_reset(f)` | clear `z1`/`z2` |
| `lowpass(cutoff, fs, q)` | RBJ low-pass |
| `highpass(cutoff, fs, q)` | RBJ high-pass |
| `bandpass(cutoff, fs, q)` | RBJ band-pass (0 dB peak) |
| `notch(cutoff, fs, q)` | RBJ notch |

Use `q = 1/sqrt(2) ≈ 0.7071` for a Butterworth response. Coefficients are
normalized by `a0`; the recurrence is
`y[n] = b0 x[n] + b1 x[n-1] + b2 x[n-2] - a1 y[n-1] - a2 y[n-2]`.

## Windows and resampling

`hann(n)`, `hamming(n)`, `blackman(n)`, `rectangular(n)`, and
`apply_window(s, w)`.

`linear(s, out_n)`, `decimate(s, factor)`, `upsample(s, factor)`. `decimate`
picks every `factor`-th sample and does not anti-alias; low-pass first when it
matters.

## Spectral helpers (desktop)

`Spectral.vy` depends on `vfft` (and therefore `vmath`). It is **not** imported
by the facade so filtering-only programs do not pull in the transform
machinery:

```vyne
use external "vsignal/Spectral.vy";

mags := vsignal.magnitude(xs, n);                  # N/2+1 bins, zero-padded
f0   := vsignal.dominant_frequency(xs, n, fs);     # Hz, bin resolution
pw   := vsignal.power(xs, n);
```

## Verification

```bash
vynec --compile examples/libs/vsignal_test.vy
./examples/libs/vsignal_test

vynec --compile examples/libs/vsignal_bench.vy
./examples/libs/vsignal_bench
```

The test checks moving-average / exponential / FIR / median hand values, the
RBJ low-pass response at DC, cutoff, and stopband, biquad impulse and streaming
state, windows, resampling, and a 16-point spectrum peak.

# vfft

Iterative radix-2 FFT for Vyne.

**Version:** 0.2.0
**Status:** unstable — public surface may change between releases
**Module name:** `vfft`
**Import path:** `use external "vfft/vfft.vy";`
**Requires:** `vmath` (trigonometric functions)

---

## Contents

1. [Overview](#overview)
2. [Quick start](#quick-start)
3. [Importing](#importing)
4. [Module layout](#module-layout)
5. [The `Plan` type](#the-plan-type)
6. [Reference semantics and the memory model](#reference-semantics-and-the-memory-model)
7. [API reference](#api-reference)
8. [Worked example](#worked-example)
9. [Limitations](#limitations)
10. [Design notes](#design-notes)
11. [Roadmap](#roadmap)
12. [Version history](#version-history)

---

## Overview

`vfft` is a pure-Vyne Fast Fourier Transform library. It computes the
forward and inverse discrete Fourier transform of a complex sequence
represented as two parallel `Array<Float64>` buffers — one for the
real parts, one for the imaginary parts.

The implementation is a classic iterative radix-2 Cooley-Tukey
decimation-in-time FFT. It is deliberately minimal: no
multi-dimensional transforms, no real-input specialization, no
strided or batched variants, no FFTW-style codelet generation. It does
one thing well — transform a power-of-two-length complex sequence in
place — and it does so with a precomputed twiddle table and
bit-reversal index that are amortized across calls.

That last point is what distinguishes `vfft` from a naïve textbook
implementation. The expensive parts of an FFT are not the butterflies
— those are `O(N log N)` arithmetic — they are the trigonometric
setup and the bit-reversal permutation, both of which are `O(N)` or
worse but constant across repeated transforms of the same length.
`vfft` computes them once, stores them in a `Plan`, and reuses that
plan on every subsequent call with the same `N`.

The library compiles to code where the inner butterfly loop runs over
bare `double*` arguments with no boxing, no tag checks, and no arena
traffic. A well-typed `vfft` program produces the same machine code
shape as a hand-written C FFT of the same algorithm.

---

## Quick start

```vyne
use external "vfft/vfft.vy";
use native vmath;

ruleset { dynamic_casting };

N :: Int64 = 1024;

# Build a real-valued test signal.
vmath.seed(42);
re :: Array<Float64> = [];
im :: Array<Float64> = [];
through i :: 0..N-1 -> loop {
    re.push(vmath.random_float(-1.0, 1.0));
    im.push(0.0);
};

# Forward transform, then inverse. Round-trip is the identity
# (up to floating-point rounding).
vfft.forward(re, im);
vfft.inverse(re, im);

out("re[0] = " + string(re[0]));
```

Both functions transform their arguments **in place**. There is no
return value beyond a status code.

If you are not sure your length is a power of two, use
`vfft.next_pow2(n)`:

```vyne
n_orig :: Int64 = 1000;
n_fft  :: Int64 = vfft.next_pow2(n_orig);   # 1024
# Zero-pad your signal to n_fft before transforming.
```

---

## Importing

```vyne
use external "vfft/vfft.vy";
use native vmath;
```

`vfft.vy` is a facade. It pulls in the two implementation modules:

| Module       | Contents                                                                                            |
| ------------ | --------------------------------------------------------------------------------------------------- |
| `Kernels.vy` | Arithmetic primitives (`pow2`, `log2_exact`, `bit_reverse`) and the butterfly kernel (`fft_kernel`) |
| `Ops.vy`     | Plan management (`make_plan`, `ensure_plan`) and the user-facing wrappers (`forward`, `inverse`)    |

You may import the leaf modules directly if you want to reuse the
kernels in a larger numerical routine, but the facade is the
recommended entry point.

---

## Module layout

```
vfft/
├── vfft.vy            # facade: use + deploy
├── Kernels.vy         # pow2, log2_exact, bit_reverse, fft_kernel
└── Ops.vy             # Plan interface, make_plan, ensure_plan, forward, inverse
```

### Two-layer design

Every user-visible call is a two-layer composition:

1. **Kernels** (`Kernels.vy`): loops over raw `Array<Float64>` and
   `Array<Int64>`. Every function takes typed-array parameters and
   returns either `Int64` (a status code, always 0) or `Float64`. This
   shape makes the call site eligible for the native array ABI — the
   emitter passes `.data` directly and the body compiles to a bare C
   loop over `double*`.

2. **Ops** (`Ops.vy`): the `Plan` interface, the plan cache, and the
   two transforms that a user actually calls.

The split exists for the same reason it does in `vlin`: a function
whose parameters are all `Array<Float64>` and whose return type is
`Int64` loses the native ABI the moment a `Matrix`-like value enters
its signature. Keeping the plan management in `Ops.vy` and the numeric
loop in `Kernels.vy` is what allows the inner loop to run unboxed.

### The butterfly kernel is a leaf

`fft_kernel` calls nothing that takes an array parameter. This is
deliberate: the current Vyne codegen does not support native-to-native
calls with array arguments (the emitter's `tryEmitNativeCall` rejects
`RawArrayPtr` parameters on the callee side). A kernel that called
another array-taking kernel would fall back to the boxed ABI and lose
the entire benefit of the native surface. Keeping `fft_kernel` a leaf
is what keeps it fast.

---

## The `Plan` type

```vyne
interface Plan {
    n      :: Int64,
    bits   :: Int64,
    tw_re  :: Array<Float64>,
    tw_im  :: Array<Float64>,
    bitrev :: Array<Int64>,
}
```

A `Plan` holds everything that depends on the transform length but not
on the data:

| Field    | Meaning                                                                    |
| -------- | -------------------------------------------------------------------------- |
| `n`      | Transform length. Must be a power of two.                                  |
| `bits`   | `log2(n)`. Precomputed so the kernel does not recompute it.                |
| `tw_re`  | `n/2` cosine twiddles: `cos(-2πk/n)` for `k = 0 … n/2 - 1`.                |
| `tw_im`  | `n/2` sine twiddles: `sin(-2πk/n)` for `k = 0 … n/2 - 1`.                  |
| `bitrev` | `n` bit-reversal indices. `bitrev[i]` is `i` with its low `bits` reversed. |

You almost never construct a `Plan` directly. `vfft.forward` and
`vfft.inverse` call `vfft.ensure_plan(n)` internally, which builds a
plan on first use and returns it unchanged on subsequent calls with
the same `n`.

### The plan cache

`Ops.vy` maintains two module-global variables:

```vyne
_plan   :: vfft.Plan = null;
_plan_n :: Int64     = 0;
```

`ensure_plan(n)` checks `_plan_n == n`; if so, it returns immediately.
Otherwise it calls `make_plan(n)` and stores the result.

The cache holds exactly **one** plan at a time. Calling `forward` with
`n = 1024`, then `n = 2048`, then `n = 1024` again will rebuild the
plan twice. In a workload that alternates transform lengths, that
cost dominates. In a workload that uses one length — the common case —
the plan is built once and reused for the lifetime of the process.

### Using the plan directly

`make_plan` and the `Plan` interface are public because they are the
only way to inspect or serialize the precomputed tables. Most programs
should not need them.

```vyne
plan :: vfft.Plan = vfft.make_plan(1024);
out("n        = " + string(plan.n));
out("bits     = " + string(plan.bits));
out("bitrev[3]= " + string(plan.bitrev[3]));   # 768 for n=1024
```

---

## Reference semantics and the memory model

### Arrays are passed by reference, and modified in place

`vfft.forward` and `vfft.inverse` write their results back into the
arrays you pass. They do not allocate a new buffer.

```vyne
re :: Array<Float64> = [...];
im :: Array<Float64> = [...];

vfft.forward(re, im);   # re and im now hold the frequency-domain data
```

If you need the original signal preserved, copy it first:

```vyne
re_copy :: Array<Float64> = [];
through i :: 0..n-1 -> loop { re_copy.push(re[i]); };
```

### The plan is arena-allocated, and the arena is `vmem`-managed

This is the one thing that will bite you if you use `vfft` inside a
`region` block. Read it carefully.

The plan's backing storage — the `Plan` struct, the twiddle arrays,
the bit-reversal table — is allocated on the arena. `vmem.rewind`
frees everything allocated after a checkpoint, including the plan if
the plan was built after the checkpoint.

The plan cache's invalidation sentinel, `_plan_n`, is a plain `Int64`
global. It is **not** arena-allocated. It survives every rewind.

This means: if the plan is first built inside a region, the region's
rewind frees the plan but leaves `_plan_n` set to the requested length.
The next call to `ensure_plan` sees a matching sentinel, returns early,
and the kernel dereferences freed memory.

**The fix is to warm the plan at top-level scope, before any region
loop:**

```vyne
# Warm the plan at top-level scope. This allocates the plan below
# every region checkpoint, so no rewind can free it.
vfft.forward(re, im);
vfft.inverse(re, im);

through iter :: 0..ITERS-1 -> loop {
    region step {
        vfft.forward(re, im);
        vfft.inverse(re, im);
    };
};
```

After the warmup, every in-loop `ensure_plan` hits the sentinel and
returns without allocating. The plan survives every rewind.

The same rule applies if you build the plan explicitly with
`vfft.make_plan(...)` — allocate it outside any region whose body will
call `forward` or `inverse`.

See **Limitations** for the general statement of this trap. It is not
specific to `vfft`; any lazily-cached, arena-allocated structure has
the same failure mode.

### Every call allocates scratch

`forward` and `inverse` each allocate two temporary `Array<Float64>`
buffers of length `n` per call:

```vyne
re_n :: Array<Float64> = [];
im_n :: Array<Float64> = [];
```

These hold the caller's data while the kernel works, and are copied
back into the caller's arrays on the way out. The copy is not strictly
necessary — `fft_kernel` could operate directly on the caller's
buffers — but the current kernel signature takes `Array<Float64>`
parameters and the current emitter cannot prove at the call site that
the caller's arrays have the element type the kernel expects. The
scratch buffers exist to keep the native ABI.

Under the default memory model (arena lives until process exit), a
loop that calls `forward` and `inverse` `K` times allocates `4·K`
temporary buffers. Use a `region` block or `vmem.checkpoint` to
reclaim them, with the caveat above about warming the plan first.

---

## API reference

### User-facing transforms

```vyne
vfft.forward(re :: Array<Float64>, im :: Array<Float64>) -> Int64
vfft.inverse(re :: Array<Float64>, im :: Array<Float64>) -> Int64
```

**`forward`** transforms the sequence in place. On return, `re[k]` and
`im[k]` hold the real and imaginary parts of the `k`-th frequency
component. The result is **unscaled** — it is the raw DFT sum, not
normalized by `N`.

**`inverse`** performs the inverse transform. It is defined as:

```
inverse(x) = conj(forward(conj(x))) / N
```

On return, `re[i]` and `im[i]` hold the time-domain samples.
`forward(inverse(x))` and `inverse(forward(x))` are both the identity
up to floating-point rounding.

Both functions return `0` on success. They return without transforming
if `n <= 1`.

Both functions call `ensure_plan(n)` internally, where `n` is
`re.size()`. The size of `im` is not checked. If `re.size() !=
im.size()` the behavior is undefined.

### Length utilities

```vyne
vfft.next_pow2(n :: Int64) -> Int64
vfft.log2_exact(n :: Int64) -> Int64
vfft.pow2(e :: Int64) -> Int64
```

**`next_pow2`** returns the smallest power of two greater than or
equal to `n`. Returns `1` for `n <= 1`.

**`log2_exact`** returns `log2(n)` if `n` is a power of two, and `-1`
otherwise. Use this to validate an input length before transforming.

**`pow2`** returns `2^e`. Returns `1` for any `e <= 0`, matching the
historical arithmetic-loop behaviour for out-of-range inputs. For
`e >= 1` it is a single left shift.

### Plan management

```vyne
vfft.make_plan(n :: Int64) -> vfft.Plan
vfft.ensure_plan(n :: Int64) -> Int64
```

**`make_plan`** builds a fresh plan. Does not consult or update the
cache. Returns a `Plan` with `n`, `bits`, `tw_re`, `tw_im`, and
`bitrev` populated. `n` must be a power of two; the function does not
validate this and will produce a plan whose `bits` field is `-1` if
`n` is not.

**`ensure_plan`** is what `forward` and `inverse` call. If the cached
plan's length matches `n`, it is a no-op. Otherwise it rebuilds the
cache. Returns `0` unconditionally.

### Low-level kernel

```vyne
vfft.fft_kernel(re :: Array<Float64>, im :: Array<Float64>,
                n :: Int64, bits :: Int64,
                tw_re :: Array<Float64>,
                tw_im :: Array<Float64>,
                bitrev :: Array<Int64>) -> Int64
```

The radix-2 Cooley-Tukey butterfly. Transforms `re` and `im` in place
using the tables in the last three parameters. `n` must equal
`re.size()`, `bits` must equal `log2(n)`, and the twiddle arrays must
have length `n/2`.

This is part of the public surface only because the parser resolves
the name. Normal programs should call `forward` and `inverse`.
`fft_kernel` does no validation; passing a mismatched `bits` or a
short twiddle table produces a silent buffer overrun.

### Bit-reversal permutation

```vyne
vfft.bit_reverse(i :: Int64, k :: Int64) -> Int64
```

Returns `i` with its low `k` bits reversed. Used by `make_plan` to
build the `bitrev` table. Not normally called directly.

Since 0.2.0 the implementation is a shift/mask loop — one `>>`, one
`&`, one `<<`, one `|` per bit — instead of the arithmetic form that
predated bitwise operator support.

---

## Worked example

A pure tone at bin 7, reconstructed exactly.

```vyne
use external "vfft/vfft.vy";
use native vmath;

ruleset { dynamic_casting };

N :: Int64 = 256;
TWO_PI :: Float64 = 6.283185307179586;
BIN :: Int64 = 7;

# Build a sine wave at frequency BIN/N.
re :: Array<Float64> = [];
im :: Array<Float64> = [];
through i :: 0..N-1 -> loop {
    angle :: Float64 = TWO_PI * float64(BIN) * float64(i) / float64(N);
    re.push(vmath.sin(angle));
    im.push(0.0);
};

# Forward transform.
vfft.forward(re, im);

# A real sine wave has two spectral peaks: +BIN and -BIN.
# The magnitudes at k=BIN and k=N-BIN should be N/2; everything
# else should be ~0.
out("|X[7]|  = " + string(vmath.sqrt(re[7]*re[7] + im[7]*im[7])));
out("|X[249]|= " + string(vmath.sqrt(re[249]*re[249] + im[249]*im[249])));
out("|X[1]|  = " + string(vmath.sqrt(re[1]*re[1] + im[1]*im[1])));

# Inverse transform. Should recover the original sine wave.
vfft.inverse(re, im);
out("re[0] after round-trip = " + string(re[0]));   # ~0
```

Expected output:

```
|X[7]|   = 128.0     (N/2)
|X[249]| = 128.0     (N/2)
|X[1]|   = 0.0       (~0, rounding)
re[0] after round-trip = 0.0
```

The `-BIN` peak appears at index `N - BIN = 249` because of the
standard DFT symmetry for real inputs: `X[N-k] = conj(X[k])`.

---

## Limitations

### Power-of-two lengths only

`vfft` implements radix-2 Cooley-Tukey. The transform length must be a
power of two. There is no mixed-radix path, no Bluestein's algorithm
for arbitrary lengths, no zero-padding logic. Use `vfft.next_pow2` and
pad your input manually if you have a non-power-of-two signal.

### Complex input only

The transform takes separate real and imaginary buffers. A real-valued
signal must supply an imaginary buffer of zeros, and the transform
spends half its work computing values that are Hermitian-symmetric
with the other half. A real-input fast path (`rfft`) is not yet
implemented; see **Roadmap**.

### Single plan cache slot

The plan cache holds one plan. Alternating transform lengths thrashes
it. If your workload alternates between two lengths, build two plans
with `make_plan` and call `fft_kernel` directly, or accept the
`O(N)` rebuild cost on every switch.

### The plan is not safe across `region` rewinds

Detailed in **Reference semantics** above, but restating because it is
the single most common way to break a `vfft` program:

**Do not build the plan inside a region whose rewind will free it.
Warm it at top-level scope first.**

The compiler does not detect this. There is no compile-time error, no
runtime check, no diagnostic. The plan looks valid — the sentinel
says so — and the kernel dereferences freed memory. Symptoms range
from silent corruption to an access violation minutes into a run.

The same rule applies to `vlin` matrices built lazily inside library
functions and to any other structure whose "valid" flag and "backing
storage" have different lifetimes. It is a language-level hazard, not
a `vfft`-specific one.

### Floating-point accuracy

The FFT is implemented in `double` with no compensated summation, no
split-radix trick, no extended precision. For a 1024-point transform
round-tripped a thousand times, the result drifts by a few ULP. For
longer transforms and larger iteration counts the drift accumulates.
If you need bit-exact reproducibility across runs, be aware that the
summation order is fixed by the algorithm and the twiddle table, so
the drift is deterministic — but it is not zero.

### No SIMD or threading

The kernel compiles to scalar `double` arithmetic. The C compiler may
auto-vectorize the inner loops if the flags allow it, but `vfft` makes
no effort to guarantee this: no `restrict`, no aligned allocation, no
explicit AVX2/NEON intrinsics. For a single 1024-point transform,
scalar is fine. For a workload that transforms millions of signals per
second, a tuned library (FFTW, Intel MKL, PocketFFT) will be
significantly faster.

### Same weak RNG as the rest of the ecosystem

The example code in this manual uses `vmath.random_float` to seed its
test signals. The generator is a small LCG with correlated low bits.
Adequate for building a plausible-looking input; not adequate if you
want a signal with spectral properties that are actually uniform.
For a flat spectrum, build the frequency-domain data directly and call
`vfft.inverse`.

---

## Design notes

### Why a plan object

A naïve FFT computes its twiddle factors and bit-reversal permutation
on every call. Both are `O(N)` — the same order as a single butterfly
stage — so their cost is not asymptotically dominant, but the constant
factor is large: `N/2` calls to `cos` and `sin`, plus `N` iterations
of a bit-reversal routine that extracts and places one bit per step.

Moving that setup into a plan that is built once and reused reduces
per-call cost by a factor of roughly 3 for a single transform and by
much more for a loop. The `Plan` concept is borrowed directly from
FFTW, which introduced it precisely for this reason.

### Why a kernel/wrapper split

Same rationale as in `vlin`: a function whose parameters are all
`Array<Float64>` and whose return type is `Int64` compiles to a C
function taking `double*` and returning `int64_t`. No boxing, no tag
checks, no arena traffic inside the loop.

`fft_kernel` takes exactly that shape. The two wrapper functions in
`Ops.vy` take `Array` (untyped) parameters, because they need to accept
whatever the caller passes — a typed or untyped array, a boxed
value from an interface field, etc. — and normalize it into the typed
scratch buffers the kernel expects.

If the shape check lived inside the kernel, the kernel would need to
take `Plan` values and check the plan length on every call, and it
would lose the ABI. Splitting validation into the wrapper and
iteration into the kernel is what makes the fast path available.

### Why conjugate-based inverse

The inverse DFT is `(1/N) · conj(DFT(conj(x)))`. Instead of writing a
second kernel with negated twiddles, `inverse` conjugates its input on
the way into the scratch buffers, calls the same `fft_kernel`, and
conjugates and scales on the way out.

Two reasons. First, it halves the surface area of the kernel — one
routine to read, one routine to debug, one routine to optimize.
Second, the twiddle table is shared: `forward` and `inverse` use the
same `tw_re` and `tw_im` arrays, so a program that calls both does
not pay for a second plan.

The cost is four extra scalar operations per element per call — the
two conjugations and the division. For an `N log N` algorithm, that
is noise.

### Why bit-reversal uses shifts

Until 0.2.0, `bit_reverse` was written arithmetically because Vyne's
parser did not propagate declared types onto plain variable reads, and
the bitwise guard in `UnaryNode::getCExpr` therefore rejected any
expression like `~x` where `x` had only been declared, never
re-annotated. The workaround extracted each bit with `(i / 2^b) % 2`
and placed it with a multiplication.

That parser gap is closed. The canonical C form is now expressible:

```vyne
bit :: Int64 = (i >> b) & 1;
result = result | (bit << (k - 1 - b));
```

This is `O(k)` per call and makes no function calls. The `O(k²)`
arithmetic version was never the bottleneck — plan construction
happens once per length — but the shift form is shorter, faster, and
matches what a reader familiar with FFT code will expect.

`pow2` and `log2_exact` use `<<` as well. Their asymptotics are
unchanged; the code is just cleaner.

### Why the plan cache is a module global

The cache must persist across calls to `forward` and `inverse`. Those
functions are stateless by design — they take only the two arrays
they transform — so the cache cannot live in their scope. A module
global is the only place left.

The consequence is that the cache is per-process, not per-call-site.
Two independent subsystems that use different transform lengths will
thrash each other's plan. This is acceptable for the library's target
use case (one program, one FFT length) and unacceptable for libraries
layered on top of `vfft`. If you are building such a library, use
`make_plan` and call `fft_kernel` directly; you own the cache.

---

## Roadmap

The following are planned or in progress.

### Real-input fast path (`rfft`)

A specialized transform for real-valued inputs. A real signal of
length `N` has a Hermitian-symmetric spectrum: `X[N-k] = conj(X[k])`.
Only `N/2 + 1` output values are unique. An `rfft` would exploit this
by packing the real input into a complex transform of length `N/2`
and unpacking on the way out. Roughly 2× speedup for real inputs.

### Out-of-place variants

`forward` and `inverse` currently copy the caller's data into scratch
buffers, transform the scratch, and copy back. An out-of-place variant
that transforms the caller's buffers directly would eliminate the
copy, at the cost of requiring the caller to own the buffers and
guarantee their element type.

Blocked on the emitter's ability to prove at the call site that the
caller's `Array` value is in fact a typed `VyneArray_f64`. The
`Array<Float64>` parameter annotation on a wrapper is not enough — the
native-ABI dispatch rejects `Array`-typed arguments that have not been
proven to be typed arrays. Fixing this is a codegen change.

### Multi-dimensional transforms

A 2-D FFT is a sequence of 1-D FFTs along each axis. The plan
structure extends naturally: a 2-D plan holds row transforms, column
transforms, and a transpose. Not implemented.

### Batched transforms

Transform `M` signals of length `N` with one call, sharing the plan.
The cache currently holds one plan; a batch API would accept a batch
of arrays and loop internally, avoiding per-call plan lookups and
per-call scratch allocation. Common in signal processing workloads.

### Region-aware plan invalidation

The plan cache should know when a region rewind has freed its backing
storage, so the next call rebuilds automatically rather than
dereferencing freed memory. This requires the runtime to expose a
generation counter that the arena bumps on every rewind, and the plan
cache to store that counter alongside `_plan_n`. Planned for a future
runtime release.

Until then, the user-facing rule stands: warm the plan outside any
region.

### SIMD kernels

Explicit vectorization of the butterfly loop, using AVX2 intrinsics
on x86-64 and NEON on ARM. Blocked on the emitter's ability to emit
platform-specific intrinsics; currently the C compiler must
auto-vectorize, which it does inconsistently.

---

## Version history

**0.2.0** — internal rewrite of `Kernels` to use bitwise operators
now that the parser propagates types onto plain variable reads.
`bit_reverse` drops from `O(k²)` to `O(k)`; `pow2` and `log2_exact`
use shifts in place of multiplication. Public API, `Plan` layout,
`fft_kernel` butterfly, and the plan cache are all unchanged — this
release is behaviour-preserving.

**0.1.0** — initial release. `Kernels` (`pow2`, `log2_exact`,
`bit_reverse`, `fft_kernel`) and `Ops` (`Plan`, `make_plan`,
`ensure_plan`, `forward`, `inverse`, `next_pow2`). Cached plan per
length. Iterative radix-2 Cooley-Tukey. Conjugate-based inverse.

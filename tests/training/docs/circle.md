# Circle Classifier — Training Notes

**Location:** `tests/training/circle.vy`
**Date:** 2026-09-25
**Status:** Converging, 100% train accuracy

---

## Purpose

Exercise the full Vyne stack — lexer, parser, AST, C emitter, linker, runtime, `vlinalg` — against a supervised learning problem. Verify that numeric primitives, matrix operations, and the optimizer loop are coherent enough to train a working classifier end to end.

---

## Problem

Classify 300 uniformly-sampled points in `[-1, 1]²` as inside or outside a circle of radius 0.5.

```
label(x, y) = 1  if x² + y² < 0.25
              0  otherwise
```

Class balance is roughly 20/80 (positive/negative).

---

## Model

```
input   2   (x, y)
hidden1 10  tanh
hidden2 10  tanh
output  1   sigmoid
```

Loss: binary cross-entropy.
Optimizer: full-batch gradient descent.
Update rule: `θ -= (LR / N) * ∇θ`.

---

## Files

| File                          | Role                           |
| ----------------------------- | ------------------------------ |
| `tests/training/circle.vy`    | Training script                |
| `modules/external/vlinalg.vy` | Matrix library (imported)      |
| `modules/external/vcolors.vy` | ANSI color helpers (imported)  |
| `runtime/modules/vmath.h`     | Scalar math primitives         |
| `runtime/vyne_runtime.h`      | Value type, arena, collections |

---

## Configuration

```vyne
N_POINTS    = 300;
EPOCHS      = 3000;
LR          = 0.3;
HIDDEN      = 10;
PRINT_EVERY = 100;
COLS        = 70;
ROWS        = 26;
SPAN        = 1.1;
```

`scale = LR / N_POINTS` is precomputed once and multiplied into each gradient.

---

## Prerequisites — runtime fixes

The classifier depends on the following runtime changes. All were landed before training was attempted; they affect both correctness and throughput.

### `VyneValue` size

The struct carried an `int ref_count` field that was written but never read. Removed. Value size dropped from 24 to 16 bytes. Affects every array element, function argument, and temporary.

**File:** `runtime/vyne_runtime.h`, `struct VyneValue`.

### Arena allocator fast path

`arena_alloc` reloaded `g_arena.head` twice per call and dereferenced through it. Replaced with cached bump-pointer and end-pointer statics. Fast path is now one comparison, one add.

**File:** `runtime/vyne_runtime.h`, `arena_alloc`.

### `arena_try_reclaim`

`vyne_array_push` on grow left the old buffer orphaned in the arena. Added a helper that rolls back the bump pointer if the old buffer is the most recent allocation, making append-heavy loops O(N) arena bytes instead of O(2N).

**File:** `runtime/vyne_runtime.h`, `arena_try_reclaim`, `vyne_array_push`.

### `vyne_string_static`

String literals were re-copied into the arena on every evaluation. Added a variant that borrows the `.rodata` pointer. The emitter now emits `vyne_string_static` for literals.

**Files:** `runtime/vyne_runtime.h`, `compiler/codegen/codegen.cpp` (`StringNode::getCExpr`).

### Character interning

`s[i]` allocated a fresh 2-byte string on every access. Added a 256-entry pool of pre-built one-byte strings.

**File:** `runtime/vyne_runtime.h`, `vyne_char_at`.

### Range pre-sizing

`vyne_range_create` grew the result array incrementally. Now sizes once from `(end - start + 1)`.

**File:** `runtime/vyne_runtime.h`.

### Map rewrite

Linear-scan map replaced with open-addressing hash table using FNV-1a and linear probing. Tombstones for deletion. Affects every `map.get`, `map.set`, `map.has`, `map.delete`.

**File:** `runtime/vyne_runtime.h`, `_vyne_map_find`, `vyne_map_set`, etc.

### Slice semantics

`SliceNode::evaluate` (interpreter) and `vyne_slice_get` (transpiler) used inclusive-high semantics but the demo file's comments assumed Python-style exclusive-high. Rewrote both to exclusive-high. The two now agree.

**Files:** `compiler/ast/ast.cpp`, `runtime/vyne_runtime.h`.

### `vmath.random_float`

The existing `vmath.random` casts both arguments to `int64_t`, so it can only return integers. Added a continuous variant that returns `lo + r * (hi - lo)` for `r ∈ [0, 1)`.

**Files:** `runtime/modules/vmath.h`, `compiler/codegen/native_maps.h`, `tests/training/vlinalg.vy`.

---

## Bugs encountered during training

Four distinct failure modes. Each was diagnosed from the loss curve or the render output.

### B1 — Dataset was a 3×3 grid

**Symptom:** Loss stalled at 0.25. Render showed a fixed checkerboard pattern.

**Cause:** `vmath.random(-1.0, 1.0)` returns one of `{-1, 0, 1}` because both arguments are cast to `int64_t`. Across 300 samples, the "dataset" contained 9 distinct points. The classifier was fitting a one-hot detector over that grid.

**Fix:** Wrap with `rand_uniform(lo, hi)` in the training script. Later promoted to `vmath.random_float` at the runtime level.

### B2 — MSE + sigmoid vanishing gradient

**Symptom:** Loss dipped briefly then diverged.

```
  100  0.727  0.257
  600  0.228  0.723    ← found signal
 1300  0.741  0.253    ← diverged
 3000  0.773  0.227
```

**Cause:** `∂L/∂z = (p − y) · σ′(z)`. With MSE + sigmoid, the sigmoid derivative attenuates the gradient as outputs saturate. Effective step size becomes large relative to gradient signal.

**Fix:** Switch to cross-entropy. The sigmoid derivative cancels, and `∂L/∂z = p − y`.

### B3 — Bias updates had flipped sign

**Symptom:** Loss stuck at exactly the base-rate cross-entropy.

```
−p·log(p) − (1−p)·log(1−p)   for   p = 59/300 = 0.197
= 0.15803
```

Accuracy pinned at the majority-class rate (0.80). Render showed no cell above the `.1`–`.2` output bucket.

**Cause:** The weight updates used `W -= scale * dW`. The bias updates used `b += scale * db`. Gradient ascent on the bias parameters while the weights did descent. Since the output bias has direct control over the base rate, the optimizer saturated `b3` immediately and the hidden-layer gradient vanished.

**Fix:** Three characters. `b1[c] = b1[c] + scale * db1[c]` → `b1[c] = b1[c] - scale * db1[c]`, and same for `b2`, `b3`.

### B4 — Xavier init produced binary weights

**Symptom:** After B2 and B3 were fixed, loss still pinned at the base rate for the current class balance.

```
  100  0.784  0.180
 3000  0.746  0.240
```

**Cause:** `vlinalg.random_uniform` calls `vmath.random(0.0, 1.0)`, which was still integer-only. Every weight was one of two values: `-limit` or `+limit`. Layer-1 hidden units collapsed to four fixed patterns of the input.

**Fix:** Two stages.

1. Script-level workaround: a continuous Xavier initializer using `vmath.random(0, 1000000) / 1e6`.
2. Permanent fix: added `vmath.random_float` at the C level, rewrote `vlinalg.random_uniform` to use it, and reverted `xavier_init` to a two-line function.

---

## Current behavior

Two independent runs with different random data and different class balance. Both converge.

**Run A** — 59/241 split:

| Epoch | Loss  | Accuracy |
| ----- | ----- | -------- |
| 100   | 0.490 | 0.803    |
| 300   | 0.351 | 0.803    |
| 500   | 0.143 | 0.967    |
| 700   | 0.070 | 1.000    |
| 3000  | 0.010 | 1.000    |

**Run B** — 70/230 split:

| Epoch | Loss  | Accuracy |
| ----- | ----- | -------- |
| 100   | 0.538 | 0.767    |
| 300   | 0.220 | 0.963    |
| 500   | 0.064 | 0.997    |
| 1300  | 0.017 | 1.000    |
| 3000  | 0.005 | 1.000    |

Render output for Run B:

```
               o        o            o  o  o o      o      o o
   o       o                     o           o   o          oo
    o            o    o  o  o   o  o    o        o             o
     o o oo  o     o    o    o o  o  o   o           o    o
       oo             o     o  o               oo         o    oo
              o o         oo  .x...           o           o
    oo         o     o    .-x#%%%%%%%#+-.     ooo
          o o     o     .=x%%%%%%%%%x%%%%%*=.    o  o      oo oo
      o o     o    oo .=#%xx%%%%%%%%%%%x%%xxx#-.o    o o o      o
   oo     o  o     o :*%%%%%%x%%%%%%x%%%%xx%%%%#-   oo    oo
   o         o o   .x#%x%%x%%%%%%%x%%%%x%x%%%%%x#=   o     o  o
      o   o        -*%%x%%%%x%%%x%xxxx%x%x%x%%%%%*.oo o  o  o
   o         oo   o:*x%xx%%%x%%%%%%x%%%x%%%%%%xo.      o       o
        o   o   o   :*xxx%%%%%%%xxx%x%%%%%%%%%%x%=   o ooo     o
             o     o  =%%%%x%%%xx%%x%xx%x%%x%%x%+oo  o  oo      o
          o o   o      o+%%%%%%%%x%x%x%%%%x%%%#=      oo
       o  oo         o   oo#%%%x%%%%%%%%x%%%#-.       ooo
               o  o      o   .o+xoo%###+=o.   o   o     o
       o o o    o     oo  o        o     oo    oo           o  o
       o     oo      o o o  o        o o              o  o  o   o
              o         o          o o    o         oo   o      o
     o o             o        o o  o        ooooo      o       o
      oooo    o   o    o o      o  o               o  o
```

`@`/`%` = high-confidence class-0 (outside). `.`/` ` = low-confidence class-1 (inside). `x` = training sample, class 1. `o` = training sample, class 0. The `x` samples sit inside the low-output region; the `o` samples sit in the high-output region.

---

## Performance

| Stage                   | Time                                     |
| ----------------------- | ---------------------------------------- |
| Transpile               | ~7 ms                                    |
| `gcc -O3`               | ~3.4 s                                   |
| Execution (3000 epochs) | ~5.0 s ( dropped from ~5.5s, changelog ) |
| Total                   | ~9 s                                     |

Execution time is dominated by the interpreter loop over `VyneValue` arrays in `vlinalg.multiply`, `hadamard`, and `transpose`. Approximately 100,000 multiply-adds per epoch.

---

## Known issues

### ~~`relu_prime` compiles to `vyne_null()`~~ — resolved

**Status:** Fixed. Verified 2026-09-26 with
`tests/transpiler/relu_prime_test.vy`. Both the `loop + push` form and
the `collect { if ... }` form produce correct output:

    [0.0, 0.0, 1.0, 1.0]
    [0.0, 0.0, 1.0, 1.0]

The `collect` block now correctly captures the value of an `if`
expression. The workaround in `vlinalg/Activations.vy` (using `loop`
with explicit `push`) can be reverted to the shorter `collect` form if
you prefer the brevity, or left as-is — both compile to the same code.

The historical note about the bug is preserved below for reference.

```vyne
through v :: row_data -> collect {
    if v > 0.0 { 1.0 } else { 0.0 }
}
```

The generated C emits both branches as statements, then unconditionally pushes `vyne_null()`. The `collect` form does not capture the value of an `if` expression in the current codegen.

**Impact:** `relu` activation cannot be used in training until this is fixed. `tanh` and `sigmoid` are unaffected.

**File:** `compiler/codegen/codegen.cpp`, `ForNode::getCExpr` and `getCExpr` on `IfNode`.

### `vmath.random` still integer-only

`vmath_random` remains for backwards compatibility. Any caller passing floats gets silent truncation. `vmath.random_float` exists but must be called explicitly.

**File:** `runtime/modules/vmath.h`.

### Generalization unverified

All numbers above are **training** accuracy. With 300 samples and 100 hidden units, the model has capacity to memorize. The smooth, roughly-circular render and the similarity of the two runs suggest it learned the constraint rather than the samples, but this has not been measured against a held-out set.

---

## Reproducing

```bash
vynec.exe --compile tests/training/circle.vy
```

The output binary is written alongside the source.

---

## Further work

Ordered by expected payoff:

1. **Held-out test set.** 100 points generated with the same rule, not trained on. Report test accuracy at end of training. Ten-minute change to `circle.vy`.

2. **Ring dataset.** Class 1 only when `0.3² < x² + y² < 0.5²`. Exceeds the 10-unit hidden layer's capacity. Expected to plateau around 0.85 accuracy with visible leakage in the render. Bumping `HIDDEN` to 32 should resolve.

3. **Fix `relu_prime` codegen.** Unlocks ReLU, which changes the gradient dynamics qualitatively on deeper networks.

4. **Numeric core rewrite.** Emit specialized C for matrix multiply, add, subtract, and element-wise ops. Bypasses `vyne_binop`'s type dispatch and `vyne_index_get`'s bounds checking. Expected 5–10× on the training loop.

5. **Momentum or Adam.** Beyond the scope of the current problem, but the next optimizer change.

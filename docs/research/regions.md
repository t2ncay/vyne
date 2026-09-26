# Lexical Regions and Shape-Typed Scratch:

## Scoped Arena Memory and Static Shape Checking in the Vyne Language

**Technical Report — Vyne Transpiler**
_Draft, 09.26.2026_

---

## Abstract

We present _lexical regions_ and _shape-typed scratch_, two language
constructs added to the Vyne programming language to address a class of
memory and typing problems that arise in numerical and machine-learning
workloads. Regions give the programmer a lexically scoped arena
checkpoint, so that all arena allocations performed inside a region are
released at its closing brace. Scratch declares a fixed-shape,
stack-resident numeric array whose dimensions are known at compile time,
indexable with no runtime allocation, no boxing, and no pointer chasing.
We describe the design, the lowering to C, the interaction between the
two constructs, and a small case study in which an existing training
loop in a neural-network classifier was rewritten to use scratch
buffers. We report that the rewritten loop eliminates all arena traffic
for the two accumulator buffers, and we identify three follow-up
capabilities (shaped assignment, shaped parameters, escape analysis)
that are required before the technique generalizes to the rest of the
loop.

---

## 1. Motivation

Vyne is a small language that targets a bump-allocated arena for all
heap memory. Every `VyneValue` — arrays, maps, strings, structs —
lives in the arena; there is no `free` and no per-object reference
counting. The arena is released at program exit by `arena_free_all`.

This model has three attractive properties. Allocation is a pointer
bump, so individual allocations are cheap. There is no fragmentation,
because the arena never hands back individual blocks. And there are no
lifetimes to reason about in the object graph: everything stays alive
until the arena dies.

It has one equally sharp disadvantage. In a long-running loop, every
iteration that allocates leaks until the program exits. A training loop
that materializes a fresh `Array` of activations on each of 10 000
epochs allocates 10 000 copies, all of which remain live. Peak resident
set grows linearly with iteration count, regardless of how many of
those allocations the program actually still reads.

Two constructs, described here, address the problem from opposite
directions:

- **Lexical regions** let the programmer say _"everything allocated
  inside this block is scratch, and may be reclaimed when the block
  ends."_
- **Shape-typed scratch** lets the compiler prove that certain values do
  not need the arena at all — they fit on the C stack, and their size is
  known statically.

Together they turn the arena from an unbounded accumulator into a
bounded scratchpad whose lifecycle is tied to lexical structure.

---

## 2. The Region Construct

A region is a lexical block introduced by the keyword `region`,
followed by a name and a brace-delimited body:

```vyne
through epoch :: 1..EPOCHS -> loop {
    region training {
        A = forward(X, W1, b1);
        loss = cross_entropy(A, Y);
        ...
    };
};
```

The construct is drawn from region-based memory management as
introduced by Tofte and Talpin \cite{tofte-talpin-1997}. Where our
design departs from the classical formulation is that the region does
not denote a _memory pool_ in the type system, but a _checkpoint_ in a
single global arena.

### 2.1 Lowering

At compile time, a region lowers to a checkpoint/rewind pair emitted
around the region body:

```c
VyneValue vmem_cp_1529 = vmem_runtime_checkpoint();
{
    // region body
}
vmem_runtime_rewind(vmem_cp_1529);
```

`vmem_runtime_checkpoint()` records the current bump pointer and the
current head block of the arena. `vmem_runtime_rewind(h)` frees every
arena block allocated after the checkpoint and resets the bump pointer
to the recorded offset. The implementation lives in `runtime/modules/
vmem.h` and relies only on the arena's existing block-chain structure;
no new allocator is introduced.

### 2.2 Lifetime and semantics

A region's lifetime is the lifetime of its enclosing C block. Values
allocated inside are valid from their allocation to the closing brace.
Any reference to a region-allocated value after the closing brace is a
use-after-rewind and is a programmer error, not a runtime error — the
compiler does not currently insert escape checks.

Control flow through a region is treated conservatively. A `break`,
`continue`, or `return` that exits a region emits a rewind on the way
out. For `return`, the returned value is first materialized in a C
local on the _outside_ of the rewind, so that a primitive return value
survives:

```c
VyneValue __ret_val_17 = <expr>;
vmem_runtime_rewind(vmem_cp_1529);
return __ret_val_17;
```

Non-primitive returns — an array, a struct, a string — cannot safely
survive a rewind, because their storage is arena-resident and would be
freed. The compiler refuses to lower such a return silently. The user
must commit the value first (Section 3).

### 2.3 Region commit

`region.commit(x)` deep-clones `x` into a _secondary_ arena, called the
commit arena, which is not touched by region rewinds. The cloned value
is then safe to reference after the region ends. Its lifetime is the
lifetime of the program, matching the lifetime of the primary arena.

```vyne
region training {
    A3 = forward(X, ...);
    if epoch == EPOCHS {
        region.commit(A3);
    }
};
// A3 is still valid here.
```

The commit arena is a deliberate simplification: committed values are
never reclaimed, so a `region.commit` inside a hot loop grows the
commit arena linearly with iteration count. A real implementation would
recover escape-analysis or reference-counting information to prove
earlier reclamation. We discuss this in Section 6.

---

## 3. Shape-Typed Scratch

Scratch declares a fixed-shape numeric array whose storage is on the C
stack and whose dimensions are known at compile time:

```vyne
region training {
    scratch db2_buf :: Float64[12];
    scratch db1_buf :: Float64[16];
    ...
};
```

`scratch` is legal only inside a region. The declaration is lowered to
a C array declaration, and the region's closing brace is what ends the
array's scope:

```c
double v_db2_buf[12];
double v_db1_buf[16];
```

### 3.1 Typing

A scratch variable's static type is a `CType` carrying three pieces of
information: the element kind (`Int64` or `Float64`), a vector of
dimensions, and a C declaration name. The type system treats scratch
values as _unboxed_ — they are never wrapped in a `VyneValue`, and they
never cross a dynamic boundary unboxed. Every use site is either inside
the region or is rejected at compile time.

### 3.2 Indexing

Scratch arrays are indexed with the same syntax as boxed arrays, `x[i]`
for rank-1 and `x[i, j]` for rank-2. Both forms lower to native C
indexing:

```c
// source:  db2_buf[c] = db2_buf[c] + delta2.data[r * HIDDEN2 + c];
// emitted: v_db2_buf[v_c] = v_db2_buf[v_c] + (coerced delta2 element);
```

The flattening from multi-index to a single linear index uses row-major
strides computed at compile time from the declared shape. Rank checking
is static: an index expression with the wrong number of components is a
compile error.

### 3.3 What scratch is not

We are explicit about the boundaries, because the boundaries are where
the feature earns its keep:

- **Scratch values do not cross function boundaries.** There is no
  shaped-parameter ABI; passing a scratch variable to a Vyne function
  boxes it and defeats the purpose.
- **Scratch values do not flow into native library calls.** A call to
  `vlinalg.multiply(x, y)` where `x` is scratch type-checks by boxing
  `x` first.
- **Scratch values do not survive a rewind.** `region.commit` currently
  accepts only boxed values. A shaped value that needs to escape is a
  compile error, not a runtime dangling pointer.

The rule we recommend is: **scratch is for local, purely-numeric
accumulation.** Reads and writes of scalars, nothing else.

---

## 4. Implementation Notes

Three changes to the compiler were required to add the two constructs.

### 4.1 Lexer

One new keyword, `scratch`, mapped to a new token kind
`VTokenType::Scratch`. `region` already existed.

### 4.2 Parser

`region` and `scratch` are parsed by two new productions:

- `parseRegionStatement` disambiguates `region name { ... }` from
  `region.commit(expr);` by peeking at the token after `region`.
- `parseScratchDeclaration` parses `scratch name :: Type[d1, d2, ...];`.
  Dimensions are currently restricted to integer literals; accepting
  named constants requires a constant-folding pass in the parser and is
  left as future work.

Multi-index syntax `x[i, j]` is parsed in `parseIdentifierExpr`'s
postfix loop and produces a `ScratchIndexNode`. Single-index syntax
`x[i]` continues to produce an ordinary `IndexAccessNode`; the codegen
for that node checks the base's static type for a shape and, if
present, lowers to native indexing (Section 4.4).

### 4.3 Static types

`CType` gains a `shape` field — a vector of `int64_t` — and three
helpers: `hasShape()`, `numElements()`, and `strides()`. Existing uses
of `CType` are unaffected because the field defaults to empty.

### 4.4 Code generation

`ScratchNode::compile` emits a C array declaration and registers the
variable with the emitter's per-scope type table, so that later uses
resolve the base expression's `CType` and discover the shape. The
region's own `emitBlockOpen` / `emitBlockClose` push and pop a C scope,
which is what makes the scratch array's lifetime lexical.

`IndexAccessNode::getCExpr` and `IndexAssignmentNode::compile` check for
a shaped base type before falling through to the boxed path. If the
base is shaped, they emit direct C indexing. If the base is a
`VyneArray_f64` or `VyneArray_i64`, they continue to emit the typed-
array member access that has always been used for those types.

`ScratchIndexNode` and `ScratchStoreNode` handle the multi-index case
by computing a flat index from the shape's strides. The rank check is
performed here.

---

## 5. Case Study: A Neural-Network Training Loop

We evaluated the two constructs on an existing Vyne program: an RNA
sequence classifier that distinguishes codon-structured RNA from
uniform-random RNA using 64-dimensional codon-usage features. The
classifier is a two-hidden-layer MLP trained by SGD. Its hot loop runs
`EPOCHS` times; inside each iteration, `forward`, `backprop`, and the
bias updates run over `N_SAMPLES = 240` training examples.

Before the rewrite, two intermediate buffers — `db1_buf` (16 elements)
and `db2_buf` (12 elements) — were declared as boxed `Array`s and
allocated fresh on the arena at each iteration.

### 5.1 Rewrite

The buffers were redeclared as scratch inside a region that wraps the
loop body:

```vyne
through epoch :: 1..EPOCHS -> loop {
    region training {
        scratch db2_buf :: Float64[12];
        scratch db1_buf :: Float64[16];

        ... forward pass, backprop ...

        through c :: 0..HIDDEN2-1 -> loop {
            db2_buf[c] = 0.0;
            through r :: 0..N_SAMPLES-1 -> loop {
                db2_buf[c] = db2_buf[c] + delta2.data[r * HIDDEN2 + c];
            };
        };

        through c :: 0..HIDDEN1-1 -> loop {
            db1_buf[c] = 0.0;
            through r :: 0..N_SAMPLES-1 -> loop {
                db1_buf[c] = db1_buf[c] + delta1.data[r * HIDDEN1 + c];
            };
        };

        through c :: 0..HIDDEN1-1 -> loop {
            b1[c] = b1[c] - scale * db1_buf[c];
        };
        through c :: 0..HIDDEN2-1 -> loop {
            b2[c] = b2[c] - scale * db2_buf[c];
        };
    };
};
```

### 5.2 Emitted code

The C output for the region head and the accumulator loops contains:

```c
VyneValue vmem_cp_1529 = vmem_runtime_checkpoint();
{
    double v_db2_buf[12];
    double v_db1_buf[16];

    ...

    v_db2_buf[v_c] = 0;
    for (int64_t r = 0; r <= 239; ++r) {
        double idx = v_db2_buf[v_c];
        ...
        v_db2_buf[v_c] = ...;
    }

    ...
}
vmem_runtime_rewind(vmem_cp_1529);
```

No `arena_alloc` call appears for either buffer. The declarations are
stack-resident; the index expressions are direct array accesses. The
checkpoint and rewind bound the array lifetimes to the region body.

### 5.3 What the rewrite eliminates

For the two accumulator buffers, the loop no longer performs:

- A per-iteration `Array` allocation on the arena.
- A per-iteration vector growth and its associated reclaim attempts.
- A per-element boxing on the write path, and a per-element unbox on
  the read path, for the buffer itself.

The two accumulator loops therefore run against native `double` slots
with no allocation on either the read or the write side.

### 5.4 What the rewrite does not eliminate

The source side of each accumulation — `delta2.data[r * HIDDEN2 + c]` —
still reads through a `VyneValue`-based tensor, because vlinalg
currently returns boxed `VyneArray_f64`. Each inner-iteration read
therefore still performs a runtime tag check and a union member access.
Removing that cost requires shape-aware overloads on the linear-algebra
library and is outside the scope of the current report.

The `forward` and `backprop` passes themselves are unchanged: they
still produce and consume boxed tensors, and every intermediate value
they materialize is still allocated on the arena and released only at
the region rewind.

---

## 6. Limitations and Future Work

Three capabilities are required before the technique generalizes to the
rest of a numerical kernel.

**Shaped assignment from boxed sources.** The assignment
`scratch dW1 :: Float64[64, 16]; dW1 = vlinalg.multiply(...);` does not
type-check. Adding it requires an element-copy loop emitted at the
assignment site, plus a runtime shape check against the declared
dimensions. This is a small codegen change and is the most tractable of
the three.

**Shape-aware function parameters.** Passing a scratch array to a Vyne
function currently boxes it. A shape-aware ABI — `(const double*,
int64_t, int64_t)` for a rank-2 `Float64` — would require the emitter to
maintain a second calling convention alongside the existing
`(int, VyneValue*)` form. This is a moderate change.

**Escape analysis.** The current design does not prove that a scratch
value has not escaped before rewind. `region.commit` therefore accepts
only boxed values, and any attempt to escape a shaped value is a
compile error. A proper escape analysis would let scratch values be
committed and would let the compiler reclaim committed values earlier
than program exit.

Additionally, `region.commit` currently deep-clones its argument into a
commit arena that is never freed. In a hot loop, this grows the commit
arena linearly with iteration count. The fix is the same escape
analysis: once the compiler can prove that a committed value is dead at
a known point, it can reclaim the corresponding arena region.

---

## 7. Related Work

Region-based memory management as a language discipline is due to
Tofte and Talpin \cite{tofte-talpin-1997}, developed further by
\cite{grossman-2002, hallenberg-2002}. Our implementation differs in
two respects. First, we use a single global bump arena, not a family of
typed regions; a region is a checkpoint into that arena, not a distinct
allocation pool. Second, we do not integrate the region discipline into
the type system, so escape safety is a programmer responsibility rather
than a typing property. These simplifications trade expressiveness for
a much smaller implementation footprint and a smaller runtime.

Stack-allocated fixed-size arrays are, of course, the default storage
class in C, C++, and Rust. The contribution of scratch is not the
storage class itself, but the language-level syntax for declaring one
and the static shape information that flows through the type system to
make indexing and rank checks free.

The specific problem of eliminating per-iteration allocation in
numerical loops is addressed at much larger scale by frameworks such as
JAX and PyTorch, which use tracing and just-in-time compilation to
fuse and hoist allocations. Our approach is complementary: it provides
a small language-level primitive that these frameworks do not need, but
that a language without a JIT can use directly.

---

## 8. Availability

The implementation is part of the Vyne compiler at [repo]. The
scratch feature is behind the `scratch` keyword; no flag is required.
The RNA classifier case study is at `tests/training/ml_seq.vy`.

---

## References

- Tofte, M. and Talpin, J.-P. _Region-Based Memory Management._
  Information and Computation, 132(2), 1997.
- Grossman, D. et al. _Region-Based Memory Management in Cyclone._
  PLDI 2002.
- Hallenberg, N., Elsman, M., Tofte, M. _Combining Region Inference
  and Region-Based Memory Management._ TOPLAS 24(4), 2002.

```

---
```

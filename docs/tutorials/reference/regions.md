# Memory and regions

**Since:** v0.0.1-alpha (documentation target; release tag to confirm)

## Purpose
A region bounds the lifetime of arena-allocated values created while the region is active.

## Syntax
```text
region-statement ::= "region" identifier "{" {statement} "}" ";"
commit-statement ::= "region" "." "commit" "(" expression ")" ";"
```

## Semantics
The compiled backend saves an arena checkpoint when it enters a region and rewinds when it leaves. A committed value is copied into a second arena that survives the rewind. Commit storage currently persists until global arena cleanup, so commits in an unbounded loop can grow memory. `vmem.total_allocated()` reports arena accounting; do not interpret it as exact process memory usage.

## Example
```vyne
region temporary {
    out("inside region");
};
```

> **Caution:** A value referring to memory allocated inside a region may dangle after rewind. Do not assume an alias escapes safely; commit the value when it must survive.

## Policies
A region may carry one policy attribute: `@name<args> region name { ... };`.

| Policy | Meaning |
| --- | --- |
| *(none)* / `@bump` | Default checkpoint/rewind bump region. |
| `@pool<T, N>` | Fixed-slot pool: `pool_alloc()` / `pool_free(x)`. `T` is `Float64` or `Int64`. |
| `@slab<T, MAX[, CAP]>` | Power-of-two size-class allocator: `slab_alloc(n)` / `slab_free(x)`. Each class's backing is allocated lazily and reclaimed by the region rewind. `CAP` defaults to 8. |
| `@ring<T, N, MAX>` | Rotating fixed slots: `ring_alloc(n)` returns the next of `N` slots (each `MAX` elements wide), overwriting the oldest after `N` allocations. There is no free intrinsic. |
| `@speculative` | Runs the body, then commits or rewinds based on `region.commit_if(pred)`. |
| `@bounded<N>` | Runtime-checked byte ceiling for the region. |
| `@zeroed` | Every allocation in the region is zero-filled. |
| `@deterministic` | `@zeroed` plus a non-determinism scan (rejects time/RNG sources). |

Policies are compiled-backend features. `slab_alloc(n)` and `ring_alloc(n)`
zero-fill the requested `n` elements; their buffers are fixed-size and must not
be grown with `push`. A `@ring` value must not be retained longer than `N`
subsequent `ring_alloc` calls — the bounded-history contract the policy exists
to express. The same policy applies for the whole region; a second policy means
a second region.

## Edge cases
The parser requires `;` after the closing region brace. Regions are a compiled-code feature; the interpreter's behavior must be verified before promising support. Returns, breaks and continues use special cleanup paths.

### See also
[vmem](../modules/vmem.md) · [Control flow](control-flow.md) · [Errors](errors.md)

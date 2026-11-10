---
id: flow
title: Control Flow
group: Language Reference
icon: fa-code-branch
order: 4
description: Conditionals, while loops, through modes, break/continue, and loop invariants.
---

# Control Flow

## Conditionals

```vyne
if x > 0 {
    out("positive");
} else if x < 0 {
    out("negative");
} else {
    out("zero");
};
```

Parentheses around the condition are optional. A block always uses braces; there is no single-statement form.

## While loops

```vyne
i :: Int64 := 0;
while i < 10 {
    out(i);
    i = i + 1;
};
```

`while (condition) { ... }` is also accepted. The condition is evaluated before each iteration; any truthy value continues the loop.

## `through` loops

`through` is the canonical iteration construct. It takes an optional iterator name, an iterable, an optional arrow, and one of five modes.

```vyne
through i :: 0..9 -> loop {
    out(i);
};

squares := through i :: 1..10 -> collect { i * i };
evens   := through i :: 0..100 -> filter { i % 2 == 0 };
allPos  := through i :: values -> every { i > 0 };
uniq    := through i :: values -> unique { i };
```

| Mode | Result | Use |
| :--- | :--- | :--- |
| `loop` | none | side effects only |
| `collect` | array | map each element to a new value |
| `filter` | array | keep elements for which the body is true |
| `every` | `Bool` | true when the body is true for all elements |
| `unique` | array | keep the first occurrence of each value |

A range (`0..9`) is inclusive on both ends. Iterating an `Array` or `Map` walks its elements; map order is unspecified.

## SIMD-friendly loops

`through` loops are eligible for SIMD lowering when the body is a simple typed-array kernel. The emitter adds `__restrict__` pointer annotations, hoists invariant field loads, and checks a relaxed shape-safety condition. You do not write intrinsics; use typed arrays and straightforward indexing:

```vyne
fn scale_in_place(xs :: Array<Float64>, k :: Float64) {
    n :: Int64 := xs.size();
    through i :: 0..n-1 -> loop {
        xs[i] = xs[i] * k;
    };
}
```

The vectorizer is a performance path, not a semantic change. Keep aliasing out of the loop body if you want the compiler to take it.

## Break and continue

```vyne
through i :: 0..100 -> loop {
    if i == 3 { continue; }
    if i > 8 { break; }
    out(i);
};
```

`break` leaves the innermost loop; `continue` advances to the next iteration. Inside `collect` / `filter`, `continue` skips the current result and `break` terminates the derivation early.

> **Loop + region caveat.** A `break` or `continue` from a loop nested inside a region triggers region-unwind emission for every open region. Keep loops outside regions when the loop can exit early, or use a `while` loop with explicit state, until the unwind path is tightened.

## Loop invariants

`@invariant(expr)` prefixes a `while` or `through ... -> loop` body. The expression is checked after every normal iteration, following the "invariant holds after each step" form.

```vyne
total :: Int64 := 0;

@invariant(total >= 0)
through i :: 0..9 -> loop {
    total = total + i;
};
```

The same `ruleset { contracts = ... }` mode that controls `requires` / `ensures` controls invariants:

```vyne
ruleset { contracts = "check" };   # assert
ruleset { contracts = "evidence" }; # assert + CSV evidence log
ruleset { contracts = "off" };     # emit nothing
```

A violation halts with `VNE-111`. `continue` currently jumps to the next iteration before the check runs; restructure the body if that path matters.

## Nesting and early exits

```vyne
through row :: 0..rows-1 -> loop {
    through col :: 0..cols-1 -> loop {
        if grid[row, col] == target {
            found = true;
            break;
        };
    };
    if found { break; };
};
```

There are no labeled loops. To exit multiple levels, carry a flag or move the search into a function and `return`.

## See also

- [Literals & Expressions](page:expr)
- [Functions](page:fn)
- [Memory & Regions](page:regions)
- [Error Handling](page:errors)

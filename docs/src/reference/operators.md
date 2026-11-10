---
id: operators
title: Operators
group: Reference
icon: fa-arrow-right-arrow-left
order: 3
description: Operator families, precedence, associativity, and special assignment forms.
---

# Operators

## Precedence

Level 1 binds loosest; level 17 binds tightest. Parenthesize mixed expressions when the intent is not obvious; the table reflects the current recursive-descent parser, not a promise that precedence will never change.

| Level | Operators | Assoc. | Meaning |
| ---: | :--- | :--- | :--- |
| 1 | `?:` | right | ternary conditional |
| 2 | `??` | left | null coalesce |
| 3 | `\|>` | left | pipeline |
| 4 | `..` | left | inclusive range |
| 5 | `\|\|` | left | logical or (short-circuit) |
| 6 | `&&` | left | logical and (short-circuit) |
| 7 | `\|` | left | bitwise or |
| 8 | `^` | left | bitwise xor |
| 9 | `&` | left | bitwise and |
| 10 | `== !=` | left | equality (deep for composites) |
| 11 | `in` | left | membership |
| 12 | `< <= > >=` | left | comparison |
| 13 | `<< >>` | left | shifts |
| 14 | `+ - // %` | left | additive, floor divide, modulo |
| 15 | `* / **` | left | multiplicative and power |
| 16 | `! - $ ~` | prefix | logical not, negation, address-of, bitwise not |
| 17 | `++ --` | postfix | increment / decrement |

`:=` and `=` are statement forms, not expressions. `??=` is a statement-form conditional assignment.

## Arithmetic

```vyne
a := 2 + 3 * 4;       # 14
b := 7 // 2;          # 3, floor division
c := 7 % 2;           # 1
d := 2 ** 10;         # 1024.0 (always Float64)
```

`Int64` arithmetic wraps. Division and modulo by a literal zero are compile-time errors; a runtime zero divisor is a runtime error. `**` always produces `Float64`.

## Comparison and equality

```vyne
a == b;
a != b;
a < b;
a <= b;
a > b;
a >= b;
```

`==` is deep for arrays, maps, and structs.

## Logical

```vyne
ready && loaded;
failed || retry;
!done;
```

`&&` and `||` short-circuit. Any truthy value satisfies `||` and continues `&&`.

## Bitwise

```vyne
flags := 0;
flags = flags | 0x80;
flags = flags & 0xFF;
flags = flags ^ 0x0F;
flags = ~flags;
flags = flags << 1;
flags = flags >> 2;
```

Bitwise operators are supported on both the desktop and AVR backends. `~` is a prefix operator.

## Membership and range

```vyne
if 3 in [1, 2, 3] {
    out("present");
}

through i :: 0..9 -> loop {
    out(i);
};
```

Ranges are inclusive. Membership semantics depend on the collection type.

## Null handling

```vyne
name := maybe_name ?? "anonymous";
cfg.mode ??= "default";
```

`??` evaluates the right side only when the left is `null`. `??=` assigns only when the current value is `null`.

## Pipeline and ternary

```vyne
value := 5 |> double |> double;
text := ready ? "go" : "wait";
```

The pipeline inserts the left value as the first argument. `10 |> add(3)` is `add(10, 3)`; `4 |> square()` is `square(4)`.

## Reference operators

```vyne
fn bump(v :: Int64^^) {
    v = v + 1;
}

addr := $counter;
```

`^^` marks a reference parameter. `$` returns an address as `Int64`; it is interpreter-only, and the compiled backend rejects address-of.

## Increment and decrement

`++` and `--` are postfix operators on numeric lvalues. Prefer `x = x + 1` in library code for readability; the postfix form is available where a compact update reads better.

## Assignment forms

| Form | Meaning |
| :--- | :--- |
| `x := expr` | Declare a new binding |
| `x = expr` | Reassign an existing binding |
| `x ??= expr` | Assign only if `x` is `null` |
| `a[i] = expr` | Index assignment |
| `a.field = expr` | Member assignment |

## See also

- [Literals & Expressions](page:expr)
- [Variables & Assignment](page:vars)
- [Control Flow](page:flow)

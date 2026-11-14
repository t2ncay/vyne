---
id: expr
title: Literals & Expressions
group: Language Reference
icon: fa-superscript
order: 2
description: Numeric, string, array, map, and interpolated literals plus every expression form.
---

# Literals & Expressions

## Numeric literals

```vyne
42             # Int64
-7             # unary minus on Int64
0x7FFF_FFFF    # hexadecimal Int64
1_000_000      # underscore digit separators
3.14           # Float64
1.0e-3         # Float64, exponent form
2.0f           # Float32
inf            # Float64 infinity
nan            # Float64 not-a-number
```

`Int64` literals must fit in a signed 64-bit value (VNE-007). `**` always produces a `Float64`; integer exponentiation would otherwise overflow silently.

## Strings and interpolation

Strings are double-quoted and UTF-8. A `{ ... }` segment inside a string interpolates an expression:

```vyne
name :: String := "Vyne";
x :: Int64 := 12;

out("hello, {name}");
out("{x} squared is {x * x}");
out("nested call: {string(vmath.sqrt(9.0))}");
```

Backslash escapes are recognized inside literals. Interpolation is the readable alternative to repeated `+` and `string(...)` calls.

## Arrays and maps

```vyne
numbers :: Array<Float64> := [1.0, 2.0, 3.0];
mixed   :: Array := [1, "two", 3.0];
empty   :: Array<Float64> := [];
cfg     :: Map := {"mode": "fast", "retries": 3};
```

A typed array requires every element to be compatible with the declared element type. A bare `Array` is boxed and accepts mixed values.

## Ranges and slices

```vyne
0..9          # inclusive range
xs[0]         # index
xs[1:3]       # slice
```

Ranges are the input to `through`. The inclusive upper bound is part of the range value, so `0..9` has ten elements. `sequence(lo, hi)` is the built-in that generates values from `lo` up to but excluding `hi`.

Array slice bounds are half-open: `xs[1:3]` selects elements 1 and 2. An ordinary
array slice copies its elements. When the expected type is `Slice<T>` (a typed
declaration or function parameter), the compiler instead creates a zero-copy
borrow over a typed numeric array. See [Slice views](page:types).

## Calls and member access

```vyne
add(2, 3);
vmath.sqrt(9.0);
circle.area();
matrix.data[i];
cfg["retries"];
```

Method calls resolve through a fixed dispatch chain: native module methods, interface constructors, group methods, built-in array methods, string methods, typed-array methods, and boxed fallbacks.

## Operators

| Family | Operators | Notes |
| :--- | :--- | :--- |
| Arithmetic | `+ - * / % ** //` | `/` is floating division; `//` is floor division |
| Comparison | `== != < <= > >=` | `==` is deep for arrays, maps, and structs |
| Logical | `&& \|\| !` | short-circuit |
| Bitwise | `& \| ^ ~ << >>` | supported on both backends |
| Membership | `in` | collection-dependent |
| Range | `..` | inclusive |
| Null handling | `?? ??=` | fallback and conditional assignment |
| Composition | `\|>`, `?:` | pipeline and ternary |
| Reference | `^^`, `$` | reference parameters and address-of |

```vyne
flags := 0;
flags = flags | 0x80;
flags = flags & ~0x01;
out((flags << 1) >> 2);

value := 5 |> double |> double;
label := ready ? "go" : "wait";
name := maybe_name ?? "anonymous";
```

The pipeline inserts the left value as the first argument: `10 |> add(3)` is `add(10, 3)`, and `4 |> square()` is `square(4)`.

## Assignment is not an expression

`:=` declares and `=` reassigns; neither returns a value. Use a separate statement when you need a result. `??=` is the exception in spirit: it conditionally assigns and is used as a statement.

## Casts and conversions

```vyne
int64("42");
float64(3);
float32(1.0);
string(42);
type(value);
sizeof(value);
```

Conversion failures follow the runtime rules of the target: integer parsing saturates rather than trapping, and arithmetic wraps on `Int64`.

## See also

- [Operators](page:operators) for the full precedence table.
- [Variables & Assignment](page:vars)
- [Collections & Built-ins](page:builtins)

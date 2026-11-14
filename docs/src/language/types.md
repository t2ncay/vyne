---
id: types
title: Values & Types
group: Language Reference
icon: fa-cube
order: 1
description: The value model, primitive types, typed arrays, maps, and Float32 promotion rules.
---

# Values & Types

Vyne is statically analyzable but not statically typed everywhere. A binding with an explicit `:: Type` is checked against that type; a binding without one is boxed and resolved dynamically. `ruleset { type_check = "strict" }` makes explicit types mandatory.

## Primitive types

| Type | Literals | Runtime representation |
| :--- | :--- | :--- |
| `Int64` | `42`, `-7`, `0x7FFF_FFFF` | 64-bit signed integer; hex and `_` digit separators |
| `Float64` | `3.14`, `1.0e3`, `inf`, `nan` | IEEE-754 double |
| `Float32` | `1.0f`, `2f`, `1e3f` | IEEE-754 single precision; default device dtype |
| `String` | `"hello"`, `"x = " + string(x)` | UTF-8, immutable, interned |
| `Bool` | `true`, `false` | logical value |
| `Null` | `null` | absence of a value |
| `Array` | `[1, "two", 3.0]` | boxed heterogeneous sequence |
| `Array<T>` | `Array<Float64>`, `Array<Int64>`, `Array<Float32>` | typed, native-ABI sequence |
| `Slice<T>` | `Slice<Float64>`, `Slice<Int64>` | borrowed pointer-and-length view; no capacity |
| `Map` | `{"key": 42}` | string-keyed association |
| interface name | `Circle(4.0)` | structured value with fields and methods |

```vyne
count  :: Int64   := 12;
ratio  :: Float64 := 0.75;
single :: Float32 := 0.5f;
name   :: String  := "Vyne";
flag   :: Bool    := true;
none             := null;

xs :: Array<Float64> := [1.0, 2.0, 3.0];
boxed :: Array := [1, "two", 3.0];
cfg :: Map := {"mode": "fast", "retries": 3};
```

## Typed arrays

`Array<Float64>`, `Array<Int64>`, and `Array<Float32>` are distinct runtime containers (`VyneArray_f64`, `VyneArray_i64`, `VyneArray_f32`). Boxing one into a generic value is O(1): the runtime wraps the pointer, it does not copy elements. Unboxing is also O(1). This is what makes `A.data[i]` a direct load and what lets numerical kernels stay unboxed at the native boundary.

Typed-array payloads are aligned to `VYNE_TYPED_ARRAY_ALIGN` (32 bytes by default). Override at build time with `-DVYNE_TYPED_ARRAY_ALIGN=64`.

A generic `Array` is boxed. Use it when the element type is genuinely mixed or unknown; use a typed array when the data is numeric.

## Zero-copy `Slice<T>` views

On the compiled desktop backend, a `Slice<T>` borrows a contiguous section of a
typed array. It stores a pointer and element count, with no payload allocation or
copy. The expected type determines the operation: `xs[lo:hi]` in a `Slice<T>`
declaration or argument creates a view; an ordinary array slice still copies.
Bounds are half-open (`lo` inclusive, `hi` exclusive).

```vyne title="slice.vy"
fn sum_view(values :: Slice<Float64>) -> Float64 {
    total :: Float64 := 0.0;
    through i :: 0..values.size()-1 -> loop {
        total = total + values[i];
    };
    return total;
}

data :: Array<Float64> := [10.0, 20.0, 30.0, 40.0];
view :: Slice<Float64> := data[1:3];
out(sum_view(view));        # 50
out(sum_view(data));        # whole-array borrow: 100
out(sum_view(data[0:2]));    # 30; no copied subarray
```

`size()` / `length()` and indexed reads/writes are supported. Writing `view[i]`
updates the owner's storage. A slice is not a growable `Array<T>`: it has no
`push`, `reserve`, or `resize`. It sees changes made through other aliases.
Keep the owner alive and its storage stable for every use of the view. Growing,
dropping, or reclaiming the owner can invalidate the pointer; a view does not
extend a region's lifetime. See [Memory & Regions](page:regions).

The shipped function-parameter types are Float64 and Int64. Float32 view structs
exist in the runtime, but `Slice<Float32>` function parameters are currently
rejected by codegen. This reference
describes compiled desktop views; the interpreter and embedded runtimes do not
provide the same general slice ABI. A runnable regression is
[slice_reserve_test.vy](https://github.com/t2ncay/vyne/blob/master/examples/slice_reserve_test.vy).

## Float32 promotion

`Float32` is a first-class type across the lexer, parser, interpreter, tagged values, typed arrays, native ABI, and codegen.

- `Float32` op `Float32` stays `Float32`.
- Mixing `Float32` with `Float64` or `Int64` promotes to `Float64`.
- `float32(x)` converts explicitly.
- `Array<Float32>` crosses the native ABI as `float*` / `VyneArray_f32`.
- Device handles default to `Float32`; `device_alloc_f64(n)` opts into doubles.

```vyne
a :: Float32 := 1.0f;
b :: Float32 := 2.0f;
c := a * b;              # Float32
d := a * 2.0;            # Float64 (mixed with Float64 literal)
e := float32(d);         # back to Float32
```

## Null and coalescing

`null` is a value, not a type error. Null coalescing and null-coalescing assignment make optional configuration ergonomic:

```vyne
name := maybe_name ?? "anonymous";
cfg.mode ??= "default";
```

## Maps

Maps have string keys and an unspecified traversal order.

```vyne
cfg :: Map := {"mode": "fast"};
cfg["retries"] = 3;
if cfg.has("retries") {
    out(cfg["retries"]);
}
```

Use `Map` for configuration, JSON-shaped data, and sparse string-keyed lookups. For numeric data, typed arrays are the right tool.

## Defaults

A declaration without an initializer receives a default: numeric zero, `false`, empty string, empty array, or `null` for a structured value. Do not rely on a non-null structured default; check explicitly when the distinction matters.

## Lifetimes

A value allocated inside a region is invalid after the region rewinds unless it was committed or moved. The compiler's region escape analysis (VNE-070) catches the common direct-assignment case, but it is deliberately syntactic: a pointer hidden inside a struct field can still escape. See [Memory & Regions](page:regions).

## See also

- [Literals & Expressions](page:expr)
- [Variables & Assignment](page:vars)
- [Collections & Built-ins](page:builtins)
- [Interfaces](page:iface)

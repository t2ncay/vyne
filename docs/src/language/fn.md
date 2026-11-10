---
id: fn
title: Functions, Generics & Contracts
group: Language Reference
icon: fa-square-root-variable
order: 5
description: Declarations, typed and untyped parameters, generics by monomorphization, defer, and executable contracts.
---

# Functions, Generics & Contracts

## Declaration

```vyne
fn add(a :: Int64, b :: Int64) -> Int64 {
    return a + b;
}

fn greet(name) {
    out("hello, " + name);
}
```

Parameters may be typed or untyped. A typed function is a candidate for native lowering; an untyped one takes the boxed path. A missing return type means the function returns no value. `return` exits immediately; a bare `return;` is allowed in a no-value function.

Named arguments and default parameter values are not supported. Call arguments are positional.

## Generics

```vyne
fn max_of<T>(a :: T, b :: T) -> T {
    if a > b { return a; }
    return b;
}

fn identity<T>(x :: T) -> T {
    return x;
}

a := max_of<Int64>(3, 5);
b := max_of<Float64>(3.0, 5.0);
c := identity<String>("vyne");
```

Generics are monomorphized: each distinct type tuple produces a separate C function. Recursive instantiation is detected and terminates with a diagnostic. Generic interfaces are supported too:

```vyne
interface Box<T> {
    value :: T;
}

b :: Box<Int64> := Box<Int64>(42);
```

## Native ABI

A function is eligible for an unboxed native variant when:

- every parameter is a primitive (`Int64`, `Float64`, `Float32`, `Bool`), a typed array (`Array<Float64>`, `Array<Int64>`, `Array<Float32>`), or a struct with a registered field layout;
- the return type is a primitive, typed array, or such a struct; and
- the body contains no top-level `defer` or `try`.

Eligible functions are emitted twice: once with the boxed signature `(int argc, VyneValue* argv) -> VyneValue`, and once with a native signature such as:

```c
double fn_vlin_k_dot(double* a, double* b, int64_t n);
```

Call sites prefer the native variant when every argument matches. Struct parameters expand to one C argument per field in declaration order; array parameters pass `.data` directly. On AVR this is what keeps numerical kernels off the 1 KB arena.

## Defer

`defer` registers a statement or block to run when the enclosing scope exits, including early returns. Defers run in LIFO order.

```vyne
fn demo() -> Int64 {
    defer out("first registered");
    defer out("second registered");

    out("body");
    return 42;
}
```

Output order is `body`, `second registered`, `first registered`, then the returned value is observed by the caller. A function with a top-level `defer` is not eligible for the native ABI.

## Contracts

`requires` runs at function entry; `ensures` runs before every return, normal or early. Inside `ensures`, `result` names the return value.

```vyne
ruleset { contracts = "check" };

fn positive(x :: Int64) -> Int64
requires(x > 0)
ensures(result > 0)
{
    return x * 2;
}
```

| `contracts` mode | Effect |
| :--- | :--- |
| `"check"` | Emit assertions; violation halts with `VNE-111` |
| `"evidence"` | Assert and append a CSV pass/fail row to `vyne_contracts.log` (or `$VYNE_CONTRACT_LOG`) |
| `"off"` | Emit nothing (release path) |

Contracts are also available on loops as `@invariant(...)`. See [Control Flow](page:flow).

## Recursion

Recursion is supported on both backends. The native path handles direct recursion; mutual recursion works through the boxed call path when the signatures cannot be specialized.

```vyne
fn fib(n :: Int64) -> Int64 {
    if n <= 1 { return n; }
    return fib(n - 1) + fib(n - 2);
}
```

## Calling convention summary

| Call form | Example |
| :--- | :--- |
| Plain function | `add(2, 3)` |
| Generic | `identity<Float64>(1.0)` |
| Module function | `vmath.sqrt(9.0)` |
| Group function | `Geometry.area_of_circle(2.0)` |
| Interface method | `circle.area()` |
| Pipeline | `5 |> double` |

## See also

- [Interfaces](page:iface)
- [Groups & Modules](page:mods)
- [Automatic Differentiation](page:autodiff)
- [Rulesets](page:rulesets)

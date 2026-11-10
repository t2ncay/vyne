---
id: tour
title: Language Tour
group: Getting Started
icon: fa-route
order: 4
description: A single pass over every construct you will meet in real Vyne code.
---

# Language Tour

This is the fast path through the language. Each section links to the full reference page.

## Declarations and types

`:=` introduces a binding. `=` reassigns an existing one. Types are written after `::`.

```vyne
x := 42;                       # inferred
y :: Int64 := 42;              # explicit
const PI :: Float64 := 3.141592653589793;
z :: Float64;

x = x + 1;                     # reassignment, not a new binding
```

| Type | Example | Notes |
| :--- | :--- | :--- |
| `Int64` | `42`, `0x7FFF_FFFF` | 64-bit signed; hex and `_` separators |
| `Float64` | `3.14`, `1.0e3` | IEEE-754 double |
| `Float32` | `1.0f`, `2f`, `float32(x)` | first-class single precision |
| `String` | `"hello"` | UTF-8, immutable |
| `Bool` | `true`, `false` | |
| `Null` | `null` | |
| `Array` | `[1, "two", 3.0]` | heterogeneous, boxed |
| `Array<T>` | `Array<Float64>` | typed, native ABI on the C backend |
| `Map` | `{"key": 42}` | string keys |

`Float32` is sticky: `Float32` op `Float32` stays `Float32`; mixing with `Float64` or `Int64` promotes to `Float64`. Typed arrays use 32-byte aligned payloads by default.

## Operators

```vyne
a := 2 ** 10;          # power
b := 7 // 2;           # floor division
c := flags & 0xFF;     # bitwise AND
d := flags | 0x80;     # bitwise OR
e := a ^ b;            # bitwise XOR
f := ~a;               # bitwise NOT
g := 1 << 8;           # shift left
h := g >> 2;           # shift right

name ?? "anonymous";    # null coalesce
cfg.mode ??= "default"; # null coalesce assignment
data |> normalize;      # pipeline
ready ? "go" : "wait";  # ternary
```

Ranges are inclusive. `through` is the canonical iteration construct:

```vyne
squares := through i :: 1..10 -> collect { i * i };
evens   := through i :: 0..100 -> filter { i % 2 == 0 };
allPos  := through i :: list -> every { i > 0 };
uniq    := through i :: list -> unique { i };

through i :: 0..9 -> loop {
    out(i);
};
```

The five modes are `loop` (side effects only), `collect`, `filter`, `every`, and `unique`. `break` and `continue` work in `loop` bodies.

## Control flow

```vyne
if x > 0 {
    out("positive");
} else if x < 0 {
    out("negative");
} else {
    out("zero");
};

while running {
    step();
};

through i :: 0..n-1 -> loop {
    if i == 3 { continue; }
    if i > 8 { break; }
};
```

Loops and functions can carry executable contracts:

```vyne
@invariant(total >= 0)
through i :: 0..n-1 -> loop {
    total = total + i;
};

fn positive(x :: Int64) -> Int64
requires(x > 0)
ensures(result > 0)
{
    return x * 2;
}
```

`ruleset { contracts = "check" | "evidence" | "off" };` controls assertion lowering. A violation halts with `VNE-111`.

## Functions and generics

```vyne
fn add(a :: Int64, b :: Int64) -> Int64 {
    return a + b;
}

fn max_of<T>(a :: T, b :: T) -> T {
    if a > b { return a; }
    return b;
}
```

Generic parameters are monomorphized: `max_of<Int64>(3, 5)` and `max_of<Float64>(3.0, 5.0)` produce two native C functions. Functions with primitive, typed-array, or registered-struct signatures also get an unboxed native variant.

## Interfaces

An `interface` is both a struct declaration and its constructor. Methods close over the fields.

```vyne
interface Circle {
    r :: Float64;
    area() -> Float64 {
        return r * r * 3.141592653589793;
    }
}

c :: Circle := Circle(4.0);
out(c.area());     # 50.265...
```

Struct-typed values participate in the native ABI: the caller and callee agree on a flat field layout, so a struct parameter expands to one C argument per field.

## Groups and modules

`group` namespaces functions and constants without creating a type. `module` makes a built-in module available. `use` imports source files or external libraries.

```vyne
group Geometry {
    const TAU :: Float64 := 6.283185307179586;
    fn area_of_circle(r :: Float64) -> Float64 {
        return r * r * Geometry.TAU / 2.0;
    }
}

use native vmath;
use native vfs;
use external "vlin/vlin.vy";

out(Geometry.area_of_circle(2.0));
out(vmath.sqrt(9.0));
```

`use native` selects a compiler-provided module. `use external "path"` resolves a library under `vyne/modules/external/`. Plain `use "file.vy"` imports a sibling source file.

## Enums

```vyne
enum Color {
    RED = 0,
    GREEN,
    BLUE,
}

out(Color.GREEN);   # 1
```

## Error handling

```vyne
try {
    parse(input);
} catch (err) {
    out("failed: " + string(err));
} finally {
    cleanup();
}

throw "something went wrong";
```

Exceptions are `longjmp`-based on the compiled backend and stack-unwound in the interpreter. The compiled runtime keeps a bounded frame stack (`VYNE_MAX_EXC_FRAMES` = 64).

## Rulesets

A `ruleset` configures compile-time and runtime policy from the point it appears:

```vyne
ruleset {
    type_check      = "strict",
    memory_limit    = 256 * 1024 * 1024,
    warnings        = "all",
    warnings_ignore = ["unused_variable"],
};

ruleset { contracts = "evidence" };
```

## Regions

A `region` scopes arena lifetime lexically:

```vyne
through epoch :: 1..N -> loop {
    region train_step {
        scratch grad :: Float64[64, 16];
        # allocations are reclaimed at the closing brace
    };
};
```

`region` carries at most one allocation, lifetime, or verification policy. Policies include `@bounded<N>`, `@zeroed`, `@deterministic`, `@realtime<ms>`, `@pool<T, N>`, `@slab<T, MAX[, CAP]>`, `@ring<T, N, MAX>`, `@streamed<T, MAX>`, `@persistent`, `@speculative`, and `@device<backend>`. Composition attributes include `@layout<soa>`, `@backward`, `@recompute`, and `@mixed`.

```vyne
@bounded<16_777_216>
@deterministic
@layout<soa>
region physics {
    scratch particles :: Particle[10_000];
};
```

`scratch` lowers to a raw C array on the stack, not the arena. `region.commit(x)` deep-clones a value into the commit arena; `region.move(x)` relocates a large typed-array block in O(1); `drop(x)` releases an arena-tail payload early.

## Automatic differentiation

```vyne
@differentiable
fn loss(w :: Float64, b :: Float64) -> Float64 {
    z :: Float64 := w * 1.0 + b;
    p :: Float64 := 1.0 / (1.0 + vmath.exp(-z));
    return (p - 0.0) * (p - 0.0);
}

@backward
@bounded<64_000_000>
region train_step {
    l :: Float64 := loss(w, b);
    backward(l);
    out("dL/dw = " + string(w.grad));
};
```

`grad(f, x)` is the expression form. Scalar straight-line bodies and `Array<Float64>` reductions through the `vstat`/`vlin` kernels are supported; everything else is rejected with a stable `VNE-12x` diagnostic rather than being miscompiled. A custom adjoint can be registered as `<fn>_vjp`.

## Devices

```vyne
@device<cuda> region gpu {
    n :: Int64 := 1024;
    a :: Array<Float32> := [];
    b :: Array<Float32> := [];
    through i :: 0..n-1 -> loop { a.push(float32(i)); };

    da :: Int64 := device_alloc(n);       # Float32 by default
    db :: Int64 := device_alloc(n);
    device_upload(da, a, n);
    device_upload(db, b, n);
    device_add(da, da, db, n);
    device_sync();
};
```

`@device` opens a device frame, runs the body, then releases device allocations before the host rewind. The CUDA driver backend JIT-compiles embedded PTX and falls back to CPU when CUDA is unavailable. `device_alloc_f64` opts into double precision.

## Arduino

```vyne
use native arduino;

arduino.serial_begin(115200);

through i :: 0..9 -> loop {
    arduino.digital_write(13, 1);
    arduino.delay(100);
    arduino.digital_write(13, 0);
    arduino.delay(100);
};

@bounded<512>
region probe {
    scratch buf :: Float64[16];
    through i :: 0..15 -> loop { buf[i] = arduino.sin(0.1 * i); };
    arduino.serial_print(buf.mean());
};
```

The same `region` and `scratch` rules apply. On AVR the arena is 1 KB, checkpoints nest 16 deep, and `out()` is unavailable — use `arduino.serial_print`.

## Where to go next

- [Values & Types](page:types)
- [Memory & Regions](page:regions)
- [Automatic Differentiation](page:autodiff)
- [Library Overview](page:libs)
- [SRCNN Showcase](page:srcnn)

---
id: iface
title: Interfaces
group: Language Reference
icon: fa-shapes
order: 6
description: Structured values with typed fields and methods, constructors, generics, and the native struct ABI.
---

# Interfaces

An `interface` declares a structured type: a set of typed fields plus methods that close over them. The interface name is also the constructor.

```vyne
interface Circle {
    r :: Float64;

    area() -> Float64 {
        return self.r * self.r * 3.141592653589793;
    }

    scaled(k :: Float64) -> Circle {
        return Circle(self.r * k);
    }
}

c :: Circle := Circle(4.0);
out(c.area());          # 50.265...
out(c.scaled(0.5).r);   # 2.0
```

Fields are written in declaration order. The constructor takes one argument per field, in that order. Semicolons and commas both separate fields; the manuals use both styles.

## Methods and `self`

Methods are declared inside the interface body without a `fn` keyword. `self` refers to the receiver.

```vyne
interface Counter {
    value :: Int64;

    bump() {
        self.value = self.value + 1;
    }

    get() -> Int64 {
        return self.value;
    }
}

c := Counter(0);
c.bump();
c.bump();
out(c.get());   # 2
```

Methods may call free functions, group functions, and external libraries. They may return a new instance of the same interface.

## Field access and assignment

```vyne
point := Point(1.0, 2.0);
out(point.x);
point.x = 3.0;
out(point.x);        # 3.0

matrix.data[i] = 0.0;
```

Field assignment is a normal assignment target. When the interface is represented as a native C struct, field writes lower directly to the corresponding C member; otherwise they take the boxed member path.

## Constructors

```vyne
Person("Ada", 36);
Pair<Int64, String>(7, "seven");
Box<Float64>(3.5);
```

The constructor name may be qualified by a group. External libraries use this heavily: `vlin.Types.Matrix(...)`, `vstat.Series(data, n)`, `vchem.Graph(...)`.

## Generic interfaces

```vyne
interface Box<T> {
    value :: T;
}

interface Pair<A, B> {
    first  :: A;
    second :: B;
}

b :: Box<Int64> := Box<Int64>(42);
p :: Pair<Int64, String> := Pair<Int64, String>(7, "seven");
```

Type parameters are monomorphized like generic functions. Nested generic annotations such as `Nested<Int64>` are accepted.

## Native struct ABI

Struct-typed values participate in the native boundary:

- a struct parameter expands into one C argument per field, in declaration order;
- a struct return uses an out-parameter;
- a registered field layout makes field access constant-time (`last_field_idx`);
- arrays of native structs are supported, with boxed fallbacks where needed.

This is what lets a library pass `Matrix { row, col, data }` across the native boundary without boxing every field. On AVR it is what keeps a four-field struct on registers instead of forcing an arena round-trip.

An interface is eligible for native C representation when every field has a C-compatible type and the layout is registered. Libraries that need a native surface keep their hot types in this shape (`vlin.Types.Matrix`, `vfft.Plan`, `vml.Types.Dense`).

## Arrays of structs

```vyne
interface Particle {
    x :: Float64;
    y :: Float64;
    z :: Float64;
    mass :: Float64;
}

particles :: Array<Particle> := [];
particles.push(Particle(0.0, 0.0, 0.0, 1.0));
```

For performance-sensitive arrays, `@layout<soa>` lowers struct `scratch` to one packed array per field. See [Memory & Regions](page:regions).

## Lifetimes

A struct allocated inside a region is invalid after the rewind unless it was committed or moved. Region escape analysis catches direct assignments, but a pointer stored inside a struct field can still outlive its region. Do not rely on the checker to see through every field.

## See also

- [Values & Types](page:types)
- [Functions](page:fn)
- [Groups & Modules](page:mods)
- [Memory & Regions](page:regions)

---
id: vars
title: Variables & Assignment
group: Language Reference
icon: fa-tag
order: 3
description: Declaration with :=, reassignment with =, const bindings, scopes, and reference parameters.
---

# Variables & Assignment

## Declaration versus reassignment

The parser distinguishes binding a new name from rebinding an existing one:

```vyne
x := 42;          # declaration, inferred type
y :: Int64 := 42; # declaration, explicit type
z :: Float64;     # declaration, default value

x = x + 1;        # reassignment
y = 7;            # reassignment
```

Using `=` on a name that was never declared is an error. Using `:=` on an existing name introduces a new binding in the current scope (shadowing). This split was introduced in v0.5.0; older code that declared with `=` must be migrated to `:=`.

## Type annotations

```vyne
count  :: Int64   := 10;
ratio  :: Float64 := 0.5;
name   :: String  := "vyne";
xs     :: Array<Float64> := [1.0, 2.0];
person :: Person  := Person("Ada");
```

An explicit annotation is checked on assignment. A declaration without one gets the inferred type when the compiler can prove it, or a boxed dynamic value when it cannot. Under `ruleset { type_check = "strict" }`, an untyped declaration is a compile error.

## Constants

```vyne
const PI :: Float64 := 3.141592653589793;
const TAU := PI * 2.0;
```

A `const` binding cannot be reassigned. It is a compile-time restriction, not a runtime immutable object; composite values still have normal region lifetimes.

## Defaults

A declaration without an initializer receives a default:

| Declared type | Default |
| :--- | :--- |
| `Int64` | `0` |
| `Float64`, `Float32` | `0.0` |
| `Bool` | `false` |
| `String` | `""` |
| `Array` / `Array<T>` | empty array |
| interface / `Map` | `null` |

Do not assume an interface default is a usable object. Check for `null` when the distinction matters.

## Scope and shadowing

Bindings are lexically scoped. A block may introduce a new binding with the same name as an outer binding:

```vyne
x := 1;
if flag {
    x := 2;       # inner binding
    out(x);       # 2
};
out(x);           # 1
```

`warnings` reports shadowed variables when enabled; add `shadow_variable` to `warnings_ignore` if the shadow is intentional.

## Assignment targets

```vyne
xs[0] = 1.5;
matrix.data[i] = 0.0;
cfg["mode"] = "fast";
point.x = 3.0;
cfg.mode ??= "default";
```

Index and member assignment follow the same lifetime rules as reads. Assigning an arena-backed value into a shallower variable can trigger region escape analysis (VNE-070).

## Reference parameters

`^^` declares a reference parameter. The callee sees and can mutate the caller's variable:

```vyne
fn bump(v :: Int64^^) {
    v = v + 1;
}

counter :: Int64 := 0;
bump(counter);
out(counter);     # 1
```

`$` returns the address of a variable as an `Int64`:

```vyne
addr := $counter;
```

Address-of is an interpreter feature. The compiled backend rejects it; use reference parameters or pass an explicit buffer when you need aliasing in compiled code.

## No destructuring

Vyne does not currently support destructuring declarations or assignments. Bind each name explicitly.

## See also

- [Values & Types](page:types)
- [Functions](page:fn)
- [Memory & Regions](page:regions)

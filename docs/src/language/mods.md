---
id: mods
title: Groups & Modules
group: Language Reference
icon: fa-box
order: 7
description: Namespacing with groups, importing native and external modules, and how the linker resolves source files.
---

# Groups & Modules

## Groups

`group` namespaces functions and constants without introducing a new type.

```vyne
group Geometry {
    const TAU :: Float64 := 6.283185307179586;

    fn area_of_circle(r :: Float64) -> Float64 {
        return r * r * Geometry.TAU / 2.0;
    }
}

out(Geometry.area_of_circle(2.0));
```

Groups may be nested inside interfaces and modules. A group can also bind to a module namespace, which is how libraries organize `Types`, `Kernels`, and `Ops`:

```vyne
group :: vlin Types {
    interface Matrix {
        row  :: Int64,
        col  :: Int64,
        data :: Array<Float64>,
    }
}
```

## Modules

A `module` declaration makes a named module available in the current file. The current spelling for compiler-provided modules is `use native`:

```vyne
use native vmath;
use native vcore;
use native vfs;
use native vmem;
use native arduino;   # AVR target only
```

The older form is still accepted:

```vyne
module vmath;
module vmem;
```

Use one form consistently. `use native` is what the current examples use.

## Importing source files

```vyne
use "helpers.vy";              # sibling source file
use "subdir/util.vy";          # relative path
```

A facade library pulls in its leaf modules in dependency order and ends with `deploy`:

```vyne title="vyne/modules/external/vsignal/vsignal.vy"
use "Types.vy";
use "Constructors.vy";
use "Kernels.vy";
use "Iir.vy";
use "Fir.vy";
use "Windows.vy";
use "Resample.vy";

ruleset { dynamic_casting };
module vsignal;

deploy vsignal;
```

`deploy <module>;` publishes the module surface to consumers. `dismiss` removes a binding from the current scope; it is rarely used in application code.

## Importing external libraries

```vyne
use external "vlin/vlin.vy";
use external "vstat/vstat.vy";
use external "voptim/voptim.vy";
use external "srcnn/model.vy";   # a project-local path is a plain use
```

The `external` path is resolved against `vyne/modules/external/`. Leaf modules inside a facade are imported relative to that facade. A library may depend on another external library by importing its facade.

Project dependencies declared in `project.toml` are resolved against the same module roots:

```toml
[dependencies]
vlin = "0.4"
vstat = "0.2"
voptim = "0.4"
```

## Resolution order

The linker walks the import graph depth-first, resolves each path, and emits modules in topological order. Cycles between source files are detected and reported instead of recursing forever.

| Form | Resolves against |
| :--- | :--- |
| `use native <name>` | compiler-provided module registry |
| `use external "lib/facade.vy"` | `vyne/modules/external/` |
| `use "file.vy"` | directory of the importing file |
| `use "dir/file.vy"` | relative to the importing file |

## Best practices

- Import the facade (`vlin/vlin.vy`), not individual leaves, unless you are deliberately trimming the compile unit. `vsignal/Spectral.vy` is a documented exception: it pulls in `vfft` only when you ask for it.
- Keep `ruleset { dynamic_casting };` at the top of facades that use untyped declarations. Application code should prefer explicit types.
- Use `group` to keep the public surface tidy; use `deploy` only in facades.

## See also

- [Library Overview](page:libs)
- [Projects & Tooling](page:projects)
- [Interfaces](page:iface)

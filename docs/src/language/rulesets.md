---
id: rulesets
title: Rulesets
group: Language Reference
icon: fa-sliders
order: 13
description: Compile-time and runtime policy for type checking, warnings, memory, contracts, and diagnostics.
---

# Rulesets

A `ruleset` declares policy from the point it appears. It can be a single flag or a block of settings:

```vyne
ruleset { dynamic_casting };
ruleset { warnings };

ruleset {
    type_check       = "strict",
    memory_limit     = 256 * 1024 * 1024,
    warnings         = "all",
    warnings_ignore  = ["unused_variable", "shadow_variable"],
    contracts        = "check",
};
```

Settings are applied in order. A ruleset can appear at any scope; its effect applies forward from that point.

## Available settings

| Setting | Form | Effect |
| :--- | :--- | :--- |
| `dynamic_casting` | flag | Relax type checking; declarations may omit `:: Type` and implicit conversions are allowed. |
| `type_check` | `"strict" \| "hybrid" \| "dynamic"` | Select the type-checking mode. `strict` requires explicit annotations. |
| `implicit_casting` | `"warn" \| "allow" \| "deny"` | Policy for implicit numeric/string conversions. |
| `warnings` | flag or `"all" \| "none" \| "error_only"` | Enable or filter warnings. |
| `warnings_ignore` | string array | Suppress named warning categories, for example `unused_variable`. |
| `strict` | flag | Treat warnings as errors. |
| `pedantic` | flag | Enable style and performance hints. |
| `verbose` | flag | Show notes as well as warnings and errors. |
| `memory_limit` | flag or integer bytes | Enable memory-usage tracking. The current hook reports; it is not a hard runtime ceiling. |
| `contracts` | flag or `"check" \| "evidence" \| "off"` | Control `requires` / `ensures` / `@invariant` lowering. |
| `profiling` | flag or `"on"` | Enable profiling notes. |
| `debug` | flag or `"on"` | Enable debug notes. |
| `trace` | flag or `"on"` | Enable trace notes. |
| `max_iterations` | integer | Set the diagnostic iteration budget. |

## Type checking

The default mode is a hybrid: explicit annotations are checked, untyped declarations are allowed with a warning. `dynamic_casting` relaxes the checker:

```vyne
ruleset { dynamic_casting };

x = 42;          # accepted; boxed dynamic binding
y = "hello";
```

`type_check = "strict"` requires every declaration to carry a type:

```vyne
ruleset { type_check = "strict" };

n :: Int64 := 42;
name :: String := "vyne";
```

`dynamic_casting` is appropriate for scripts and library facades that intentionally use untyped locals. It is not recommended for application code.

## Warnings

```vyne
ruleset { warnings };

ruleset {
    warnings        = "all",
    warnings_ignore = ["unused_variable"],
};
```

Warnings cover unused variables, shadowed variables, and a few style patterns. `strict` upgrades warnings to errors; `pedantic` adds extra hints. `warnings_ignore` takes an array of category names.

## Contracts

```vyne
ruleset { contracts = "check" };
```

| Value | Effect |
| :--- | :--- |
| `"check"` (default when the flag is present) | Emit assertions; a violation halts with `VNE-111`. |
| `"evidence"` | Assert and append a CSV pass/fail row to `vyne_contracts.log` or `$VYNE_CONTRACT_LOG`. |
| `"off"` / `"none"` / `"release"` | Emit nothing. |

`evidence` is useful in CI: it records which contract was exercised, not just whether the program survived.

## Memory limit

```vyne
ruleset { memory_limit = 268_435_456 };   # 256 MB
```

The flag form enables tracking with a default limit. The value form sets the threshold. Today this feeds the tracking hook; region policies (`@bounded`) are the enforceable per-region mechanism.

## Ruleset scope

Because settings apply forward from the declaration, a common pattern is:

```vyne
ruleset { dynamic_casting, warnings };   # facade-level convenience

fn public_api(x :: Float64) -> Float64 {
    return x * 2.0;
}
```

Keep rulesets at the top of a file unless you deliberately want to change policy mid-file. A ruleset is not a runtime flag alone: some settings affect parsing and type resolution, so ordering matters.

## See also

- [Error Codes](page:errcodes)
- [Functions](page:fn)
- [Memory & Regions](page:regions)
- [Error Handling](page:errors)

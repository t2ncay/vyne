---
id: errcodes
title: Error Codes
group: Reference
icon: fa-bug
order: 4
description: A lookup table for VNE diagnostics, grouped by compiler phase and runtime policy.
---

# Error Codes

Diagnostics are emitted with a stable `VNE-*` code, a severity, a source location, an excerpt, and optional suggestions. Codes in the `VNE-010`–`VNE-032` range are mostly informational notes emitted when a ruleset is applied.

## Lexer and parser

| Code | Meaning |
| :--- | :--- |
| `VNE-001` | Unexpected token |
| `VNE-002` | Missing semicolon |
| `VNE-003` | Unknown type name |
| `VNE-005` | Unresolved import |
| `VNE-006` | Malformed numeric literal (hex or float) |
| `VNE-007` | Integer literal overflow |
| `VNE-008` | Unexpected character |

## Rulesets and diagnostics

| Code | Kind | Meaning |
| :--- | :--- | :--- |
| `VNE-010` | note | Warnings enabled |
| `VNE-011` | note | Dynamic casting enabled |
| `VNE-012` | note | Memory-limit tracking enabled |
| `VNE-013` | error | Unknown ruleset flag |
| `VNE-014` | error | Value expected for a ruleset key |
| `VNE-015` | note | Memory limit set |
| `VNE-016` | warning | Unknown ruleset value |
| `VNE-017` | error | Invalid warning level |
| `VNE-020` | warning | Unknown ruleset flag |
| `VNE-021` | note | Valid `type_check` modes |
| `VNE-022` | warning | Unknown `type_check` mode |
| `VNE-023` | note | Valid `implicit_casting` modes |
| `VNE-024` | note | Profiling enabled |
| `VNE-025` | note | Profiling disabled |
| `VNE-026` | note | Debug mode enabled |
| `VNE-027` | note | Trace mode enabled |
| `VNE-028` | note | Max iterations set |
| `VNE-029` | note | Type-check mode hint |
| `VNE-030` | note | Warnings disabled hint |
| `VNE-031` | note | Pedantic mode enabled |
| `VNE-032` | note | Verbose mode enabled |

## Generics, interfaces, and declarations

| Code | Meaning |
| :--- | :--- |
| `VNE-040` | Malformed interface type-parameter list |
| `VNE-041` | Interface type parameter is not a simple identifier |
| `VNE-042` | Malformed function type-parameter list |
| `VNE-043` | Function type parameter is not a simple identifier |
| `VNE-050` | Expected `commit`, `commit_if`, or `move` after `region.` |
| `VNE-060` | Invalid `scratch` element type |
| `VNE-061` | Scratch dimension is not positive or not a valid expression |
| `VNE-062` | Struct `scratch` without `@layout<soa>` |
| `VNE-063` | Invalid `@layout` mode or whole-element SoA access |
| `VNE-100` | Method table full (`VYNE_MAX_METHODS` = 4096) |

## Regions and scratch

| Code | Kind | Meaning |
| :--- | :--- | :--- |
| `VNE-070` | error | Region escape: assigning region-local storage to a shallower variable |
| `VNE-071` | error | Scratch shape mismatch or invalid single index on rank > 1 |
| `VNE-072` | runtime | Scratch index out of bounds |
| `VNE-080` | error | Invalid region policy argument, composition, or target |
| `VNE-081` | error | Invalid policy argument token |
| `VNE-082` | runtime | Pool/slab/ring/streamed capacity exceeded |
| `VNE-083` | error/runtime | Intrinsic misuse (`drop`, `move`, pool/slab/ring/streamed alloc/free, persistent nesting) |
| `VNE-084` | error | `commit` / `move` on a stack-allocated scratch array |
| `VNE-085` | error | `commit_if` outside `@speculative` |
| `VNE-086` | error | Qualified commit across a nesting level |
| `VNE-089` | error | Invalid `@realtime` argument |
| `VNE-090` | runtime | `analog_write_resolution` above 8 (clamped) |
| `VNE-091` | runtime | Pin has no external interrupt |
| `VNE-101` | error | `@bounded` compile-time violation (static scratch or bad argument) |
| `VNE-102` | runtime | `@bounded` exceeded at runtime (arena or VLA) |
| `VNE-106` | error | `@deterministic` region contains a time/RNG source |
| `VNE-107` | error | `return` inside `@zeroed` / `@deterministic` |
| `VNE-108` | error | `return` inside `@bounded` |
| `VNE-109` | runtime | `@realtime` deadline overrun |

## Contracts

| Code | Kind | Meaning |
| :--- | :--- | :--- |
| `VNE-111` | runtime | `requires` / `ensures` / `@invariant` violation |
| `VNE-112` | error | Malformed contract or `@invariant` |
| `VNE-113` | warning | Unknown `contracts` mode |

## Automatic differentiation

| Code | Meaning |
| :--- | :--- |
| `VNE-120` | Function outside the supported `@differentiable` subset |
| `VNE-121` | Unsupported statement or mutation inside `@differentiable` |
| `VNE-122`–`VNE-125` | Unsupported expression or `grad()` misuse |
| `VNE-126` | `backward()` could not resolve the recorded differentiable call |
| `VNE-127` | Malformed `newton_solve()` call or residual outside the implicit scalar subset |
| `VNE-128` | Runtime implicit solve failure (non-convergence or singular `dF/dz`) |

## Reading a diagnostic

Every diagnostic in the terminal prints:

- the severity (error, warning, note, perf hint);
- the code, which you can look up here;
- the message;
- the source location (`file:line`);
- a short excerpt with a caret at the column;
- zero or more suggestions when the compiler has a specific fix.

If a code is missing from this page, treat that as a documentation bug: the compiler's diagnostics are the source of truth.

## AVR-specific runtime diagnostics

On the Arduino target, some desktop `fprintf + exit` paths become serial output followed by a halt:

- `VNE-072` scratch bounds — `vyne_panic_bounds`
- `VNE-102` `@bounded` — `vyne_panic_region_bound`
- `VNE-109` `@realtime` — serial message and halt
- `VNE-111` contract violation — serial message and halt

## See also

- [Error Handling](page:errors)
- [Rulesets](page:rulesets)
- [Memory & Regions](page:regions)

# Vyne transpiler codegen

This directory replaces the original `codegen/` directory. `codegen.cpp` is
now an intentionally empty **compatibility translation unit**; each feature
has one implementation `.cpp`. Compile/link **all** `.cpp` files in this
folder, including the unchanged `linker.cpp`, exactly once. For a CMake target
with an explicit source list, add the new files; for a wildcard over
`codegen/*.cpp`, no source-list change is needed. Do not compile the original
monolithic `codegen.cpp` alongside these files: that duplicates definitions.

## Architecture

- `codegen.h`: shared/public dependencies for AST node C emission.
- `detail/codegen_helpers.h`: translation-unit-local, stateless helpers for
  native coercion, array-element inference and literal formatting.
- `emitter.h`: output buffers, scopes, static type metadata and emission
  context. All per-compilation mutable state belongs to `C_Emitter`.
- `ctype.h`: static type to C representation mapping.
- `native_maps.h`: native function metadata.
- `linker.h/.cpp`: import graph ordering; separate from AST-to-C lowering.

| Source                                              | Responsibility                                            |
| --------------------------------------------------- | --------------------------------------------------------- |
| `literals.cpp`                                      | Number/string/bool/null                                   |
| `assignments.cpp`                                   | Variables, assignment and native initialization           |
| `blas.cpp`                                          | BLAS lowering for `vlin.multiply` under `--blas`          |
| `builtins.cpp`                                      | Built-in calls                                            |
| `operators.cpp`                                     | Binary, unary and postfix                                 |
| `control_flow.cpp`, `loops.cpp`                     | Branches, returns, loops                                  |
| `functions.cpp`, `function_calls.cpp`               | Definitions and calls                                     |
| `collections.cpp`                                   | Arrays, indexing, ranges, slicing                         |
| `builtins.cpp`                                      | Built-in calls                                            |
| `program.cpp`                                       | Program, block, ternary                                   |
| `members.cpp`, `interfaces.cpp`, `method_calls.cpp` | Fields and methods                                        |
| `groups_modules.cpp`, `imports.cpp`                 | Groups, modules, imports                                  |
| `language_features.cpp`                             | Enums, defer, lifecycle, coalescing, membership, pipeline |
| `exceptions.cpp`, `regions.cpp`                     | Exception and region constructs                           |
| `autodiff.h`, `autodiff.cpp`                        | `@differentiable` adjoints, `@backward`, `grad`           |
| `maps_strings_scratch.cpp`                          | Maps, interpolation, scratch arrays                       |

## Invariants when extending codegen

1. AST expressions may emit prerequisite statements via `getCExpr`: evaluate
   them in source order before using the returned C expression. Never reorder
   operands just because their return values are strings.
2. Box at dynamic boundaries. Preserve proven static types as native C values
   only where safe. For typed arrays, use `boxTypedArray`, not `boxIfNative`.
3. Add new AST method implementations to the appropriate feature `.cpp`, and
   add the new `.cpp` to explicit build lists. Avoid definitions in headers.
4. `emitter.h`, `ctype.h`, `native_maps.h`, `linker.*` are carried over
   unchanged; this is a structural split, not a semantic rewrite.

The original function bodies in `codegen.cpp` have been preserved, with
cross-file helpers moved into `detail/codegen_helpers.h`. The attachment set
lacks e.g. `utils/file_utils.h` and the complete runtime directory, so a full
project build cannot be verified from these files alone. Run your existing
build and regression suite after replacing the directory.

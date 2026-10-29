# Vyne v0.4.0 — dual licensing, three new libraries, and the native-struct ABI

**Released 2026-10-04**

This release formalizes the dual-license structure of the project, lands three new standard libraries (`vjson`, `vrand`, `vfft`), introduces the `@pool` region policy, completes the native struct ABI with field-layout registration and out-parameter returns, and closes a cluster of residual defects in the emitter's array and collection handling that survived the v0.3.0 empty-array fix. It also ships the VFS module, vmem peak-allocation tracking, and a broad refactor of `MethodCallNode` into a dispatch chain.

---

## Highlights

- **Dual licensing, formalized.** Compiler sources are AGPL-3.0-only, runtime sources are MIT. Every C/C++ source file now carries an SPDX header identifying its license, and the repository carries the full AGPL-3.0 text, the MIT text, and updated license documentation. Commits `f12b4d9`, `14835ac`, `a10365f`.

- **Three new libraries.** `vjson` (parse, serialize, validate), `vrand` (continuous and discrete distributions, sampling utilities, RNG access), and `vfft` (FFT with a cached twiddle-factor and bit-reversal table). Each ships with a README and either a demo or a benchmark. Commits `eb4da7b`, `cfcec99`, `1ac7dd7`, `336a3e5`, `b36749b`, `a6c6c3a`, `8e0e2df`.

- **`@pool` region policy.** A fixed-slot allocator with runtime support. Where the default region grows linearly through the commit arena, `@pool` releases slots at O(1) and exposes a fixed capacity at the policy level. Positive-coverage tests accompany the change. Commits `68c9a17`, `5ff510c`, `aa56396`.

- **Native struct ABI.** Struct types are now first-class across the native boundary: field-layout registration, dispatch support, return-by-out-parameter, and `last_field_idx` for constant-time field access. Native C structs are also now eligible for interface representation. Commits `50669ff`, `653f4c2`, `62f805f`, `204f05a`.

- **`MethodCallNode` refactor.** Method-call lowering is now a dispatch chain (native module methods → interface constructors → group methods → built-in array methods → string methods → typed-array methods), with per-branch argument handling and consistent receiver evaluation. Commit `946b279`.

- **Array and collection semantics corrected.** A cluster of fixes closes residual gaps in the boxed fallback path for `ForNode` collect, untyped `Array` assignment, empty-literal returns, and the native `collect` fast path. Commits `a95fa39`, `6c15b81`, `97ea172`, `ad760e5`, `513f656`, `8547a10`, `9ba478f`, `981796a`, `3c29b0c`.

---

## New features

### Dual licensing

The project now ships two licenses at the repository root:

- `LICENSE` — AGPL-3.0-only for the compiler, CLI, codegen, and everything outside the runtime.
- `LICENSE-MIT` — MIT for the runtime, external module sources, and utilities.

Every C/C++ source file carries an SPDX header identifying its license. Hand-written `.c` files (e.g. `runtime/tests/smoke.c`) are stamped MIT; compiler sources are stamped AGPL-3.0-only. Commits `f12b4d9`, `14835ac`, `a10365f`.

### `vjson`

A native JSON module with parsing, serialization, and validation. Ships with a demo (`examples/…/json_demo.vy`) demonstrating error handling and round-trip validation. Commits `eb4da7b`, `cfcec99`, `24e7e9a`.

### `vrand`

Continuous and discrete distributions, sampling utilities, and RNG access, layered on the runtime. A `vrand_demo` example exercises distributions and helpers. Commit `1ac7dd7`, `344b889`.

### `vfft`

An FFT module with a facade for FFT operations. The kernel caches twiddle factors and the bit-reversal table, which is the change that most affects repeated-transform throughput. Ships with a benchmark and a smoke test. Commits `336a3e5`, `80339ba`, `b36749b`, `204512b`, `8e0e2df`.

### `@pool` region policy

`@pool` implements a fixed-slot allocator whose release is O(1) per slot rather than the linear `region.commit` growth used by the default policy. Positive-coverage tests were added and the test output verbosity was tuned. Commits `68c9a17`, `5ff510c`, `aa56396`.

`region.commit_if()` is now supported inside `@speculative` regions, allowing a speculative block to conditionally commit its allocations. Commit `68abf76`.

### Native struct ABI

Struct types now cross the native boundary:

- Field-layout registration and dispatch support for struct-typed native functions. Commit `653f4c2`.
- Struct return types via out-parameters. Commit `62f805f`.
- `last_field_idx` on structures so field access is constant time rather than a scan. Commit `50669ff`.
- Native C struct representation for interface eligibility. Commit `204f05a`.

### VFS module

A Virtual File System module provides read, write, list, and path-manipulation primitives. It supports Windows and Unix-like systems and handles UTF-8 paths. Commit `d671170`.

### vmem peak tracking

The vmem module gains peak-allocation tracking: inspect peak usage, reset the peak, and validate checkpoint states. A dedicated test verifies the behavior. Commits `d671170`, `8afe18c`.

### String builder

A string-builder implementation for efficient string accumulation, avoiding repeated reallocation on append-heavy code. Commit `362d07f`.

### vmath unboxed variants

Trigonometric and logarithmic functions gain unboxed variants for use in numeric kernels where the boxed path is a measurable cost. Commit `1217813`.

### Adam optimizer and fused dense forward pass

`vml` gains an Adam optimizer and a fused forward pass for dense layers. A `fused_check` example verifies the fused pass against the un-fused path and reports the maximum difference. Commit `d703ec5`, `d3d5ad2`.

### Monotonic nanoseconds for benchmarking

`vcore_runtime_now_ns` implements a monotonic nanosecond clock on Windows and Unix-like systems, with an improved memory barrier. `now_ns` is registered in `VCORE_MAP`. This is the timing substrate for the benchmark harness. Commits `947e3f5`, `16e776c`, `c176633`.

### Import syntax: `native` and `external`

Module imports are normalized to distinguish `native` (compiler-provided) from `external` (`.vy` source) dependencies. `vlin` moved from `native` to module syntax; `vml` now uses native `vmath` and external `vlin`. A new import-syntax pattern is enforced across the codebase. Commits `0c83434`, `1d38043`, `b32f8f3`, `b06b28f`, `e44e6e8`.

### CLI

- Terminal color support and help-message formatting improved. Commit `c2b2ef2`.
- Version output in CLI help corrected to reflect the current release. Commit `4f1f53a`.
- Windows executable-path handling normalized in `runTranspile`. Commit `948b5e0`.

### Examples

- `gene_finder` — detects protein-coding DNA using a period-3 spectral signature. Commit `20ef942`.
- Fibonacci example updated with type annotations and improved performance metrics. Commit `4061817`.
- vfft benchmark iteration count and random-float range adjusted. Commit `204512b`.

### Legacy-site tooling

A set of Node scripts under `tools/` extracts and normalizes the legacy HTML site: `extract-legacy.mjs`, `code-normalize.mjs`, `layout.mjs`, `markdown.mjs`, `slug.mjs`, `vyne-highlight.mjs`. Commit `dff5227`.

---

## Bug fixes

### Residual empty-array and collection-handling defects

**Follow-up to the v0.3.0 empty-array fix.** The v0.3.0 release gated the empty-literal fast path on a known element type. The follow-up commits here close the residual cases:

- `ForNode` collect produced an empty result when the loop body's value was boxed. Adjusted the boxed fallback and ensured value emission. Commit `a95fa39`.
- `ForNode` collection handling improved to guarantee proper value assignment. Commit `6c15b81`.
- Empty-array literals in assignments and return statements are handled by the fast path only when the declared element type is known. Commit `97ea172`.
- Tests for empty-array return functions and size checks. Commit `ad760e5`.
- `AssignmentNode` element-type determination streamlined to avoid the previous over-eager inference. Commit `8547a10`.
- Array-type assignment handling refactored to use global declarations. Commit `9ba478f`.
- `reservoir` and `sample_without_replacement` updated to use the generic `Array` type, closing a latent narrowing. Commit `981796a`.
- Residual gap documented in the native `collect` fast path for `Float64` and `Int64`. Commit `3c29b0c`.
- Static type inference in `ASTNode` now handles empty statements. Commit `ee1aefa`.
- `collect_typed_test` added to cover conditional handling in typed arrays. Commit `03816e2`.

### Assignment handling

- Fresh-declaration tracking added to assignment handling, along with interface primitive fields. Commit `daefe61`.
- RHS kind resolution now supports function and method calls. Commit `c65746f`.

### Method-call handling

`MethodCallNode` was refactored into a dispatch chain. Argument handling and type-checking were tightened for `byte_at`, typed-array methods, and string methods; error messages for incorrect argument counts were improved; receiver evaluation was consolidated so boxing is consistent across branches. Commit `946b279`.

### `vyne_out` `fflush`

Removed an unnecessary `fflush(stdout)` from `vyne_out`, eliminating a redundant syscall on every output call. Commit `232d4cb`.

### Struct handling

`structs.h` now returns null for empty structures rather than a valid-looking empty struct. Commit `d671170`.

### Typed-array allocation

Typed-array creation now uses a power-of-two capacity-allocation strategy in `typed_arrays.h`, reducing reallocation on append-heavy workloads. Commit `d671170`.

### `matmul_1024_blas.vy`

Removed an unnecessary newline and cleaned up the output call in the benchmark driver. Commit `90bf692`.

### Matrix multiplication type handling

Matrix multiplication array types now use `Float64` explicitly. Benchmark configuration updated accordingly. Commits `a5ab47a`, `0047b76`.

---

## Runtime and memory

### VFS

See New features above. Commit `d671170`.

### vmem peak tracking

See New features above. Commits `d671170`, `8afe18c`.

### Map entry management

Control-byte sentinels were introduced in `types.h` for efficient map entry management. Commit `d671170`.

### `vcore_runtime_now_ns`

Improved memory-barrier handling, ensuring the monotonic timestamp is correct across cores. Commit `c176633`.

---

## Documentation

- **`vml` README** with usage examples. Commit `a6c6c3a`.
- **`vfft` README** with documentation for the FFT facade. Commit `8e0e2df`.
- **`todo_libraries.md`** rewritten: `vlin`, `vml`, `vbio`, `vfft`, `vjson` marked completed; priority order for upcoming libraries (`vfilter`, `voptim`) set; additional libraries organized by domain with effort estimates. Commit `1091793`.
- **Chemistry and drug-discovery TODO** created. Commit `ebf0d26`; status updates for `byte_at` and native array ABI in commits `6aa7bab`, `ef4ab29`.

---

## Internal changes

### Method-call dispatch chain

`MethodCallNode` is now organized as a chain of dispatch helpers rather than a single branching body. Commit `946b279`.

### Assignment RHS resolution

RHS kind resolution now recognizes function and method calls, not only expression forms. Commit `c65746f`.

### Typed-array capacity

Power-of-two capacity growth in `typed_arrays.h`. Commit `d671170`.

### Struct field layout

`last_field_idx` added to structure layout, letting downstream code emit direct field access instead of scanning the field table. Commit `50669ff`.

### Import syntax normalization

`native` vs. `external` imports enforced across `vlin`, `vml`, and downstream modules. Commits `0c83434`, `1d38043`, `b32f8f3`, `b06b28f`, `e44e6e8`.

### Test coverage

- `map_methods` test suite validating `Map` functionality. Commit `3f2fb77`.
- `collect_typed_test` for conditional handling in typed arrays. Commit `03816e2`.
- `vmem` peak-tracking test. Commit `8afe18c`.
- Positive-coverage tests for `@pool`. Commit `5ff510c`.
- `vfft` smoke test. Commit `80339ba`.

### Vendored binaries

OpenBLAS library binaries (`libopenblas.dll.a`, `libopenblas.exp`, `libopenblas.lib`, `libopenblasp-r0.3.34.a`) and a pkg-config file were added under `vendor/openblas/`. Commit `3afdf30`.

### Unattributed commits

Two commits are titled _"Implement code changes to enhance functionality and improve performance"_ (`2e61202`, `8c7dacc`). Their bodies contain no additional detail. If you still have the working tree, please amend the commit messages so the release note can attribute them; otherwise they will appear in the commit list but not in any category above.

---

## Breaking changes

### Import syntax

The `native` vs. `external` distinction is now enforced. Any `.vy` file that relied on the previous ambiguous import form must be updated. `vlin` is now an external module rather than a native one. See commits `0c83434`, `1d38043`, `b32f8f3`, `b06b28f`.

### `Array` element-type inference

`AssignmentNode` no longer over-eagerly infers an element type for untyped `Array` declarations. Code that relied on the v0.3.0 narrowing (which was itself a temporary behavior) will now get boxed semantics. Add an explicit `Array<Float64>` or `Array<Int64>` annotation if you want the fast path. Commits `8547a10`, `9ba478f`.

> **Note:** the v0.3.0 release note already stated that the silent narrowing was removed. This release tightens the remaining paths — `ForNode`, return statements, `reservoir` — so the changelog entry is a _completion_ of that change, not a new one. If you want the release note to read that way, say so and I'll rewrite it as a single continuity bullet rather than a breaking-change entry.

### Struct returns from empty structures

`structs.h` now returns null for empty structures. Code that assumed a valid, zero-initialized struct from an empty struct literal must handle the null case. Commit `d671170`.

---

## Upgrade guide

1. **Update import declarations.** Replace bare imports with the `native` / `external` form. `vlin` is now `external`.
2. **Add element types to untyped `Array` accumulators.** If your code used `x :: Array = []` and relied on the previous narrowing, add `Array<Float64>` (or `Array<Int64>`) explicitly. The boxed path will still work, but the fast path requires the annotation.
3. **Handle null from empty struct literals.** If you construct a struct with no fields and rely on a non-null pointer, add a check.
4. **Re-run benchmarks** if you compare against v0.3.0; the `vfft` kernel is faster due to twiddle-factor caching, and the `@pool` policy has different memory behavior than the default region.
5. **Confirm the version string in CLI help** matches the tag you publish. Commit `4f1f53a` fixed this once already; verify it again after any further bumps.

---

## Known limitations

- **Native `collect` fast path is incomplete for `Float64` and `Int64`.** Documented in commit `3c29b0c`; the boxed fallback covers the gap but at per-element cost.
- **`vfft` is single-precision only, or unconfirmed.** The commit log does not state the precision or the supported lengths. Please confirm before publishing this section.
- **`@pool` is not a drop-in replacement for the default region.** It imposes a fixed slot count chosen at compile time; oversizing wastes stack, undersizing fails at runtime. See the `@pool` test commits for the covered failure modes.
- **The legacy-site tooling under `tools/` is Windows/Node-only as currently written.** It is not part of the compiler build.

---

## Contributors

- **Tuncay Gafarli** — all commits in this release.

## Full commit list

See `git log v0.3.0..HEAD` for the raw list. The 69 commits this release covers span `4f1f53a` (2026-09-30) through `a10365f` (2026-10-04).

**Full Changelog**: <https://github.com/t2ncay/vyne/compare/v0.0.5...v0.4.0>

---

# Vyne v0.5.0 — declaration syntax, bitwise operators, SIMD `through` loops, and the xmake migration

**Released 2026-10-26**

This release splits declaration from reassignment with the `:=` operator, lands the full bitwise operator family, vectorizes `through` loops with SIMD, extends the native-C-struct ABI to arrays and field writes, migrates the build to xmake on C++23/C++26, removes the Vurage and VServ modules, and introduces the `vchem_common`, `vgraph`, and `vchem` libraries for the chemistry engine. It also ships typed-array region commits, 0-arg native variants, and a refactored monotonic nanosecond clock.

---

## Highlights

- **Declaration vs. reassignment split.** `:=` now introduces a new binding; `=` performs reassignment. Parser, codegen, standard libraries, and benchmarks were migrated. Commit `fc2c6d2f` (#89).

- **Bitwise operators, landed.** Lexer, parser, codegen, tests, and precedence documentation for `&`, `|`, `^`, `~`, `<<`, `>>`. `vfft`'s `Kernels.vy` was refactored to use them. Commits `32057998` (#84), `40fea384` (#85), `fd0debf2`, `d34ff780`, `11cb029b`.

- **SIMD vectorization for `through` loops.** `__restrict__` pointer annotations, boxed-interface pathways fixed under SIMD flags, invariant field-load hoisting, and a relaxed SIMD-safety shape check. Commit `7824f0cf` (#90).

- **Typed-array region commits, 0-arg native variants, and monotonic ns clock.** Commit `b92ceb35` (#92).

- **Native C structs, completed across the ABI.** Struct arrays, field-write handling on native receivers, boxing helpers for arrays of native structs, assignment/member-access handling, and C-eligibility checks for interface representation. Commits `110869ee`, `8ced75d5`, `61dac080`, `b4dfe1e8`, `d2dc30fe`, `0b94a737`.

- **Build system migration.** The project now builds with **xmake** on C++23/C++26, with platform-aware dependency handling, system raylib detection, and platform-specific math-library linking. Commits `687a6eff`, `8e090118`, `038683cb`, `01e834da`, `9e407529`, `3341ed38`.

- **Module removals.** The legacy **Vurage** runtime module and the deprecated **VServ** server module were removed in full. Commits `57197dfc`, `5a6b4788`, `8ac8bfb3`.

- **Chemistry engine libraries.** `vchem_common`, `vgraph`, and the compiler support needed by `vchem` (nested friend-library parsing, mangled-name constructor resolution) land. Commits `a5f104de`, `11e49099`, `953623b8`.

---

## New features

### Language syntax: `:=` for declarations, `=` for reassignment

The parser now distinguishes binding a new name (`:=`) from rebinding an existing one (`=`). `vfft`, `vml`, and the `matmul_1024_vlin.vy` benchmark were migrated. Commit `fc2c6d2f` (#89).

### Bitwise operators

Lexer, parser, and codegen support for `&`, `|`, `^`, `~`, `<<`, `>>`. `vfft`'s `Kernels.vy` was refactored to use them (`fd0debf2`), a precedence table was documented (`d34ff780`), and guidance on scratch values and bitwise operators was added (`11cb029b`). Tests ship with the implementation (`32057998`, `40fea384`).

### SIMD vectorization for `through` loops

`through` loops are now eligible for SIMD lowering. The change adds `__restrict__` pointer annotations, fixes boxed-interface pathways under SIMD flags, hoists invariant field loads out of the loop, and relaxes the SIMD-safety shape check. Commit `7824f0cf` (#90).

### Typed-array region commits and 0-arg native variants

Typed arrays participate in region commits; native functions may be declared with zero arguments. The monotonic nanosecond clock was extended in the same commit. Commit `b92ceb35` (#92).

### Codegen: native booleans and `Int64` typed-array size

Conditions are emitted as native booleans, and typed-array sizes are emitted as `Int64`. Commit `839ba7ff` (#87).

### BLAS matmul with transposed first operand

A new wrapper for `C = Aᵀ · B` was added, with a BLAS path that honours the transpose flag. Commits `a2bc3339`, `6b80064b`.

### Native C struct expansion

- Struct arrays: `110869ee`, `211ec2b9`, `61dac080`, `0b94a737`.
- Field-write handling on native receivers: `8ced75d5`.
- Assignment and member-access handling for native structs: `b4dfe1e8`.
- C-eligibility checks for interfaces: `d2dc30fe`.
- Boxing helpers for arrays of native structs: `61dac080`.

### Chemistry engine: `vchem_common`, `vgraph`, `vchem`

New external libraries for the chemistry engine, plus compiler support for **nested friend-library parsing** and **mangled-name interface constructor resolution**. Commits `a5f104de`, `11e49099`, `953623b8`.

### Version and build hardening

Robust versioning in the entry file (`3fd861ce`), the website version mismatch fixed (`45523f5c`, closes #88), xmake `sys_link()` OS modulations (`9e407529`), platform-specific math linking (`01e834da`), and an updated compilation database for core std libraries (`330f76fc`).

### Examples and tooling

- `cast_test` — array manipulation and memory management (`ada3b20c`).
- `interface_test` — Point array handling, Point property mutation, and Molecule/Atom regression coverage (`02996419`, `633d1ec9`, `211ec2b9`, `0b94a737`, `73d81c14`).
- `dumm_test` — module-scoped function definitions (`6533662f`).
- `function_test` — consolidated `add` function (`2cd9c4f9`).

---

## Bug fixes

### Parser and type resolution

- Symbol-table type now propagates to plain variable reads. Commit `441b28db`.
- Type resolution in `vml` fixed. Commit `394abfc9`.
- Nested friend-library parsing in `vchem` fixed. Commit `11e49099`.
- Mangled-name interface constructor resolution fixed. Commit `953623b8`.
- File-include error resolution in the parser fixed. Commit `7b6b9030`.

### Bitwise tokenizer

Typo fix in the tokenizer's bitwise handling. Commit `40fea384` (#85).

### Version output

Website version mismatch corrected. Commit `45523f5c` (closes #88).

### BLAS

Transposed-first-operand support corrected in the BLAS matmul path. Commits `a2bc3339`, `6b80064b`.

### Native C structs

Field-write paths, member access, and array boxing for native structs corrected across the Oct 4 cluster (`b4dfe1e8`, `8ced75d5`, `61dac080`, `110869ee`).

### Codegen build

C++ build repaired after scratch/region include changes (`<cstdint>` added to `ctype.h`; invalid `throw` placeholder replaced). Commit `b92ceb35` (#92).

---

## Runtime and memory

### Monotonic ns clock

The clock's memory-barrier handling and 0-arg native variant are in place; `vcore_runtime_now_ns` remains the benchmark substrate. Commit `b92ceb35` (#92).

### Typed-array region commits

Typed arrays now participate in region commits, with the corresponding codegen path in place. Commit `b92ceb35` (#92).

### Native C struct arrays

Arrays of native C structs are supported in the runtime and codegen, with boxed fallbacks where needed. Commits `110869ee`, `61dac080`.

### `vmath` native mapping

Native mapping for `vmath` (and, in a follow-up, `vmath` + `vcore`) was adjusted as part of the `#92` cluster. Commit `b92ceb35` (#92).

### Module removals

The Vurage runtime module (`pager.h`, `storage.*`, `type.c`, `vurage.cpp`, `vurage.h`) and the VServ server module (`vserv_*.cpp`) were removed; `xmake.lua` was cleaned of raylib and OpenSSL references and moved to C++23. Commits `57197dfc`, `5a6b4788`, `7532bec7`, `8ac8bfb3`.

---

## Documentation

- Bitwise operators and precedence table. Commit `d34ff780`.
- Guidance on scratch values and bitwise operators. Commit `11cb029b`.
- Residual-gap sections formatted for clearer type description. Commit `dc2d9396`.
- Inline-comment pass. Commit `18e112d9`.
- Dataset links for future reference. Commit `c55c540b`.
- Installation build commands split (#86). Commit `0b12ca61`.
- README updated to reflect C++26 and xmake. Commit `3341ed38`.
- Drug-discovery TODO status updates for native C structs and Point arrays. Commits `f8bee632`, `a3892f8d`, `7b2017cd`, `cc1400c1`.

---

## Internal changes

### Build system

- xmake project script with configuration and testing (`687a6eff`).
- Improved platform detection and dependency handling (`8e090118`).
- C++23 switch and system raylib detection (`038683cb`).
- OS-specific `sys_link()` dependencies (`9e407529`).
- Platform-specific math linking (`01e834da`).
- `.gitignore` updates for `.json`, `.xmake`, `build/`, and `.VSCodeCounter` (`e5c2e465`, `2ce50bdd`).

### Clang tooling

- `clangd` formatting flags (`ef048d6b`).
- `.clang-format` (`9108020c`).
- clang compiler parameters and formatting (`e4933714`, `6d828b51`).
- Compilation database for core std libraries (`330f76fc`).

### Codegen and parser

- `__restrict__` annotations for SIMD (`7824f0cf`).
- Native booleans and `Int64` typed-array size (`839ba7ff`).
- Linker AST specifications commented for codegen (`3d772e4c`).
- `:=` split in parser, codegen, and libraries (`fc2c6d2f`).

### Cleanup

- Vurage shared library and includes removed (`5a6b4788`).
- VServ module removed (`8ac8bfb3`).
- `dynamic_casting` ruleset removed from `function_test` (`7532bec7`).
- Redundant combined modules file removed (`b92ceb35`).
- `_expect` newline removed (`00da256b`).
- `interface_test` redundant output statements removed (`73d81c14`).
- `vfft` `Kernels.vy` refactored for bitwise operators (`fd0debf2`).
- `matmul_1024_vlin.vy` migrated to `:=` (`fc2c6d2f`).

### Tests

- Bitwise tests (`32057998`, `40fea384`).
- `interface_test` Molecule/Atom regression suite (`0b94a737`).
- `cast_test` (`ada3b20c`).
- SIMD-shape safety coverage (`7824f0cf`).
- Typed-array region commit coverage (`b92ceb35`).
- 0-arg native variant coverage (`b92ceb35`).
- Safety-test variable initializers updated (`b92ceb35`).

### Version badge

README version badge bumped to `0.4.0` (`eca6b27e`); the robust entry-file versioning (`3fd861ce`) and the website-mismatch fix (`45523f5c`) follow.

---

## Breaking changes

### Declaration operator

`:=` now declares; `=` reassigns. Any `.vy` file that previously declared with `=` must be updated. The parser, codegen, `vfft`, `vml`, and the `matmul_1024_vlin.vy` benchmark were migrated in-tree. Commit `fc2c6d2f` (#89).

### Module removals

The Vurage runtime module and the VServ server module are gone. Downstream code that imported either must be ported or replaced. Commits `57197dfc`, `5a6b4788`, `8ac8bfb3`.

### `dynamic_casting` ruleset

Removed from `function_test` and any dependents. Commit `7532bec7`.

### Bitwise precedence

The precedence table is now documented and enforced; code that relied on the previous (unstated) behaviour should be re-checked against the table. Commit `d34ff780`.

### Typed-array region commits

Typed arrays now participate in region commits; allocation and commit behaviour changes for code that relied on the previous non-participating path. Commit `b92ceb35` (#92).

---

## Upgrade guide

1. **Adopt `:=` for declarations.** Replace `x = expr` at first binding with `x := expr`. Keep `=` only for reassignment.
2. **Drop Vurage and VServ.** Replace any imports with the new equivalents, or remove the dependency.
3. **Re-check bitwise precedence.** Cross-reference the table in commit `d34ff780` and update any expression whose intent depended on implicit precedence.
4. **Re-run SIMD-sensitive `through` loops.** The SIMD-safety shape check was relaxed, but `__restrict__` annotations changed pointer aliasing assumptions.
5. **Re-benchmark typed-array code.** Typed-array region commits and `Int64` sizes change allocation and commit behaviour.
6. **Add `vchem_common` / `vgraph` to the build graph** if you consume the chemistry engine; nested friend-library parsing and mangled-name constructor resolution are now expected by the parser.
7. **Rebuild with xmake.** The pre-xmake build files are superseded; C++23/C++26 is now the supported standard.
8. **Verify the version string** in CLI help after publishing (see `3fd861ce` and `45523f5c`).

---

## Known limitations

- **`vchem` nested-friend parsing is new.** Only the patterns exercised by the chemistry engine are covered; deeply nested friend graphs may still surface parser errors.
- **SIMD `through` loops are shape-checked, not proven.** The relaxed safety check widens eligibility; correctness for highly irregular strides should be re-tested.
- **0-arg native variants** are supported at the call boundary; overload resolution among multiple 0-arg variants is not documented.
- **Vurage/VServ removal is one-way.** Any downstream consumer needs an explicit migration path; no compatibility shim ships.
- **`vfft` precision and supported lengths** remain unconfirmed from v0.4.0 and are still not stated in the commit log.
- **Bitwise precedence table is documentation-only in this release.** Enforcement is via parser order; a formal grammar update is pending.
- **`vmath` / `vcore` native mappings changed twice** in the `#92` cluster; code that relied on the earlier mapping should be re-validated.

---

## Contributors

- **Tuncay Gafarli** — the majority of commits in this release.
- **Agil Mammadov** — installation-doc split (#86, `0b12ca61`).
- **f3rhd** — bitwise lexing/parsing/codegen and the tokenizer typo fix (#84, #85; `32057998`, `40fea384`).

## Full commit list

See `git log v0.4.0..HEAD` for the raw list. The 66 commits this release covers span `c65a1e45` (2026-10-04) through `3d772e4c` (2026-10-26).

**Full Changelog**: <https://github.com/t2ncay/vyne/compare/v0.4.0...v0.5.0>

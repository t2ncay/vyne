# Changelog

## Unreleased — automatic differentiation surface + straight-line reverse mode

This change lands the B + A AD surface recommended for Vyne:

- `@differentiable` on a function marks it as having a compiler-generated
  adjoint. The C backend emits the primal unchanged and a
  `fn_<name>_grad(...)` companion.
- `@backward` on a region creates the tape scope. It is a predicate that
  composes with `@bounded`, `@recompute`, and `@mixed`; the memory policy is
  still dispatched by `RegionNode::compile`.
- `@implicit` composes with `@backward` and the memory policies. Inside it,
  `newton_solve(F, x..., z0)` solves the scalar residual `F(x..., z) = 0`,
  and `backward(root)` applies the Implicit Function Theorem
  (`dz/dx_i = -(dF/dx_i)/(dF/dz)`) at the converged root. The reverse pass
  never stores or replays solver iterations; `backward(root, seed)` chains an
  explicit upstream sensitivity. The residual must be a scalar
  `@differentiable` function whose last parameter is the state. Desktop only
  in this release; vector residuals and Jacobian solves are deferred with
  `VNE-127` / `VNE-128`.
- `backward(loss)` seeds the adjoint sweep and populates `w.grad` for the
  recorded arguments. The gradient slots are ordinary C locals inside the
  region block, so they die at region rewind and are covered by `@bounded`.
- `grad(f, x)` is expression-level sugar for the same seeded sweep, currently
  for unary `Float64` functions.

The reverse-mode generator (`codegen/autodiff.cpp`) recomputes the primal
intermediates, seeds the returned expression, walks assignments in reverse, and
publishes input gradients through out-parameters. It supports `+ - * / **`,
`sin cos exp log sqrt tanh abs relu`, and `pow` over straight-line `Float64`
bodies. Everything outside the subset is rejected with `VNE-120`..`VNE-128`
instead of silently miscompiling.

Array gradients are supported for `Float64 -> Float64` functions that reduce
`Array<Float64>` values through the `vstat`/`vlin` kernels:

- `grad(f, w)` returns a fresh `Array<Float64>` when `w :: Array<Float64>`;
- the VJPs for `mse`/`k_mse_sum`, `mae`/`k_mae_sum`, `dot`/`k_dot`, `sum`,
  `mean`, `norm_sq`/`sum_sq`, and `abs_sum` are emitted as loops over the
  native array storage and accumulate into the caller-provided out container;
- array arguments are plain variables (parameters or read-only globals);
  array locals, element-wise intermediates, and matmul VJPs are deferred;
- `backward(...)` on array parameters is deferred (use `grad(f, x)`).

Also included:

- region attributes may now compose (`@backward @bounded<...>`), with
  `@recompute` / `@mixed` parsed and recorded for a later phase;
- numeric literals accept `_` digit separators (`64_000_000`);
- examples under `examples/autodiff/` (including `array_grad.vy` and
  `implicit_test.vy`) and a
  reference page at `docs/tutorials/reference/autodiff.md`.

---

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

---

# Vyne v0.6.0 — region policies, CUDA + Float32, the Arduino Uno backend, and `vynec init`

**Released 2026-11-09**

This release turns the region memory model into a policy suite (`@bounded`, `@zeroed`, `@deterministic`, `@ring`, `@streamed`, `@persistent`, `@slab`), makes `Float32` a first-class type and lands the CUDA `@device` backend with FP32 as the default device dtype, ships five scientific standard libraries (`vstat`, `vtermplot`, `vcomplex`, `vinterp`, `vcsv`) plus the image stack (`vimage`, `vconv`, ESPCN inference), adds the first non-desktop target (the Arduino Uno AVR backend) with a 91-function MCU runtime, and introduces `vynec init` with an interactive FTXUI questionnaire and five project templates. It also hardens `vlin`, adds typed-array alignment and codegen peepholes, and closes a cluster of region and MCU codegen defects.

---

## Highlights

- **Region policy suite.** Seven policies and a qualified commit syntax. `@bounded<N>`, `@zeroed`, and `@deterministic` land with `<regionName>.commit(x)` / `.commit_if(pred)`; `@ring<T, N, MAX>`, `@streamed<T, MAX>`, `@slab<T, MAX[, CAP]>`, and `@persistent` follow. Commits `3837317` (#97), `a6abd34` (#99), `80a0e74` (#100), `a10425a` (#101), `a684ed0` (#102).

- **First-class `Float32` and the CUDA `@device` backend.** `Float32` spans the lexer, parser, interpreter, tagged values, typed arrays, native ABI, and codegen. `@device<backend>` runs regions on a device arena with a dynamic CUDA driver backend, a CPU fallback, and FP32 as the default. Measured on an RTX 5060 + i7-14700: 512×512 FP32 matmul in 167 µs (1607 GFLOP/s) versus OpenBLAS `sgemm` at 626 µs (429 GFLOP/s). Commit `9c8275e` (#111).

- **Five scientific libraries.** `vstat` (descriptive statistics, correlation, regression, histograms, streaming), `vtermplot` (terminal line/scatter/bar/histogram/heatmap/sparkline), `vcomplex` (scalar and split-array complex arithmetic), `vinterp` (1-D and 2-D interpolation), and `vcsv` (RFC 4180 CSV/TSV). Each ships a README, a test, and a benchmark. Commits `3c8d8d4` (#104), `53ec847` (#105), `f7e7984` (#107), `ee81de9` (#108), `28a0a40` (#109).

- **Image and super-resolution stack.** `vimage` (stb-backed decode/encode, resize, crop, grayscale) and `vconv` (OIHW `conv2d`, activations, ESPCN pixel shuffle, weight-bundle loader), plus `vstat` metrics (`mse`, `mae`, `psnr`, `ssim`), an end-to-end ESPCN inference example, and a PyTorch weight exporter. Commit `0dfb939` (#112).

- **Arduino Uno AVR backend.** The first non-desktop target: a self-contained MCU runtime (1 KB bump arena, 16-deep checkpoints, hard-halt OOM), 91 functions across 10 headers, typed-array containers, `region`/`scratch` lowering, `@bounded` on MCU, and single-command transpile → avr-gcc → flash → serial monitor. Commits `f3fc3bf` (#95), `1c8a703` (#96), `f4af3e9`, `2f78387`, `23a3de7`, `5a803e9`.

- **`vynec init`.** A dependency-free template engine, an interactive FTXUI 7.1.0 questionnaire, and five templates (blank, ML, embedded, scientific, SR). Generated ML/scientific/SR projects build and run offline. Commits `681e7e0` (#113), `6ff43e1` (#114), `0ddbf0a`, `31bc169` (#115).

---

## New features

### Region policies

#### `@bounded`, `@zeroed`, `@deterministic`, and qualified commits

`@bounded<N>` brackets a region with arena accounting and halts on exit if the region exceeded `N` bytes (VNE-101 at compile time, VNE-102 at runtime). `@zeroed` sets `g_arena_zero_mode`, so every allocation inside the region is zero-filled, and static scratch is emitted as `= {0}` with a `memset` for VLAs. `@deterministic` is `@zeroed` plus an AST scan that rejects calls to `vcore.now`/`now_ns`/`input`, `vmath.random`/`random_float`/`seed`, and `arduino.millis`/`micros`/`random`/`analog_read*` (VNE-106).

The parser recognizes `<regionName>.commit(x)` and `<regionName>.commit_if(pred)` only when the qualifier names the innermost open region; committing across a nesting level fires VNE-086. Commit `3837317` (#97).

`@bounded` accounting was later extended to include static scratch and runtime-sized VLAs, not just arena traffic: static scratch is counted at compile time (`applyBoundedStaticScratch`) and VLAs are checked per declaration before the stack pointer advances. Commit `73fb29a`.

#### `@ring<T, N, MAX>`

A rotating-slot allocator: `N` fixed slots backed by one `N*MAX` region allocation. `ring_alloc(n)` returns the next slot, zero-fills the requested elements, and advances the cursor; after `N` allocations the oldest slot is overwritten. There is no `ring_free` — the region rewind reclaims the backing store. Commit `80a0e74` (#100).

#### `@streamed<T, MAX>`

A pinned-memory frame for host/device transfer staging. The runtime uses `VirtualAlloc` + `VirtualLock` on Windows and `mmap` + `mlock` elsewhere, falling back to `malloc` where locking is unavailable; lock success is reported by `vmem.stream_is_pinned`. `streamed_alloc(n)` zero-fills at `VYNE_TYPED_ARRAY_ALIGN` and enforces a total element budget (VNE-082). Nested frames reclaim independently, and every early-return path unwinds the stream frame first. Commit `a10425a` (#101).

#### `@persistent`

Routes a region body's allocations to `g_commit_arena`, so values survive ordinary region rewinds for the whole program lifetime. The outermost `enter` saves the enclosing arena and the outermost `leave` publishes the advanced commit-arena pointer; nested `@persistent` regions share storage. `region.commit(x)` inside `@persistent` returns its argument unchanged (already in the commit arena), and persistent regions are excluded from escape checks (VNE-070). Desktop-only. Commit `a684ed0` (#102).

#### `@slab<T, MAX[, CAP]>` and typed-array alignment

`@slab` is a power-of-two size-class allocator: lazy per-class backing from the region arena, O(1) `slab_alloc(n)` / `slab_free(x)`, zero-fill on allocation, and double-free/wrong-region diagnostics. Typed-array payloads (create, push reallocation, slice) now come from `arena_alloc_aligned(..., VYNE_TYPED_ARRAY_ALIGN)`, defaulting to 32 bytes and overridable with `-DVYNE_TYPED_ARRAY_ALIGN=64`. Commit `a6abd34` (#99).

#### Codegen peepholes

Identity folds remove `x - 0`, `x * 1`, `1 * x`, `x / 1`, and (Int64 only) `x + 0`; the Float64 `+ 0` case is deliberately not folded because `-0.0 + 0.0` flips the sign of zero. Division and modulo by a non-zero numeric literal skip the runtime divide-by-zero branch, and a literal zero divisor is now a compile-time error. Commit `a6abd34` (#99).

#### Poison-on-rewind (debug mode)

When enabled, `arena_rewind()` and `arena_try_reclaim()` overwrite released memory with a quiet-NaN word (`0x7FF8DEADBEEFCAFE`) and an `0xA5` byte remainder, so a stale read after a rewind yields NaN instead of a plausible leftover. Off by default; enable with `VYNE_POISON_ARENA=1`, `vyne_arena_set_poison(1)`, or `-DVYNE_ARENA_POISON_DEFAULT=1`. Desktop-only. Commit `793ec9f` (#98).

### `Float32` and the CUDA `@device` backend

`Float32` is now a real `VType` across the compiler and runtime. Literals use the `1.0f` / `2f` / `1e3f` forms, and the `float32(x)` builtin converts explicitly. `Array<Float32>` lowers to `VyneArray_f32` / `V_F32_ARRAY`, scalars use `V_FLOAT32`, native functions can take or return `Array<Float32>` as `float*` / `VyneArray_f32`, and struct fields, `type()`, `string()`, `push`/`index`/`slice`/`pop`/`sort` all handle the type. Promotion is sticky: `Float32` op `Float32` stays `Float32`; mixing with `Float64` or `Int64` promotes to `Float64`.

`@device<backend>` opens a device frame, runs the body, closes the frame, then rewinds the host arena; early returns and native return paths unwind the device frame first. The intrinsics `device_alloc`, `device_alloc_f64`, `upload`, `download`, `add`, `sub`, `mul`, `scale`, `relu`, `fill`, `matmul`, `sync`, `bytes`, `peak_bytes`, `backend`, and `available` are intercepted only inside an active device region. Host buffers must be typed `Array<Float32>` / `Array<Float64>`; handles are `Int64` device pointers. `device_alloc(n)` allocates `Float32` by default. CUDA kernels are compiled to PTX by `scripts/build_cuda_ptx.ps1`, embedded in `detail/vyne_cuda_ptx.h`, and JIT-compiled by the driver, so generated C needs no CUDA headers or link flags. `@device` is rejected on AVR. Commit `9c8275e` (#111).

### Standard libraries

#### `vstat`

A full descriptive-statistics and least-squares library layered as Types/Kernels/Reductions/Correlation/Regression/Histogram/Streaming/Metrics leaves plus a facade. The surface includes `Series { data, n }` for zero-copy native calls, `sum`/`mean`/`min`/`max`/`range`, variance/stddev, median/quantile/percentile/IQR/MAD/mode, z-scores, cumulative sums, differences, covariance, Pearson/Spearman, autocorrelation, cross-correlation, linear/multiple/ridge regression, histograms/CDF/KDE, and Welford one-pass state. Commit `3c8d8d4` (#104). `mse`, `mae`, `psnr`, and `ssim` were added by `0dfb939` (#112).

#### `vtermplot`

A matplotlib-flavoured terminal plotting library with line, scatter, bars, histogram, heatmap, and sparkline surfaces; Unicode and ASCII themes, optional ANSI color, axes/labels, and file export. `SeriesData` carries typed x/y arrays, rendering is string-based, and sparklines use the `vcore` string builder for O(n) output. Commit `53ec847` (#105).

#### `vcomplex`

Scalar complex arithmetic (`add`/`sub`/`mul`/`div`/`scale`/`neg`/`conj`/`abs`/`abs_sq`/`arg`/`equals`), elementary functions (`exp`/`log`/`sqrt`/`pow`/`sin`/`cos`/`tan`), polar conversion and rotation, and element-wise operations over split `(re, im)` arrays. Element-wise kernels are native leaves over `Array<Float64>`; `ComplexArray` wrappers allocate the output and make one native call. Commit `f7e7984` (#107).

#### `vinterp`

1-D interpolation over sorted curves (nearest, linear with extrapolation, clamped linear, Lagrange, natural cubic spline, resampling) and 2-D interpolation over row-major grids (nearest, bilinear, Catmull-Rom bicubic, fractional-index and physical-coordinate wrappers). Kernels inline their binary search so they remain leaves under the native array ABI. Commit `ee81de9` (#108).

#### `vcsv`

RFC 4180-style CSV/TSV parse and write with quoted fields, doubled quotes, embedded delimiters/newlines, CRLF input, optional headers, flat row-major storage, typed-column extraction, column selection, serialization, and vfs-backed file I/O. The parser scans bytes with `String.byte_at` and appends with the new `vcore.sb_append_char`, eliminating per-character string allocation; a 274 KB / 10k-row parse dropped from ~2270 ms to ~1 ms. A cross-library integration test chains `vcsv` → `vstat` → `vinterp` → `vcomplex` → `vtermplot`. Commit `28a0a40` (#109).

#### `vimage` and `vconv`

`vimage` wraps vendored `stb_image` / `stb_image_write` for PNG/JPEG/BMP/TGA decode and encode, with bilinear resize, clamped crop, and Rec.601 grayscale; pixels are HWC `Float64` in `[0,1]`, and `load_raw` returns a `[width, height, channels, samples...]` header stripped by an O(1) `pixels()` view. `vconv` is the CPU forward convolution core: square-kernel `conv2d` with stride and zero padding, PyTorch OIHW weight layout, optional bias, ReLU/leaky-ReLU/tanh/sigmoid, ESPCN pixel shuffle, and a `.vwy`/`.vwt` weight-bundle loader. Both are wired into `native_maps.h` and `C_Emitter::findNative`. Commit `0dfb939` (#112).

`examples/espcn_inference.vy` is the first end-to-end image SR program (decode → three conv layers → activation → pixel shuffle → encode, with PSNR/SSIM against a ground truth and a bilinear baseline), and `scripts/export_espcn_weights.py` exports PyTorch state dicts (or a deterministic passthrough model with `--random`) into the binary `.vwy` format.

### Arduino Uno AVR backend

#### Backend and MCU runtime

`vynec --arduino uno --arduino-flash examples/arduino/blink.vy` transpiles, builds against the MCU runtime with avr-gcc, and produces `.elf` / `.hex` ready for `arduino-cli upload`. The MCU runtime is a separate, self-contained implementation (it does not reuse `vyne_runtime.h`): `vyne_arduino.h` owns `setup()` / `loop()` and calls the emitted `vyne_setup()` / `vyne_loop()`; `detail/mcu_arena.h` provides a 1 KB bump allocator with 8-byte alignment and a 16-deep checkpoint stack, halting hard on OOM; `detail/mcu_types.h` mirrors the desktop `VyneValue` discriminant layout; `detail/mcu_hardware.h` is the only file that touches the Arduino API. `C_Emitter` gained `arduinoTarget_` / `isArduinoTarget()`, a two-branch `finalize()`, and the `arduino` pseudo-module. Commit `f3fc3bf` (#95).

The `region`/`scratch` model was then brought to AVR: `vmem_runtime_checkpoint` / `vmem_runtime_rewind` alias the single MCU arena, `vyne_panic_bounds` replaces `fprintf(stderr) + exit` for VNE-072, `VYNE_UNLIKELY` keeps bounds branches identical across targets, and `arduino.micros`/`sin`/`cos`/`sqrt`/`delay_microseconds`/`arena_used` are registered. The `nativeInit` path now binds boxed property getters to an `ni_N` temp before the union-tag check, removing a double evaluation. Commit `1c8a703` (#96).

#### 91-function MCU runtime

The monolithic `mcu_hardware.h` was split into one self-contained header per concern (each pulls only from `<Arduino.h>` and avr-libc): `mcu_serial.h` (7), `mcu_timing.h` (2), `mcu_math.h` (34), `mcu_pins_ext.h` (9), `mcu_memory.h` (4), `mcu_bits.h` (9), `mcu_eeprom.h` (4), and `mcu_interrupts.h` (8). `mcu_types.h` gained `g_vyne_adc_shift` / `g_vyne_pwm_bits` so `analog_read` / `analog_write` honour their resolution setters, and the EEPROM bindings were reimplemented against `<avr/eeprom.h>` instead of the bundled Arduino library. Commit `f4af3e9`.

#### Typed arrays on the MCU runtime

`VyneArray_f64`, `VyneArray_i64`, and the untyped `VyneArray` fallback now exist in `mcu_types.h`, matching the symbols the emitter already produces (create, push, slice, `to_value`, `value_to_array_*`, plus `vyne_array_create`/`set`/`push`/`get`/`_vyne_array_size`). Typed containers are 4 bytes per `Float64` element (8 per `Int64`) versus 16 per boxed `VyneValue`; boxing is a pointer pun that relies on the shared size/cap offsets, and push grows by 1.5× above 8 elements from the arena. Commit `2f78387`.

#### TinyML XOR and reactive demos

`examples/arduino/tinyml_xor.vy` is a 2→3→1 MLP learning XOR live on the board with a source-level LCG for reproducibility, ReLU + MSE to avoid transcendentals, `region step` / `region probe` blocks, and a hardware math self-test. It converges (loss 0.20 → 0.09 over 30k steps) to 91% on 10 held-out samples; the plateau is a documented ReLU dead-unit artifact, not a runtime bug. `examples/arduino/reactive.vy` blinks the built-in LED according to whether the arena grew and uses SOS to signal a rewind failure. `examples/arduino/typed_arrays.vy` exercises empty-literal push growth, literals, indexing, slicing, and region rewind. Commits `f4af3e9`, `1c8a703`, `2f78387`.

#### Arduino CLI and codegen fixes

The CLI gained `--arduino`, `--flash`/`--arduino-flash`, `--monitor`, `--port`, `--baud`, and `--fqbn`, driving the full transpile → avr-gcc → upload → monitor pipeline. Commit `5a803e9`. Hex literals (`0x...`) are accepted by the lexer (`ced54ff`), and `UnaryNode::getCExpr` now emits native operators for literals and provable-native expressions instead of the desktop-only `vyne_unary` call, fixing every unary minus on MCU and shrinking desktop output (`4332172`, `f4af3e9`). Serial `begin` and printing were fixed for debugging (`c6cf264`).

### `vynec init` project initializer

Milestone 1 added a dependency-free template engine supporting `{{var}}`, `{{#if var}}...{{/if}}`, `{{#each list}}...{{/each}}`, and `{{.}}` / `{{@index}}`, with unknown variables rendering as the empty string and no partials, expression evaluation, or escaping. The `init` command generates `project.toml` plus `src/main.vy`, `README.md`, `.gitignore`, and `LICENSE`; it refuses to overwrite an existing `project.toml` (exit 3), skips other existing files, initializes and commits a git repository when appropriate, and writes complete toolchain/dependency/target/build/test/bench sections. Flags: `--name`, `--kind`, `--targets`, `--libraries`, `--cuda`, `--license`, `--yes`, `--no-tui`, `--no-git`, `--self-test`. Commit `681e7e0` (#113).

Milestone 2 replaced the placeholder with an FTXUI 7.1.0 questionnaire: five questions (kind, targets, libraries, conditional CUDA, license), recommended defaults per kind, single-select bullets and multi-select checkboxes, back/cancel navigation, a confirmation screen that lists the files to be created, an 80×24 minimum, and a clean exit 2 in non-TTY environments before entering the alternate screen. Milestone 3 added the five templates — `blank`, `ml` (4→16→3 MLP on UCI Iris, `vcsv` loader, Adam + SGD, `vtermplot` loss curve, `@bounded<1048576>`), `embedded` (Arduino Uno blinky with `@bounded<512>` and `hardware.toml`), `scientific` (RK4 Lorenz + Lyapunov estimate, `@bounded<16777216>`), and `sr` (ESPCN 2× inference with bundled sample/reference PNGs and an exported `.vwy` model). `mixed` maps to the ML template with every library. Commit `6ff43e1` (#114).

`vynec init` later gained derived-name sanitization (spaces and non-identifier characters become `_`, a leading digit or `-` gets a `_` prefix, fallback `vyne_project`), so directories such as "Coding projects" no longer block the TUI. Commit `0ddbf0a`.

### `vlin` hardening

Every public `vlin` operation now throws on shape mismatch instead of returning a `0x0` (or `-1`) matrix that propagates NaN. New element-wise primitives: `abs`, `sqrt`, `exp`, `log`, `pow_scalar`, `pow_elementwise`, `max_elementwise`, `min_elementwise`. New in-place variants for the backward pass: `sub_into`, `hadamard_into`, `div_into`, `scale_into`, `copy_into`, `transpose_into`, alongside `add_into` / `multiply_into`. `add_bias` validates the bias length, and `Matrix.reshape` now throws on size mismatch. Commit `e16f3e4`.

### Language and codegen

- **Hex literals.** Hexadecimal integer literals are accepted by the lexer. Commits `ced54ff`, `6077f22`, `28ca465`.
- **Native unary fast paths.** Literals, booleans, and provable-native expressions lower to raw C operators. Commits `4332172`, `f4af3e9`.
- **Floating-point literal form preserved.** `floatLit` now appends `.0` when a whole-valued double would otherwise print as an integer, and maps `inf`/`nan` to `INFINITY`/`NAN`; this fixes `32.0 / 7.0` silently becoming integer division. Commit `3c8d8d4` (#104).
- **`Float32` unary and scratch support.** Native fast paths for unary minus and logical not, plus `Float32` scratch arrays. Commit `9c8275e` (#111).

### Examples and tooling

- `alphafold_distance_map.vy` — runtime-dim scratch, new scratch reductions, `.copy_to()` into a typed array. Commit `c5a8de2` (#94).
- `earthquake_model.vy` — decision-tree model modernized. Commit `13e38e7`.
- Library tests and benchmarks: `vstat_test` / `vstat_bench`, `vtermplot_test` / `vtermplot_bench`, `vcomplex_test` / `vcomplex_bench`, `vinterp_test` / `vinterp_bench`, `vcsv_test` / `vcsv_bench`, `libraries_integration_test`, `vimage_test`, `vconv_test`, `vstat_metrics_test`, `vconv_bench`. Commits `3c8d8d4`, `53ec847`, `f7e7984`, `ee81de9`, `28a0a40`, `0dfb939`.
- Device tests: `gpu_elementwise.vy`, `gpu_matmul.vy`, `device_test.vy`, `device_matmul_bench.vy`. Commit `9c8275e` (#111).
- Region tests: `region_rules_smoke.vy`, `region_ring.vy`, `region_slab.vy`, `region_streamed.vy`, `region_persistent.vy`, `codegen_peepholes.vy`, and the safety files `region_early_exit_zeroed.vy`, `region_early_exit_bounded.vy`, `region_scratch_over_bound.vy`, `region_vla_over_bound.vy`. Commits `3837317`, `73fb29a`, `eb236d7`, `a6abd34`, `80a0e74`, `a10425a`, `a684ed0`.
- Arduino demos: `reactive.vy`, `tinyml_xor.vy`, `typed_arrays.vy`. Commits `1c8a703`, `f4af3e9`, `2f78387`.
- The SR project template was updated with a CUDA-accelerated ESPCN path (Float32 conversion, transposed weights, broadcast biases). Commit `47795fd`.

---

## Bug fixes

### Region semantics

- **Early returns skipping exit checks.** A `return` inside `@zeroed` / `@deterministic` skipped the zero-mode decrement, and a `return` inside `@bounded` skipped the bound check. These are now compile-time errors: VNE-107 (`@zeroed`/`@deterministic`) and VNE-108 (`@bounded`). `break` and `continue` are unaffected. Commit `eb236d7`.
- **`@bounded` ignored scratch.** Static scratch and runtime VLAs never touch the arena, so a region could hold far more memory than its bound. The bound now counts all three sources, with compile-time accounting for static scratch (VNE-101) and a pre-declaration runtime check for VLAs (VNE-102); the exit panic reports the true peak. Commit `73fb29a`.
- **Single-index access on runtime-dim scratch.** `scratch buf :: Float64[n]` lowers to a raw C VLA, but `buf[i]` fell through to the `Array` path and emitted `v_buf.data[i]`. Both `IndexAccessNode` and `IndexAssignmentNode` now detect runtime-dim scratch, enforce rank 1 (VNE-071 for higher rank), and emit `bRaw[idx]` with the normal bounds check. Commit `c79cb9f`.

### Arduino / MCU codegen

- **Unlinked `vyne_unary` on every unary minus.** Fixed by the native unary fast paths (`4332172`, `f4af3e9`).
- **`region`/`scratch` link failures on AVR.** Fixed by the MCU runtime shims and bounds-check dispatch (`1c8a703`, #96).
- **Double-evaluated boxed property getters.** `nativeInit` re-emitted a getter inside the emitted ternary; it now binds the boxed RHS to a temp first. Commit `1c8a703` (#96).
- **Serial `begin` / printing.** Fixed for debugging. Commit `c6cf264`.

### Typed arrays

- **`.place_all()` mutated a throwaway wrapper.** The typed-array path fell through to the boxed fallback and grew the wrapper rather than the array, so a later `arr.size` read 0. A dedicated handler now targets the underlying `VyneArray_f64`. Commit `c5a8de2` (#94).
- **`Float32` unary and scratch gaps.** Completed by native Float32 fast paths. Commit `9c8275e` (#111).

### `vlin`

- **Silent shape mismatches.** All public ops now throw with row/column counts in the message instead of returning `0x0` / `-1`. Commit `e16f3e4`.
- **README worked example triggered a shape mismatch.** Rewritten to use `add_bias`. Commit `e16f3e4`.

### Runtime and libraries

- **`vconv` weight-bundle bounds.** The `.vwy` parser now caps names at 4096 bytes and rejects value counts that cannot fit in the remaining file bytes before multiplication, turning malformed bundles into empty models instead of oversized allocations or `size_t` overflow. Commit `0dfb939` (#112).
- **Floating-point literals emitting as integers.** Fixed in `floatLit`. Commit `3c8d8d4` (#104).
- **`vcsv` per-character allocation.** Byte-oriented parsing via `String.byte_at` + `vcore.sb_append_char`. Commit `28a0a40` (#109).

### Build and CI

- **Ubuntu 24.04 host build.** Added `<cstdint>` to `lexer.h` (both `int64_t` and the new `uint64_t` were satisfied only transitively) and escaped `??=` trigraph sequences in user-facing strings. Commit `28ca465`.
- **FTXUI include path in CI.** Added `-I./vendor` so `cli/tui.cpp` compiles on Linux. Commit `b5dfd26` (#116).
- **Raylib removed from CI.** Dropped the X11/OpenGL dev packages, the raylib clone/build step, and the `-lraylib -lGL -lX11` link flags. Commit `28ca465`.
- **Init TUI with space-containing directory names.** Derived project names are sanitized. Commit `0ddbf0a`.

---

## Runtime and memory

### Desktop arena

- Poison-on-rewind debug mode, with `ArenaBlock.used` now recording the high-water mark so a rewind to an earlier block poisons exactly the bytes filled after the checkpoint. Commit `793ec9f` (#98).
- 32-byte typed-array payload alignment (`VYNE_TYPED_ARRAY_ALIGN`), with `@slab` power-of-two size classes. Commit `a6abd34` (#99).
- `@ring` rotating-slot runtime (`vyne_ring_runtime.h`). Commit `80a0e74` (#100).
- `@streamed` pinned-memory runtime (`vyne_stream_runtime.h`) with platform providers and best-effort locking. Commit `a10425a` (#101).
- `@persistent` commit-arena frames (`vyne_persistent_enter` / `vyne_persistent_leave`) and commit-arena introspection. Commit `a684ed0` (#102).
- `vmem` gains `stream_total_allocated`, `stream_peak_allocated`, `stream_is_pinned`, `stream_block_count`, and commit-arena totals.

### CUDA device arena

`vdevice.h` provides a device arena with checkpoint/rewind, a dynamic CUDA driver backend (`nvcuda.dll` / `libcuda.so.1`), a CPU fallback, and elementwise/matmul kernels for Float32 and Float64. `@device` regions and the `device_*` intrinsics lower through codegen. Commit `9c8275e` (#111).

### MCU arena

The Arduino arena exposes `arena_used()` and a `g_arena.total_allocated` counter that `@bounded` reads; `@bounded` overflow halts via `vyne_panic_region_bound`. Commit `6ff43e1` (#114). The arena is 1 KB with a 16-deep checkpoint stack and hard-halt OOM (`f3fc3bf`, #95).

### String builder

`vcore.sb_append_char(handle, code)` appends a single byte without allocating a one-character string, the substrate for `vcsv`'s byte-oriented parser. Commit `28a0a40` (#109).

---

## Documentation

- **README refocused** on scientific/embedded computing with the AVR target in scope: corrected operator table, documented bitwise support and hex literals, fixed ruleset examples, added VNE-006/-007/-083/-090/-091, expanded the CLI reference, and archived the removed modules. Commit `cf9be6c`.
- **Arduino runtime API reference** (`vyne/runtime/arduino/README.md`): full API reference for 91 functions across 10 headers, design constraints, memory model, common patterns, a four-step binding checklist, and an explicit not-bound list. Commit `23a3de7`.
- **`docs/device.md`**: `@device` syntax, intrinsics, device-arena semantics, the CUDA driver/PTX path, the CPU fallback, PTX regeneration, performance comparison, and limitations. Commit `9c8275e` (#111).
- **`docs/init.md`**: questionnaire, flags, templates, exit codes, and test command. Commit `6ff43e1` (#114).
- **Regions reference** (`docs/tutorials/reference/regions.md`) updated with the implemented-policy table for `@bounded`, `@zeroed`, `@deterministic`, `@ring`, `@streamed`, `@persistent`, and `@slab`. Commits `3837317`, `80a0e74`, `a10425a`, `a684ed0`, `a6abd34`.
- **Standard-library roadmap rewritten** (`docs/todos/todo_libraries.md`): shipped libraries marked, remaining work (`voptim`, `vsignal`, `vsparse`, `vdecomp`, `vnum`) defined, and codegen constraints for library authors recorded. Commit `47c9e22`.
- **Language design milestone** (`milestone.md`): a 1323-line goals document. Commit `742165d`.
- **Library READMEs** for `vstat`, `vtermplot`, `vcomplex`, `vinterp`, `vcsv`, `vimage`, and `vconv`, plus the ESPCN asset guide (`examples/espcn_inference.md`). Commits `3c8d8d4`, `53ec847`, `f7e7984`, `ee81de9`, `28a0a40`, `0dfb939`.
- **`vlin` README**: `Elementwise.vy` in the layout table, a new element-wise math section, and a corrected worked example. Commit `e16f3e4`.
- **CLI README and help refresh**: `vynec init` documented with templates, flags, and exit codes; `vynec --help` and `vynec init --help` updated. Commit `31bc169` (#115).

---

## Internal changes

### Codegen and parser

- Region-policy emission (`regions.cpp`), `BoundedContext` stack, `zeroModeDepth`, and qualified-commit parsing. Commits `3837317`, `73fb29a`, `eb236d7`.
- Scratch method dispatch (`tryEmitScratchMethod`) wired ahead of typed-array methods, and a `place_all` handler in `tryEmitTypedArrayMethod`. Commit `c5a8de2` (#94).
- Runtime-dim scratch indexing in `collections.cpp`. Commit `c79cb9f`.
- `UnaryNode::getCExpr` native fast paths in `operators.cpp`. Commits `4332172`, `f4af3e9`.
- Hex-literal tokenization in `lexer.cpp` and `#include <cstdint>` in `lexer.h`. Commits `ced54ff`, `6077f22`, `28ca465`.
- Floating-point literal emission in `floatLit`. Commit `3c8d8d4` (#104).
- `Float32` across parser, interpreter, AST, typed arrays, and native ABI. Commit `9c8275e` (#111).
- `@device` region lowering and intrinsic interception. Commit `9c8275e` (#111).
- `vynec init` dispatch in `main.cpp`, `cli/template_engine.h`, `cli/init.cpp`, `cli/tui.cpp`, and `cli/tui.h`. Commits `681e7e0`, `6ff43e1`, `0ddbf0a`.

### Build and CI

- xmake gains the init test target (`xmake test-init`), FTXUI vendoring configuration, and `UNICODE` / `_UNICODE` / `NOMINMAX` definitions on Windows. Commits `681e7e0`, `6ff43e1`.
- Host CI drops Raylib/X11/OpenGL and keeps the `vendor/arduino` exclusion; adds `-I./vendor` for FTXUI. Commits `28ca465`, `b5dfd26` (#116).

### Vendoring

- `vendor/ftxui_all.hpp` with the FTXUI MIT license, generated by `tools/amalgamate.py`; `cli/tui.cpp` is the single FTXUI implementation translation unit. Commit `6ff43e1` (#114).
- `stb_image` / `stb_image_write` vendored for `vimage`, compiled into the generated translation unit. Commit `0dfb939` (#112).

### Tests

- Runtime tests: `poison.c`, `alignment.c`, `ring.c`, `slab.c`, `streamed.c`, `persistent.c` under `vyne/runtime/tests/`. Commits `793ec9f`, `a6abd34`, `80a0e74`, `a10425a`, `a684ed0`.
- `scripts/test_init.ps1` covers every template (generate, inspect, compile/run ML/scientific/SR, build embedded firmware when avr-gcc is present, `--yes`, implicit blank, and the no-name non-interactive fallback); 75 checks. Commits `6ff43e1`, `0ddbf0a`.
- Library, device, region, Arduino, and codegen-peephole tests listed under New features.

### Changelog housekeeping

- The v0.4.0 changelog update is the oldest commit in this release window. Commit `7ac3925`.

---

## Breaking changes

### `vlin` throws on shape mismatch

Every public `vlin` op now throws on mismatch instead of returning `0x0` / `-1`, and `Matrix.reshape` throws on size mismatch. Callers that checked for `0x0` must be updated; callers that ignored it will now see a hard error at the mismatch. Commit `e16f3e4`.

### `@bounded` counts scratch and VLAs

A region that fit its bound under the arena-only accounting may now fail VNE-101 (static scratch) or VNE-102 (runtime VLA / arena). Raise the bound or shrink the scratch. Commit `73fb29a`.

### Early returns in checked regions are rejected

`return` inside `@zeroed` / `@deterministic` fires VNE-107, and `return` inside `@bounded` fires VNE-108. Restructure so the return is outside the region, or drop the policy. Commit `eb236d7`.

### `Float32` semantics and device dtype default

`Float32` op `Float32` stays `Float32`; mixing with `Float64` or `Int64` promotes to `Float64`. `device_alloc(n)` now allocates `Float32`; use `device_alloc_f64(n)` for the double path. Mixed-dtype handle sets are rejected. Commit `9c8275e` (#111).

### Typed-array alignment

Typed-array payloads are now 32-byte aligned by default. Code that assumed 8-byte payload alignment (for pointer arithmetic or hand-rolled interop) should re-check; override with `-DVYNE_TYPED_ARRAY_ALIGN`. Commit `a6abd34` (#99).

### Desktop-only region policies

`@streamed` and `@persistent` are rejected on AVR, and `@device` is rejected on AVR. `@bounded`, `@zeroed`, and `@deterministic` do lower on the MCU runtime. Commits `a10425a` (#101), `a684ed0` (#102), `9c8275e` (#111).

### Removed CI dependencies

Raylib, X11/OpenGL dev packages, and their link flags are gone from CI. This matches the earlier `vglib` removal from the surface and does not reintroduce the module. Commit `28ca465`.

---

## Upgrade guide

1. **Catch `vlin` shape errors.** Remove explicit `0x0` checks and rely on the new throw; audit any code that previously ignored a `0x0` return.
2. **Re-check `@bounded` budgets.** Bounds now include static scratch and runtime VLAs. Raise `N` or shrink scratch arrays; run the region safety files for the exact diagnostics.
3. **Move `return` out of `@zeroed` / `@deterministic` / `@bounded` regions,** or drop the policy from that region (VNE-107 / VNE-108).
4. **Adopt `Float32` deliberately.** Add the `f` suffix or `float32(x)` where you want single precision, and use `device_alloc_f64` if you need the double device path.
5. **Re-check typed-array interop** if you relied on 8-byte payload alignment; 32 bytes is now the default.
6. **Keep device/persistent/streamed regions on desktop.** These policies are rejected on AVR; `@bounded`, `@zeroed`, and `@deterministic` are supported there.
7. **Try `vynec init`.** Use the interactive questionnaire or `--yes` / `--name` for scripted generation, then `vynec init --help` for templates and exit codes.
8. **Rebuild with xmake** after pulling; the Arduino backend, FTXUI, and stb integrations changed the build graph.
9. **Verify the version string** in CLI help after publishing; `main.cpp` and `cli/init.cpp` still carry the previous version constant and need bumping to `0.6.0`.

---

## Known limitations

- **`@deterministic` scan is shallow.** It cannot see inside `WhileNode` / `ForNode` bodies; the runtime zeroed counter is the defence-in-depth there. It also does not imply `@bounded`.
- **Multiple VLAs in one block** are checked per declaration; their combined peak is not separately checked.
- **`region.commit` on a primitive `Float64` / `Int64` local** is still not lowered (`RegionCommitNode::compile` assumes `VyneValue`).
- **Slices of runtime VLAs** (`buf[lo:hi]`) still lower through `SliceNode` to a container-style call; slices of runtime-dim scratch are not supported yet.
- **`break` / `continue` from a loop nested inside a region** triggers `emitRegionUnwind` for every open region (pre-existing use-after-free risk). Keep loops outside regions until the fix lands.
- **`@streamed` pinning is best-effort.** Platforms without locking fall back to `malloc`, and `vmem.stream_is_pinned` reports the actual outcome.
- **`@persistent` has no automatic reclamation** and is desktop-only; values live for the whole program.
- **CUDA `@device` FP64 is parity, not a scalable win.** Against tuned BLAS, FP64 is ~1.8× faster at 512×512 but ~8% slower at 1024×1024 on consumer Blackwell; the 200–300× figure is against the naive Vyne CPU loop only. FP32 is the recommended path. The CPU fallback is naive, not tuned BLAS.
- **`vimage` / `vconv` bind only in compiled mode.** The AST interpreter does not bind these native modules yet.
- **SSIM uses an 8×8 uniform window, not Gaussian.** Numbers quoted against published SSIM values should account for this; identical inputs return 1.0 and identical `psnr` inputs return a 100 dB sentinel.
- **`vconv` is CPU-only.** The `@device` path accelerates elementwise/matmul intrinsics, not the convolution kernels.
- **Arduino is Uno R3 only.** No I2C/SPI/Wire/SoftwareSerial bindings, no callback-based interrupts (`attach_interrupt` counts edges only), no typed-array bounds checking, 32-bit `double`, emulated `int64`, 16 nested regions max, and `out()` is unavailable.
- **MCU typed-array method surface is partial.** `contains`, `pop`, `reverse`, `sort`, `place_all`, `delete_at`, `back`, and `pop_front` are not bound on AVR.
- **`vlin.multiply` through the `Ops.vy` wrapper is still boxed** on MCU even though the kernel is unboxed; direct `vlin.k_*` calls from a sketch have not been hardware-tested.
- **`vynec init` needs an 80×24 terminal.** Non-TTY environments exit 2, and `--no-tui` without `--name` also exits 2.

---

## Contributors

- **Tuncay Gafarli** — all commits in this release.

## Full commit list

See `git log v0.5.0..HEAD` for the raw list. The 40 commits this release covers span `7ac3925` (2026-10-29) through `47795fd` (2026-11-09).

**Full Changelog**: <https://github.com/t2ncay/vyne/compare/v0.5.0...v0.6.0>

---

# Vyne v0.7.0 - differentiable implicit regions, project-aware tooling, and the numerical stack

**Released 2026-11-11**

This release makes Vyne's automatic-differentiation stack cover implicit equations, not just explicit code: `@implicit` regions solve `F(x, z) = 0` and differentiate the residual at the root with the Implicit Function Theorem, without unrolling the solver. It also lands the source-to-source AD surface (`@differentiable`, `@backward`, `grad`), custom `<fn>_vjp` adjoints, the `voptim` / `vsparse` / `vdecomp` and `vsignal` / `vcontrol` numerical libraries, project-aware `vynec build` / `run` / `bench` commands with JSON reports, thread-local arenas and `@layout<soa>` struct scratch, early `drop()` and zero-copy `region.move()`, `@realtime` deadlines with executable contracts, Arduino I2C/SPI bindings, and a full-device SRCNN showcase. The documentation site was rebuilt around the current language and library surface, and the planning documents were reconciled with the shipped tree.

---

## Highlights

- **Differentiable implicit regions.** `@implicit` composes with the region policies, enables `newton_solve(F, x..., z0)` / `implicit_solve`, and lowers `backward(root[, seed])` to a one-shot residual adjoint using `dz/dx_i = -(dF/dx_i)/(dF/dz)`. Solver iterations are never stored or replayed. Adds `VNE-127` / `VNE-128` and a self-checking `implicit_test.vy`. Commit `6cd2870` (#132).

- **Source-to-source AD surface.** `@differentiable fn`, `@backward region`, and `grad(f, x)` generate a primal plus a `fn_<name>_grad` companion for straight-line `Float64` bodies and `Array<Float64>` reduction VJPs. A `<fn>_vjp` convention supplies custom reverse passes for tensor-shaped losses. Commits `be018e0` (#122), `c595bdd` (#128).

- **Scientific library stack.** `voptim` ships SGD/momentum/Nesterov/Adam/AdamW/RMSProp/Adagrad plus learning-rate schedules; `vsparse` ships CSR, SpMV, residuals, and conjugate gradient; `vdecomp` ships LU, QR, Cholesky, Jacobi eigen, and singular values. `vsignal` adds DSP filters and `vcontrol` adds PID, complementary and Kalman filters plus state-space models. Commits `05d8f50` (#127), `6d75b19` (#121).

- **Project-aware CLI.** `vynec build` and `vynec run` discover `project.toml`, resolve the conventional entry point, load dependencies, and map targets; `vynec bench` adds warmup/measured runs, mean/median/min/max/p95/p99/stddev, and deterministic JSON reports. The existing flag-based CLI is unchanged. Commits `867a638` (#129), `a890577` (#130).

- **Region and runtime upgrades.** Arenas are thread-local while the commit arena is a shared, spinlock-protected hand-off point; `@layout<soa>` lowers struct scratch to one packed array per field. `drop()` reclaims a typed-array tail early and `region.move()` transfers large allocations into the commit arena without a copy. `@realtime<ms>` adds deadline telemetry and executable `requires` / `ensures` / `@invariant` contracts. Commits `e64ed24` (#124), `a3c8d27` (#123), `e8b9dfa` (#120).

- **Arduino I2C/SPI.** The AVR backend gains 8 Wire bindings and 6 SPI bindings, typed `vi2c` / `vspi` surfaces with range validation, and demand-driven bundled-library builds that keep unused peripherals out of the firmware. Commits `748c6fa` (#118).

- **Full-device SRCNN and convolution adjoints.** `vconv` gains CPU backward kernels, the device backend gains FP32/FP64 `conv2d`, and the SRCNN project runs all convolutions, ReLUs, residual add, and download inside one `@device<cuda>` frame while training through `@backward` and a custom `sr_loss_vjp`. Commit `c595bdd` (#128).

- **Documentation rebuild.** The GitHub Pages site is regenerated from `docs/src` with full pages for AD, CUDA/`@device`, the AVR target, all 20+ libraries, the CLI, performance, keywords, operators, and error codes; the README region-policy matrix and external-library section are refreshed. Commits `6fdda0d` (#131), `a0fef12`, `206b127`, `3c72cc5`, `8bfa634`.

---

## New features

### Differentiable implicit regions

`@implicit` marks a region whose body solves an implicit equation. The compiler emits a Newton primal over the residual's native Float64 variant and records the converged root plus the captured parameter values as ordinary C locals. The reverse pass differentiates the residual once at that fixed point and applies the Implicit Function Theorem:

```
dz/dx_i = -(dF/dx_i) / (dF/dz)
```

```vyne
@differentiable
fn residual(x :: Float64, z :: Float64) -> Float64 {
    return z * z * z + x - 1.0;
}

x :: Float64 := 0.0;

@implicit
@bounded<64_000_000>
region solve {
    z :: Float64 := newton_solve(residual, x, 0.0);
    backward(z);              # x.grad = -1/3
};
```

- `newton_solve(F, x..., z0)` and `implicit_solve` are intercepted only inside an `@implicit` region; outside it, the names resolve as ordinary functions.
- The residual must be a compiler-generated `@differentiable` scalar `Float64` function whose last parameter is the state. Custom `<fn>_vjp` adjoints are rejected because they cannot expose `dF/dz`.
- Parameter arguments must be plain variables, and the root must be a fresh `:=` declaration directly assigned from the solver so `.grad` can be published.
- Newton uses a `1e-12` tolerance, a default 64-iteration ceiling, and an optional `@implicit<N>` override.
- `backward(root)` seeds `dL/dz = 1.0`; `backward(root, seed)` chains an explicit upstream sensitivity and publishes both parameter and root `.grad` slots.
- `VNE-127` reports malformed implicit calls or residuals outside the scalar subset. `VNE-128` reports runtime non-convergence or a singular `dF/dz` through `vyne_panic_implicit`.
- `@implicit` is desktop-only in this release; it is rejected on AVR. Vector residuals, Jacobian assembly, and linear solves are deferred.
- `examples/autodiff/implicit_test.vy` covers a scalar cubic root, seeded and unseeded adjoints, a two-parameter residual, `@implicit<48>`, and `@bounded` composition. Commit `6cd2870` (#132).

### Source-to-source automatic differentiation

`@differentiable` marks a function whose adjoint the compiler generates. `@backward` scopes the reverse sweep to a region, and `grad(f, x)` is expression-level sugar. The generated companion recomputes the primal intermediates, seeds the returned expression, walks assignments in reverse, and publishes input gradients through out-parameters. There is no runtime tape interpreter and no per-operation dispatch.

- Supported scalar expressions: `+ - * / **`, `sin cos exp log sqrt tanh abs relu`, and `pow(a, b)`, in plain-call and method form.
- `Array<Float64>` gradients lower the VJPs for `mse` / `k_mse_sum`, `mae` / `k_mae_sum`, `dot` / `k_dot`, `sum`, `mean`, `norm_sq` / `sum_sq`, and `abs_sum` to loops over native array storage.
- `grad(f, w)` returns a fresh `Array<Float64>` for unary array functions; `backward(...)` on array parameters is deferred.
- Region attributes compose (`@backward @bounded<...>`), and numeric literals accept `_` digit separators.
- Anything outside the subset is rejected with `VNE-120`..`VNE-128` instead of silently miscompiling.
- A function named `<fn>_vjp` registers itself as the custom reverse pass for `<fn>`. It receives the original boxed arguments and owns its gradient storage, which is how conv-net losses and other tensor-shaped workloads opt out of the scalar/array subset.
- Examples: `scalar_grad.vy`, `backward_region.vy`, `array_grad.vy`, `autodiff_test.vy`, `custom_vjp_test.vy`, and `implicit_test.vy` under `examples/autodiff/` and `examples/test/`. Commits `be018e0` (#122), `c595bdd` (#128), `6cd2870` (#132).

### Numerical libraries

#### `voptim`

Optimizer states, native update kernels, and learning-rate schedules for flat `Array<Float64>` buffers. The surface covers SGD, momentum, Nesterov, Adam, AdamW, RMSProp, and Adagrad plus constant, step, exponential, cosine, and warmup-cosine schedules. State is built once; step wrappers validate shape and update parameters and moment buffers in place. Direct interface construction supports zero-copy setup from `vlin.Matrix.data`. `voptim_test` covers every schedule and optimizer on a quadratic, AdamW decay, and bias correction; the benchmark runs 100k parameters for 100 steps. Commit `05d8f50` (#127).

#### `vsparse`

CSR construction (`zeros`, `identity`, `diagonal`, `from_dense`), conversion (`to_dense`, `transpose`, `copy`), native-ABI SpMV, residual/Frobenius/dot helpers, and conjugate gradient. CG validates shape, allocates `x`/`r`/`p` plus one reusable `Ap` buffer, and keeps peak memory at O(rows) independent of iteration count. Tests cover non-symmetric SpMV and transpose, identity/diagonal/zeros, Frobenius, and an SPD solve to `1e-8`. CSC, SpMM, preconditioners, and graph-Laplacian helpers are deferred. Commit `05d8f50` (#127).

#### `vdecomp`

Partial-pivot LU with determinant metadata and triangular solves, Householder QR and least squares, Cholesky with SPD solve, symmetric cyclic-Jacobi eigen, and singular values. Raw routines take flat row-major arrays plus dimensions; `Ops` wrappers accept `Matrix` / `Series` structs and provide `determinant`, `solve`, `inverse`, `solve_spd`, `qr_matrix`, `least_squares_matrix`, `eigen_matrix`, and `svd_values`. Singular values currently form `A^T A`, so the condition number is squared; one-sided Jacobi is the documented follow-up. Commit `05d8f50` (#127).

#### `vsignal` and `vcontrol`

`vsignal` adds moving average, exponential, FIR, median, biquad IIR (RBJ low/high/band/notch), windows, resampling, and optional FFT magnitude helpers. `vcontrol` adds PID with anti-windup, derivative filtering and bumpless transfer, a complementary filter, scalar and constant-velocity Kalman filters, and a two-state state-space model. Streaming state lives in `Array<Float64>` fields so mutations propagate through the boxed call path; whole-buffer work uses native kernels. Both ship tests and benchmarks under `examples/libs`. MCU linkage is blocked on the MCU struct runtime. Commit `6d75b19` (#121).

### Project-aware CLI

`vynec build [path]` and `vynec run [path]` walk up to `project.toml`, resolve `src/main.vy` before `main.vy`, load `[project]`, `[dependencies]`, `[targets]`, `[build]`, and `[device.cuda]`, and map board/CUDA targets. The existing flag-based CLI remains untouched, and explicit flags stay authoritative over project defaults. Commit `867a638` (#129).

`vynec bench` and `vynec bench <file>` add warmup plus measured execution using `high_resolution_clock`, silent child execution, mean/median/min/max/p95/p99/population-stddev statistics, and a deterministic JSON report containing project, target, timestamp, configuration, and results. Project discovery, `[bench]` warmup/iterations, CLI overrides, and CUDA native compilation are supported. Indirect VyneLinker units are filtered to user-owned files, so external modules do not inflate benchmarks. Commit `a890577` (#130).

### Regions and runtime

#### Thread-local arenas

`g_arena`, the bump pointers, the zero-mode counter, and the fresh-page flag are thread-local, so local allocation has no lock contention. The commit arena remains process-global and is protected by a recursive spinlock with a thread-local nesting depth, allowing `@persistent` to hold it for a whole region without self-deadlocking. `arena_free_thread()` and `vmem_runtime_thread_cleanup()` free only a worker's own blocks; process teardown still frees the shared commit arena. `tls_arena.c` stresses eight threads with independent allocation cycles and shared commit publishing. Commit `e64ed24` (#124).

#### `@layout<soa>` struct scratch

`@layout<soa>` composes with any memory policy and lowers struct `scratch` to one packed C array per primitive field for both static and runtime (VLA) dimensions. `particles[i].field` and multi-index `particles[r, c].field` rewrite directly to the field array through the existing flat-index and bounds-check helper. `@layout<aos>` is the explicit default, `@layout<struct_of_arrays>` is an alias, struct scratch without `soa` is `VNE-062`, and whole-element access is `VNE-063`. Commit `e64ed24` (#124).

#### `drop()` and zero-copy `region.move()`

`drop(x)` reclaims a typed-array payload when it is the live arena tail and empties the container; a non-tail drop is a no-op and a double drop is harmless. `region.move(x)` is the expression-valued sibling of `region.commit(x)`: it splices a dedicated tail block into the commit arena when possible and falls back to a commit-arena copy for small or shared blocks. Primitive values, stack scratch arrays, and allocations that predate the current region are rejected. Commit `a3c8d27` (#123).

#### `@realtime` and executable contracts

`@realtime<ms>` records a monotonic timestamp on entry and checks the deadline at normal close and on early returns. Overruns halt with `VNE-109`, and worst observed time / overrun counts are exposed through `vcore.realtime_*` or `arduino.realtime_*`. `requires`, `ensures`, and `@invariant` are executable contracts with desktop and AVR runtimes, evidence logging, and dedicated positive/negative tests. Commit `e8b9dfa` (#120).

### Arduino I2C/SPI

- Eight I2C bindings over `<Wire.h>`: `begin`/`end`, clock selection, single-byte read/write, register read/write with repeated start, and `available`.
- Six SPI bindings over `<SPI.h>`: `begin`/`end`, `SPISettings` clock/mode, transaction end, 8-bit transfer, and 16-bit transfer16.
- `vyne_arduino.h` includes the new headers only under `VYNE_USE_I2C` / `VYNE_USE_SPI`, and the AVR build integration scans bundled library `src/` trees, compiles them recursively, and links only the peripherals the generated translation unit actually uses.
- Typed `vi2c` and `vspi` surfaces validate 7-bit addresses, registers, values, clocks, and data modes, returning documented status codes instead of truncating silently.
- `i2c_imu.vy` reads the MPU-6050 `WHO_AM_I` register and `spi_loopback.vy` checks a five-byte MOSI-to-MISO pattern. Blink firmware size is unchanged, proving unused peripherals stay out of the build. Commit `748c6fa` (#118).

### Device convolution and SRCNN

- `vconv` gains CPU `conv_backward_input`, `conv_backward_weight`, `conv_backward_bias`, and `relu_backward`, registered in `VCONV_MAP` and wrapped in `vconv/Ops.vy`, with finite-difference tests.
- The CUDA backend gains FP32 and FP64 `conv2d` kernels (HWC input, OIHW weight, square kernel, stride, zero padding, optional bias) and exposes `device_conv2d`; the host dispatcher validates handles and dtype and provides a CPU-backend fallback.
- The SRCNN project runs all three convolutions, both ReLUs, the residual add, and the final download inside one `@device<cuda>` frame. The full-GPU output matches the CPU model; training uses `@backward` with the custom `sr_loss_vjp` and `voptim` Adam.
- Measured on the generated 128x128 -> 256x256 pair: bilinear 29.8139 dB PSNR / 0.985724 SSIM, FSR-style baseline 31.5510 dB / 0.990767, Vyne SR 33.5100 dB / 0.994912. The README records that this is a single-image demo and that the FSR baseline is a style implementation, not bit-exact AMD EASU/RCAS. Commit `c595bdd` (#128).

### Documentation, examples, and project maintenance

- **Language manual rewrite.** The GitHub Pages site is rebuilt around the current surface with pages for AD, CUDA/`@device`, the AVR target, all 20+ libraries, the CLI, performance, keywords, operators, and error codes. The source pipeline (`docs/src`, `sync-docs.mjs`), Markdown tables/images, Vyne highlighting, search index, and generated content/pages are included. Commit `6fdda0d` (#131).
- **README region-policy matrix.** The memory-model section now documents all sixteen modifiers with allocator/lifetime behavior, intrinsics, target support, diagnostics, composition rules, and fixed-size buffer restrictions. Commit `a0fef12`.
- **External library system.** The README section is rewritten for the current facade convention, library groups, kernel/wrapper pattern, state-array convention, test/benchmark locations, and 110-function AVR surface. Commit `206b127`.
- **Planning docs reconciled.** The seven `docs/todos` documents carry dated status snapshots and correct stale checklists; a domain-expansion strategy document is added. Commits `3c72cc5`, `8bfa634`.
- **Project maintenance.** Stale `rv_soc` and `rv_spc` example projects are removed, `srcnn` project metadata is corrected, and the generated-init project templates are committed. Commits `9a5e37c`, `c23c093`, `d2d1e36`, `ff665be`.
- **New tests and demos.** `implicit_test.vy`, `custom_vjp_test.vy`, `device_conv_test.vy`, `vconv_backward_test.vy`, `region_layout_soa.vy`, `region_drop.vy`, `region_move.vy`, `tls_arena.c`, `perf_hotpaths_test.vy`, and the `voptim` / `vsparse` / `vdecomp` / `vsignal` / `vcontrol` tests and benchmarks.

---

## Bug fixes

### Hot-path performance series (#125, #126)

The two performance commits close defects across lexer, parser, codegen, and runtime without changing observable behavior.

- Map probes reuse the interned key hash instead of hashing every key twice.
- String transforms (`uppercase`, `lowercase`, `trim`), slices, interpolation, and array/struct stringification hand arena-owned buffers directly to `vyne_string_own`, removing duplicate allocation and copy passes.
- Interpolated values and array/struct elements are rendered once instead of twice.
- Map reads no longer intern missing keys, removing unbounded intern-table growth on miss-heavy workloads.
- String equality checks pointer equality before falling back to `strcmp`, and two null-string values now compare equal instead of dereferencing NULL.
- Typed-array growth uses `arena_alloc_aligned` on every boxed push and `place_all` path, preserving the 32-byte SIMD alignment after reallocation.
- The lexer reserves token capacity, stops copying plain string literals, bulk-scans string bodies, and casts every `ctype` argument to `unsigned char`.
- The parser binds lookahead tokens by reference and borrows interpolated-string parts instead of copying them.
- Codegen appends directly to plain `std::string` buffers instead of building indentation strings and routing every line through `std::stringstream`.
- Literal `through` bounds stay inline in the C `for` header; variable bounds are still captured at entry so mutating the bound does not change the trip count.
- Measured: 200k unique-key map misses 44.07 -> 29.49 ms median; 1M hit/miss lookups 27.20 -> 21.14 ms; large-string benchmark 37.71 -> 22.82 ms; 20k-statement `--emit-c` 29.4 -> 28.7 ms; 500k get/set map loop 15.2 -> 15.0 ms; 200k trim+uppercase+lowercase 26.6 -> 25.9 ms. Commits `3cbf49e` (#125), `1950047` (#126).

### Compiler and runtime corrections

- `@layout<soa>` reuses the scratch flat-index/bounds-check helper and teaches `resolveRHSKind` that field reads are primitives, preventing false `VNE-070` escapes when reducing a field into an outer scalar. Commit `e64ed24` (#124).
- `region.move` accepts the expression form on an outer assignment and marks the moved value committed, so the escape checker does not reject a valid transfer. Commit `a3c8d27` (#123).
- The custom-VJP path uses the original boxed arguments rather than assuming the scalar adjoint signature, and the registry is populated before the differentiable pre-pass so declaration order does not matter. Commit `c595bdd` (#128).
- AVR bundled-library builds are demand-driven, avoiding the ~1.4 KB flash penalty that an unconditional `Wire` global constructor added to a blink sketch. Commit `748c6fa` (#118).
- `@persistent` commit helpers serialize through the recursive commit spinlock, and worker threads free only their own arena blocks. Commit `e64ed24` (#124).
- Stale example projects and metadata are removed or corrected. Commits `9a5e37c`, `c23c093`.

---

## Runtime and memory

### Desktop arena

- Thread-local `g_arena`, bump pointers, zero-mode counter, and fresh-page flag; zero lock contention for local allocation. Commit `e64ed24` (#124).
- Shared commit arena protected by a recursive spinlock with thread-local nesting depth; `@persistent` holds it for the region body. Commit `e64ed24` (#124).
- `arena_free_thread()` / `vmem_runtime_thread_cleanup()` for worker exits; `arena_free_all()` remains process teardown. Commit `e64ed24` (#124).
- `drop()` LIFO tail reclamation and `region.move()` zero-copy block splice with commit-copy fallback. Commit `a3c8d27` (#123).
- `@realtime` timestamp/overrun telemetry and `vyne_panic_deadline`; contract checks and evidence logging. Commit `e8b9dfa` (#120).
- `vyne_panic_implicit` for `VNE-128` solver failures. Commit `6cd2870` (#132).

### Struct-of-arrays scratch

Under `@layout<soa>`, struct scratch lowers to one contiguous array per primitive field with static or runtime dimensions. Member reads and writes address the field arrays directly; multi-index access flattens row-major. The payload stays on the C stack for the region lifetime and is packed for SIMD and future coalesced device upload. Commit `e64ed24` (#124).

### Device arena and convolution

The device arena gains FP32/FP64 convolution kernels and a host dispatcher with a CPU fallback. The PTX is embedded and JIT-compiled by the driver, so generated C needs no CUDA headers or link flags. Commit `c595bdd` (#128).

### Arduino runtime and build integration

The MCU runtime gains Wire/SPI bindings behind `VYNE_USE_I2C` / `VYNE_USE_SPI`, a no-op `vyne_deploy_module`, and demand-driven compilation of bundled library sources. `@realtime` and contracts lower on AVR. Commit `748c6fa` (#118), `e8b9dfa` (#120).

### CLI and project configuration

Project loading parses `project.toml`, resolves dependencies against the source-tree and installed-prefix module roots, and forwards deterministic/CUDA metadata to the emitter. Benchmark configuration adds `[bench]` warmup/iterations and report generation. Commits `867a638` (#129), `a890577` (#130).

---

## Documentation

- **New documentation site.** Full language, library, showcase, CLI, performance, error-code, keyword, and operator pages generated from `docs/src` with a maintainable sync pipeline. Commit `6fdda0d` (#131).
- **Automatic differentiation reference.** Documents `@differentiable`, `@backward`, `grad`, array reduction VJPs, custom `<fn>_vjp` adjoints, the generated C shape, diagnostics, and the deferred subset. Commit `be018e0` (#122), `c595bdd` (#128).
- **Implicit regions reference.** Documents `@implicit`, `newton_solve` / `implicit_solve`, `@implicit<N>`, `backward(root[, seed])`, the IFT formula, the scalar/desktop scope, and `VNE-127` / `VNE-128`. Commit `6cd2870` (#132).
- **Regions reference.** Documents thread-local arenas, `@layout<soa>`, `drop()`, `region.move()`, `@realtime`, and executable contracts. Commits `e64ed24` (#124), `a3c8d27` (#123), `e8b9dfa` (#120).
- **README refresh.** Region-policy matrix, external-library system, scientific library surface, CLI commands, and project layout. Commits `a0fef12`, `206b127`.
- **Planning docs.** Dated status snapshots for the seven `docs/todos` documents and a new domain-expansion strategy. Commits `3c72cc5`, `8bfa634`.
- **Library and project manuals.** READMEs for `voptim`, `vsparse`, `vdecomp`, `vsignal`, `vcontrol`, `vi2c`, `vspi`, `vconv`, and the SRCNN project, including API scope, benchmarks, and limitations. Commits `05d8f50` (#127), `6d75b19` (#121), `c595bdd` (#128), `748c6fa` (#118).

---

## Internal changes

### Compiler and codegen

- Source-to-source AD generator (`codegen/autodiff.{h,cpp}`): scalar straight-line reverse mode, array reduction VJPs, custom-VJP routing, `grad()` and `backward()` lowering, and the differentiable-function registry. Commits `be018e0` (#122), `c595bdd` (#128).
- `@implicit` region scope, solver lowering, captured root/parameter temps, IFT adjoint, and `VNE-127` / `VNE-128` diagnostics. Commit `6cd2870` (#132).
- Thread-local arena/runtime support, layout stack, SoA scratch registry, and field-array rewriting. Commit `e64ed24` (#124).
- `drop` builtin and `region.move` AST/codegen paths. Commit `a3c8d27` (#123).
- `@realtime` and contract parsing/lowering in the region and function paths. Commit `e8b9dfa` (#120).
- Device convolution dispatch, CUDA kernel loading, and PTX regeneration. Commit `c595bdd` (#128).
- Arduino I2C/SPI maps, bundled-use flags, and demand-driven build integration. Commit `748c6fa` (#118).
- Lexer/parser/codegen hot-path changes: token reservation, borrowed lookahead, direct buffer appends, inline literal loop bounds, map-hash reuse, and aligned array growth. Commits `3cbf49e` (#125), `1950047` (#126).

### CLI and tooling

- `cli/project.{h,cpp}`, `cli/file_handler.{h,cpp}`, and `main.cpp` add project discovery, configuration, dependency resolution, `build`/`run`, and shared project banners. Commit `867a638` (#129).
- `cli/bench.{h,cpp}` adds warmup/measured execution, statistics, JSON reports, and user-owned-file filtering. Commit `a890577` (#130).

### Tests

- AD: `scalar_grad.vy`, `backward_region.vy`, `array_grad.vy`, `autodiff_test.vy`, `custom_vjp_test.vy`, `implicit_test.vy`.
- Regions/runtime: `region_layout_soa.vy`, `region_drop.vy`, `region_move.vy`, `tls_arena.c`, `realtime_test.vy`, contract positive/negative tests, `perf_hotpaths_test.vy`.
- Libraries: `voptim_test` / `voptim_bench`, `vsparse_test` / `vsparse_bench`, `vdecomp_test` / `vdecomp_bench`, `vsignal_test` / `vsignal_bench`, `vcontrol_test` / `vcontrol_bench`.
- Device: `device_conv_test.vy`, `vconv_backward_test.vy`.
- Arduino: `i2c_imu.vy`, `spi_loopback.vy`.

### Docs pipeline

- `docs/src` sources, `sync-docs.mjs`, generated content/pages, search index, Markdown tables/images, and Vyne syntax highlighting. Commit `6fdda0d` (#131).

---

## Breaking changes

No intentional source-language breaking change is introduced in this window. The following behavior and surface changes are worth checking:

- **`@implicit` is desktop-only.** AVR rejects it with `VNE-080`; keep implicit solves on the desktop backend until the vector/MCU work lands.
- **`@layout<soa>` has layout boundaries.** Struct scratch requires `@layout<soa>` (`VNE-062`), and whole-element reads/writes are rejected in favor of per-field access (`VNE-063`).
- **`region.move()` restrictions.** It accepts only arena-backed values owned by the current region; primitives, stack scratch, and allocations predating the region are rejected.
- **Thread-local arenas.** Native C code that assumed a single process-global `g_arena` across threads must move shared publishing through the commit arena and call `vmem_runtime_thread_cleanup()` on worker exit.
- **Stale example projects removed.** `examples/projects/rv_soc` and `rv_spc` are gone; use the current `vynec init` templates or the SRCNN project.
- **CLI additions are additive.** `vynec build` / `run` / `bench` are new entry points; `vynec file.vy`, `--compile`, `--run`, and the existing flags keep their behavior.

---

## Upgrade guide

1. **Try the project-aware CLI.** Run `vynec build` or `vynec run` from a project directory, and use `vynec bench` for reproducible JSON reports. Existing flag-based invocations remain valid.
2. **Adopt the AD surface.** Use `@differentiable` + `@backward` + `grad` for explicit models, and `@implicit` + `newton_solve` for equations. Keep residual signatures `(x..., z) -> Float64` and bind the root with a fresh `:=`.
3. **Re-check region layouts.** If you use struct `scratch` for SIMD-style kernels, add `@layout<soa>` and access fields individually. Use `drop()` for LIFO tail release and `region.move()` for zero-copy persistent transfer.
4. **Audit thread assumptions.** Custom C or host workers should rely on thread-local local allocation and publish shared results through the commit arena; call the thread cleanup hook before a worker exits.
5. **Rebuild AVR firmware.** I2C/SPI sources are now demand-driven; regenerate and reflash sketches to pick up the new include/link behavior.
6. **Run the library tests and benchmarks** for `voptim`, `vsparse`, `vdecomp`, `vsignal`, and `vcontrol` after pulling; the library READMEs record the supported surface and deferred items.
7. **Regenerate the documentation site** with `npm run sync` in `docs/` if you maintain a local copy; generated content, pages, nav, and search index are updated from `docs/src`.
8. **Bump the version constants.** `main.cpp` and `cli/init.cpp` still carry `VYNE_VERSION = "0.4.0"`; update both to `0.7.0` before publishing.

---

## Known limitations

- **`@implicit` is scalar and desktop-only.** Vector residuals, Jacobian assembly, linear solves, array states, and AVR lowering are deferred. Custom `<fn>_vjp` adjoints cannot supply `dF/dz` and are rejected.
- **AD subset is deliberately small.** Scalar bodies are straight-line `Float64` with a final return; array support covers the reduction VJPs listed above. Array locals, element-wise array intermediates, matmul VJPs, loops, branches, `backward()` on array parameters, and `@recompute` / `@mixed` lowering are deferred. Custom adjoints own their gradient storage.
- **`voptim` wrappers pay a boxing boundary.** Native `k_*` kernels allocate nothing, but the wrapper costs the current call frame at the boundary. Optimizer state must be built once and kept in typed arrays.
- **`vsparse` scope.** CSC, SpMM, preconditioners, and graph-Laplacian helpers are not shipped; CG is the only iterative solver.
- **`vdecomp` singular values use `A^T A`,** so the condition number is squared. One-sided Jacobi is the follow-up for ill-conditioned matrices.
- **`vsignal` / `vcontrol` are desktop-only.** Their interface types are not implemented by the MCU struct runtime yet, so AVR linkage is blocked even though the kernels are array loops.
- **SRCNN is a single-image demo.** The FSR baseline is a style implementation rather than bit-exact AMD EASU/RCAS, convolution training remains CPU-side, and the device convolution supports square kernels with the documented stride/pad cases.
- **Benchmarks cover user-owned files only.** Project mode filters external modules and units outside the `project.toml` directory; explicit single-file benchmarks remain unfiltered.
- **`@realtime` is a watchdog, not a WCET proof.** Contracts and deadlines are runtime-checked and report violations; they do not statically prove timing or functional correctness.
- **Arduino I2C/SPI are single-byte / 16-bit transfers.** Block transfers need a buffer ABI, SoftwareSerial is not bound, and the target remains the Uno R3.
- **Thread-local arenas prepare for `@parallel`, which is still a separate language feature.** The arena layer is ready; the parallel region surface is not shipped.
- **`@layout<soa>` covers struct scratch, not arbitrary region values.** Whole-element access is rejected; per-field access is the contract.
- **Project templates changed.** The removed `rv_soc` / `rv_spc` examples are not maintained; use the shipped init templates or the SRCNN project.

---

## Contributors

- **Tuncay Gafarli** - all commits in this release.

## Full commit list

See `git log v0.6.0..HEAD` for the raw list. The 22 commits this release covers span `8bfa634` (2026-11-09) through `6cd2870` (2026-11-11).

**Full Changelog**: <https://github.com/t2ncay/vyne/compare/v0.6.0...v0.7.0>

---

# Vyne v0.8.0 - ESP32 hardware workflows, device training, universal plotting, and managed graphics

**Released 2026-11-15**

This release extends Vyne from numerical computation into practical embedded, device-training, and graphics applications. It introduces the ESP32 backend with configurable hardware presets and a select-once build/run workflow, zero-copy `Slice<T>` views and typed-array capacity management, explicit device-resident convolution and training primitives, reusable `vtensor` / `vnn` helpers, and a standalone ESPCN inference/training project. Universal `vplot` charts share a rendering engine across host images and ESP32 displays, while `vraylib` integrates vendored raylib 6.2, raymath, and rlgl with App-owned resources, automatic frame lifetimes, shaders, cameras, render targets, music, and one-shot audio. The Bodycam and Sector Shift examples are restored through the managed facade, and the documentation site grows from 53 to 61 pages with freshness checks and dedicated CI.

---

## Highlights

- **ESP32 backend.** Compile Vyne into Arduino-ESP32 firmware for ESP32, S2, S3, C3, and C6 boards. The runtime exposes shared MCU operations plus Wi-Fi/HTTP, NVS, displays, sprites, touch, IMU, DAC, and power controls, with a 64 KB default arena and demand-driven peripheral headers. Commit `61a14ae` (#134).

- **Select-once hardware configuration.** `vynec init` records a board/preset and connection settings in `hardware.toml`; `vynec hardware list`, `show`, and `setup` handle discovery, resolved configuration, and toolchain installation. `vynec build` compiles firmware and `vynec run` uploads, resets, and starts the board. Commits `e5bdb2c` (#142), `20d64f9` (#143).

- **Zero-copy slices and reusable buffers.** `Slice<Float64>` and `Slice<Int64>` borrow typed-array storage without copying the payload. `reserve()` and `resize()` add explicit capacity and length management without changing the existing `Array<T>` ABI. Commit `b496955` (#135).

- **Explicit GPU training primitives.** Device convolution gains backward input/weight/bias kernels, ReLU gradients, pixel shuffle and its reverse pass, MSE gradients, reductions, crop staging, SGD, and AdamW. Preallocated operations allow training buffers to be reused across steps. Commits `13bd9b8` (#136), `1ce2dee` (#137).

- **Reusable device ML libraries.** `vtensor` carries region-local device handles and HWC shape metadata; `vnn` composes explicit `@device_fn` convolution, activation, loss, backward, and optimizer blocks. `vml` remains the CPU dense/MLP layer. Commits `4f10ab9` (#138), `2da8412` (#139).

- **ESPCN inference and training.** A standalone project combines CPU exact inference, full-device Float32 inference, deterministic crop training, best/latest checkpoints, checkpoint reload, metrics, and reproduction scripts. Repeatability of the fixed bundled graph is documented for the same GPU and driver version. Commits `1ce2dee` (#137), `55e6747` (#140), `0fd74f9`.

- **Universal live plotting.** `vplot/vplot.vy` adds colorful line, scatter, bar, histogram, and heatmap charts on host images and ESP32 displays, managed chart surfaces, bounded rolling series, automatic timestamps, and throttled live presentation. Commit `6b80291` (#145).

- **Managed desktop graphics and audio.** `vraylib` brings raylib 6.2, raymath, and rlgl to compiled Vyne, then layers typed App-owned resources, automatic frame cleanup, named uniforms, resizable targets, cameras, streamed music, one-shot Sound, and fixed native saturation on top. Commits `b349223` (#146), `38b43c0` (#147), `d53eb69`.

- **Documentation and acceptance coverage.** The manual now publishes 61 pages, including ESP32, plotting, device libraries, managed/native graphics, ESPCN, and graphics showcases. A dedicated workflow validates generated output, local links, anchors, navigation, and search. Commits `cc7905d` (#148), `774aec1`.

---

## New features

### ESP32 firmware backend

The compiler gains a Desktop / AVR / ESP32 target model and emits an Arduino-ESP32 sketch against `vyne/runtime/esp32/vyne_esp32.h`. The CLI delegates core compilation, upload, and serial monitoring to `arduino-cli`.

- Board aliases: `esp32`, `esp32s2`, `esp32s3`, `esp32c3`, and `esp32c6`.
- Flags: `--esp32`, `--esp32-flash`, `--esp32-monitor`, `--esp32-port`, and `--esp32-baud`.
- Repeatable `--esp32-build-property key=value` overrides for standalone builds.
- Shared bindings for GPIO, serial, math, bits, memory, I2C/SPI, interrupts, contracts, and realtime telemetry.
- ESP32-specific bindings for NVS, Wi-Fi/HTTP, additional UARTs, display/touch, MPU6050, DAC, capacitive touch, sleep/reset, and heap inspection.
- Feature-gated headers keep unused peripheral stacks out of generated firmware.
- Up to four off-screen display sprites support buffered drawing through the normal `display_*` calls.

`use native arduino;` exposes the shared MCU surface on ESP32; EEPROM bindings are deliberately absent there, with NVS providing persistent storage. Commits `61a14ae` (#134), `2250209` (#141).

### Hardware configuration and initialization

`hardware.toml` becomes functional configuration rather than descriptive metadata. Bundled presets cover generic ESP32 boards and the measured HW-950 display/touch setup.

```shell
vynec init screen --name screen --kind embedded --preset hw950 --port COM5
cd screen
vynec hardware setup
vynec hardware show
vynec build
vynec run --monitor
```

- Configuration precedence is CLI overrides → TOML overrides → preset defaults.
- `--board`, `--preset`, `--port`, and `--baud` support scripted initialization.
- `--hardware-set section.key=value` supplies repeatable typed overrides.
- Validation covers drivers, dimensions, GPIO suitability, chip-select conflicts, and incompatible board families.
- Firmware, sketches, response-file settings, and intermediates live under `build/firmware/`.
- Hardware settings preserve platform USB/core definitions.
- Changed response-file settings request a clean object rebuild to prevent stale firmware uploads.
- Existing Uno metadata remains accepted.

The HW-950 starter uses configured landscape orientation, polls touch continuously, reports geometry/calibration/contact state, and accepts serial **C** for explicit four-corner calibration. Saved NVS calibration is otherwise preserved. Commits `e5bdb2c` (#142), `20d64f9` (#143).

### Zero-copy `Slice<T>` and typed-array capacity management

`Slice<T>` is a separate pointer-and-length view; `Array<T>` retains its existing data, size, and capacity layout.

- A typed slice declaration or argument borrows contiguous array storage.
- Whole-array arguments can become whole-array views.
- Bounds are half-open: `data[lo:hi]`.
- Indexed reads/writes and `size()` / `length()` are supported.
- Ordinary array slicing retains its copying behavior.
- Views do not own storage or extend the owner's lifetime.
- Growing, dropping, or reclaiming the owner can invalidate a view.

`reserve(n)` preserves length while ensuring capacity. `resize(n, fill)` changes logical length, initializes new elements, and retains capacity when shrinking. Desktop growth uses the existing arena-tail reclamation strategy; embedded growth checks the static arena budget before allocation. Commit `b496955` (#135).

### Device convolution and training primitives

The CUDA runtime adds Float32 and Float64 backward convolution kernels with HWC activations and OIHW weights. Gather-style accumulation assigns one output element to each thread, avoiding atomic reductions.

The explicit device surface includes:

- Convolution forward and backward input/weight/bias.
- ReLU forward and backward.
- Pixel shuffle forward and backward.
- MSE gradients and per-element loss contributions.
- Device scalar reduction with `device_sum`.
- Packed-buffer staging with `device_copy_slice`.
- In-place SGD and AdamW updates.

Handle-returning `vconv_*_device` wrappers allocate outputs within the active device frame. Preallocated `device_*` intrinsics let callers reuse activations, gradients, loss buffers, and optimizer moments.

`@device_fn` marks helpers that may issue device intrinsics from an active device region or another device helper. Imported helpers and nested helper calls are supported; calls without an active device context are rejected. Commits `13bd9b8` (#136), `1ce2dee` (#137).

### `vtensor`, `vnn`, and ESPCN

`vtensor.DeviceTensor` combines an Int64 device handle with height, width, channels, and element count. It carries metadata, not ownership.

`vnn` provides preallocated convolution/ReLU blocks, pixel shuffle, MSE gradient/reduction, backward blocks, and AdamW updates over those tensors and caller-owned handles. The ESPCN trainer is migrated from project-local wrappers to this reusable library surface. Commit `4f10ab9` (#138).

The standalone `examples/projects/espcn` project adds:

- CPU exact and full-device Float32 ReLU inference.
- Bilinear and FSR-style non-neural comparisons.
- PSNR, SSIM, edge/ringing metrics, timings, and CPU/GPU differences.
- Committed sample/reference images and an offline passthrough model.
- PyTorch checkpoint export to the weight-bundle format.
- Three fixed training crops and one held-out validation crop.
- Preallocated training buffers and device-resident AdamW moments.
- Training/validation CSVs and best/latest checkpoint export.
- Best-checkpoint reload and CPU inference verification.

PowerShell and POSIX reproduction scripts clean build outputs, smoke-test the exporter, train, reload the best checkpoint, run inference, and check decreasing losses. Commits `1ce2dee` (#137), `55e6747` (#140), `0fd74f9`.

### Universal plotting

The new `vplot/vplot.vy` facade shares one native chart/layout engine across RGB host images and RGB565 ESP32 displays.

- Line, scatter, signed bar, histogram, and heatmap charts.
- Automatic/fixed ranges, grids, labels, legends, themes, and dashboard viewports.
- One-call `show_*` display helpers and `save_*` PNG helpers.
- Managed charts with reusable surfaces and explicit close.
- Fixed-capacity live rings, automatic timestamps, and best-effort 100 ms redraw throttling.
- Borrowed Float64 line views and image overlays.
- Ordered column reduction that preserves spikes in dense monotonic lines.
- Native memory, allocation, and frame diagnostics.
- Generation-checked handles and allocation-free steady-state updates.

Selecting `vplot` during HW-950 initialization generates the short chart starter. The original single-file `vplot.vy` text-bar helper retains its API with updated syntax. Commit `6b80291` (#145).

### Vendored and managed `vraylib`

`vraylib` integrates raylib 6.2 with raymath and rlgl for compiled desktop Windows, Linux, and macOS.

The binding inventory covers 928 declarations: 901 direct/adapted bindings, two literal-safe text adapters, and 25 explicitly unavailable operations. Resources use generation-checked handles; pointer adapters validate supported buffer capacities and ownership. Vendored native archives build on first use and are cached by compiler/source/configuration fingerprints. Commit `b349223` (#146).

The managed facade adds:

- `App` and headless sessions.
- Typed Texture, Image, Font, Shader, Model, Camera, Music, Sound, and RenderTarget wrappers.
- Cached asset loading and named shader-uniform lookup.
- Frame-scoped transient arena cleanup.
- Automatic music updates and screen-following target resizing.
- LIFO render-pass tracking and callback-paired drawing.
- Borrowed logical target views that follow resize.
- Named compiled callbacks through `App.run`.
- Idempotent resource/session cleanup and descriptive load failures.

`App.saturation(drive, mode)` adds a fixed mixed-stereo soft/tanh or hard-clipping processor. Its native callback uses a packed lock-free atomic configuration without allocation, locks, logging, or Vyne calls. Commits `38b43c0` (#147), `d53eb69`.

### Restored graphics showcases

The Bodycam scene is migrated to typed assets, managed frames/music, cached uniforms, and resizing render targets while preserving its scene, animation, movement, physics, and post-processing.

Sector Shift retains the corridor, stalker events, radio, malfunction, blackouts, recording overlay, and ending. It adds corrected time-based movement/fades, one-shot radio playback, sector-specific fog/VHS shaders, managed billboard drawing, and native audio saturation.

Bounded runs and deterministic autoplay support acceptance testing; the original media remain local assets. Commits `b349223` (#146), `38b43c0` (#147), `6ce9f2a`, `b676295`.

---

## Bug fixes

### ESP32 runtime and hardware

- Reduce the default ESP32 arena from 128 KB to 64 KB so Wi-Fi-enabled firmware fits the original ESP32 linker layout. Commit `61a14ae` (#134).
- Correct Arduino-ESP32 ADC attenuation names, missing boxed operators, Float32 tagging, and unconfigured touch handling. Commit `61a14ae` (#134).
- Reject unavailable embedded native members during Vyne compilation rather than emitting unresolved C calls. Commit `61a14ae` (#134).
- Prevent sprite text crashes by selecting the built-in font, validate dimensions before narrowing, preserve allocation-failure behavior, and correctly clear/delete selected sprites. Commit `2250209` (#141).
- Convert boxed numeric arguments instead of reinterpreting integer bits as doubles; preserve signed 64-bit serial output. Commit `2250209` (#141).
- Treat missing calibration as an expected first-boot condition and validate versioned calibration blobs. Commit `2250209` (#141).
- Preserve platform definitions when applying display settings; correct HW-950 geometry, enables, orientation, and mirror handling. Commit `2250209` (#141).
- Add stable-sample, geometry-checked XPT2046 calibration with rotation/mirror mapping and bounded pixel coordinates. Commit `2250209` (#141).
- Execute discarded zero-argument native calls so display initialization and RNG state advancement are not dropped. Commit `20d64f9` (#143).
- Clean cached firmware objects when hardware response-file contents change. Commit `20d64f9` (#143).

### Graphics, exceptions, and portability

- Stabilize optimized MinGW Win64 exception jumps using the platform-aware `vyne_setjmp` path. Commit `b349223` (#146).
- Isolate qualified interface layouts so `vraylib.Image` and `vimage.Image` do not collide. Commit `38b43c0` (#147).
- Preserve thrown string/array/map/struct graphs across managed frame cleanup and application shutdown, including shared/cyclic graphs. Commit `38b43c0` (#147).
- Refresh managed sampler/model bindings when logical render targets replace storage. Commit `38b43c0` (#147).
- Correct Sector Shift's fog baseline, immediate blackout freezes, one-shot radio state, and bounded frame-independent ending fades. Commits `6ce9f2a`, `b676295`.
- Correct hosted Windows xmake configuration and Darwin process-memory/value-conversion build issues. Commit `b349223` (#146).

### CI and documentation

- Restrict compiler source collection so standalone hardware tests do not introduce a second `main`; run calibration regressions separately and remove trigraph warnings. Commit `e246b1c`.
- Remove an unused Windows-only header exposed by Linux generated-program testing. Commit `20d64f9` (#143).
- Separate plotting PNG directories from extensionless Linux executables. Commit `6b80291` (#145).
- Track the previously ignored documentation package/navigation manifests, reject unknown internal page IDs, and repair relocated manual links and extracted AVR anchors. Commit `cc7905d` (#148).
- Correct root README build commands, interpreter modes, repository links, prerequisites, and stale embedded-library descriptions. Commit `774aec1`.

---

## Runtime and memory

### Borrowed and reusable typed storage

Slices add a non-owning pointer-and-length ABI alongside unchanged typed-array containers. Capacity management remains arena-backed; embedded growth checks available arena space. Views require stable live owner storage. Commit `b496955` (#135).

### Device frame reuse

Training buffers, saved activations, gradients, loss storage, and optimizer moments can be allocated once and reused inside the owning device region. Device reduction and crop staging avoid unnecessary host round trips. Commits `13bd9b8` (#136), `1ce2dee` (#137).

### Plotting resources

Plots own their series; managed charts also own reusable output surfaces. Native plotting storage survives host region rewinds and is explicitly released. Steady-state append/render/update reuse reserved storage. Commit `6b80291` (#145).

### Managed graphics frames

Native resources and persistent caches outlive transient Vyne frames. App frame transitions reclaim transient arena data, update music, resize following targets, and unwind active passes. Exception promotion preserves error values in commit storage across cleanup. Commit `38b43c0` (#147).

### Recorded measurements

- Optimized CPU convolution moved approximately 2.00 → 1.74 ms for the recorded 64×64×3 → 32-channel 3×3 workload. Commit `13bd9b8` (#136).
- The recorded RTX 5060 device convolution comparison measured approximately 11.53 ms CPU versus 1.07 ms GPU end-to-end for its stated workload. Commit `13bd9b8` (#136).
- HW-950 managed plotting acceptance recorded 385 frames in 60 seconds, stable 33,760 native bytes, eight setup allocations, and zero Vyne arena growth. Commit `6b80291` (#145).
- Initialized HW-950 touch acceptance recorded eight in-bounds positive-pressure contacts, 119 heartbeats, and stable 32-byte arena usage over 240 seconds. Commit `20d64f9` (#143).

These are workload- and hardware-specific observations, not universal timing guarantees.

---

## Documentation

- **61-page manual.** Publish ESP32, universal plotting, `vtensor`, `vnn`, managed/native `vraylib`, ESPCN, and graphics showcase pages; refresh navigation, pagers, and search. Commit `cc7905d` (#148).
- **Documentation freshness CI.** Isolated rebuilds detect stale, missing, and retired generated files and validate local resources, anchors, duplicate IDs, navigation, and search coverage. Commit `cc7905d` (#148).
- **Executable Markdown examples.** Add a runner for slices, typed buffers, device helpers, tensor metadata, MSE, PNG plotting, and compile-only interactive graphics. Commit `cc7905d` (#148).
- **Hardware guides.** Document setup, precedence, presets, display/touch configuration, calibration, and recorded COM5 acceptance. Commits `2250209` (#141), `e5bdb2c` (#142), `20d64f9` (#143).
- **ML guides.** Clarify CPU `vml` versus device `vtensor`/`vnn`, and document ESPCN checkpoints, reproduction, metrics, and determinism scope. Commits `2da8412` (#139), `55e6747` (#140), `0fd74f9`.
- **README refresh.** Update execution paths, library groups, workflows, prerequisites, project layout, documentation links, and contribution guidance. Commit `774aec1`.
- **Sponsorship policy.** Add `SPONSORS.md` with recognition, intake, allocation, and technical-independence rules; recognize Zeka Shamilli as Founding Sponsor. Commit `8fac133`.

---

## Internal changes

### Compiler and tooling

- Generalize target selection and demand-driven embedded features for ESP32. Commit `61a14ae` (#134).
- Add slice types, view construction, native dispatch, and reserve/resize lowering. Commit `b496955` (#135).
- Add device helper contexts, imported-helper handling, and training-intrinsic lowering. Commit `1ce2dee` (#137).
- Add schema-aware hardware configuration, bundled presets, initializer choices, and firmware staging. Commit `e5bdb2c` (#142).
- Add reproducible raylib binding generation, API inventory, native cache builds, checked resources, and managed helper bindings. Commits `b349223` (#146), `38b43c0` (#147).
- Add checked named-function descriptors and callable parameter dispatch for compiled callbacks. Commit `38b43c0` (#147).

### Tests

- ESP32 peripheral sketches, native runtime/touch fixtures, hardware runners, Python configuration checks, and host calibration/orientation regressions.
- Source-tree and installed-distribution initialization, hardware precedence, validation, upload, and monitoring acceptance.
- Slice/reserve/resize regression.
- Device forward/backward parity, Float32/Float64 operations, reductions, crop staging, MSE, SGD, AdamW, and imported `@device_fn` coverage.
- CPU `vml` training and reusable `vtensor`/`vnn` smoke tests.
- Plotting lifecycle, memory, decoded PNG, and 128 chart/theme/viewport/band parity cases.
- Graphics resource/pointer ownership, callbacks, structured exceptions, resize, uniforms, GPU readback, and native sanitizer checks.
- Sector story thresholds, freezes, radio, fades, shader pixels, saturation DSP, and full local autoplay/audio cleanup.
- Five documentation-pipeline regressions and fresh-checkout generation checks.

### Repository maintenance

Hardware and plotting planning documents are added and reconciled with implementation; generated project scaffolds and example formatting are maintained, and stale build/sketch outputs are ignored. The initial commit in this window, `cacd78a`, synchronizes v0.7.0 version references and introduces no runtime feature.

---

## Breaking changes

No intentional broad source-language breaking change is introduced in this window. The following behavioral and ownership boundaries matter when upgrading:

- **Embedded native surfaces are closed.** Unsupported members now fail during Vyne compilation instead of at the C link stage.
- **ESP32 arena default is 64 KB.** Applications relying on the earlier 128 KB assumption must configure and validate their memory budget.
- **Hardware initialization has configured defaults.** No-argument display initialization uses resolved settings; explicit rotation arguments remain authoritative.
- **`vynec run` uploads configured firmware.** Use `build` for compile-only behavior and request monitoring explicitly.
- **Slices are borrowed views.** They do not own storage, grow, or preserve data after owner reclamation.
- **Managed graphics wrappers share resources.** Copying a wrapper shares its token; closing the resource invalidates aliases.
- **Managed frames reclaim transient host data.** Long-lived application containers must be allocated outside the frame or explicitly preserved.
- **Native target attachment handles can become stale after resize.** Prefer managed logical views.
- **ESPCN inference prefers generated checkpoints.** Existing best/latest build checkpoints take precedence over bundled weights.

---

## Upgrade guide

1. **Rebuild the compiler and generated programs** together so runtime headers, device PTX, and graphics bindings match.
2. **Select embedded hardware once.** Initialize a board/preset, inspect `hardware show`, install dependencies with `hardware setup`, then use `build` and `run` from the project directory.
3. **Rebuild and reflash ESP32 firmware.** Review the 64 KB arena, target-specific peripherals, display geometry, and explicit calibration workflow.
4. **Reserve buffers before borrowing views.** Keep slice owners alive and storage stable; use reserve/resize to reuse typed capacity.
5. **Keep device training explicit.** Allocate buffers inside the device region, initialize optimizer moments, and use `vtensor`/`vnn` for reusable blocks.
6. **Try the ESPCN reproduction scripts.** They clean build outputs and validate the bundled training/checkpoint/inference lifecycle.
7. **Migrate graphical applications through the managed App guide.** Pair cleanup, respect resource dependencies, and keep transient values within frame lifetimes.
8. **Use the new plotting facade.** Import `vplot/vplot.vy` for host/display/live charts; retain `vtermplot` for terminal output.
9. **Regenerate documentation after source/manual edits.** Run `npm run sync`, `npm test`, and `npm run check` in `docs/`, then commit generated Pages output.
10. **Synchronize release versions before publishing.** The current compiler/toolchain/site metadata still identify v0.7.0; update release-facing references to v0.8.0.

---

## Known limitations

- **Device training is not automatic tensor AD.** The reusable ML surface targets explicit HWC image graphs and the documented convolution/activation/optimizer operations.
- **ESPCN training is a fixed-crop smoke workflow.** It is not multi-image dataset training or a general super-resolution quality claim. Full-device inference currently targets ReLU checkpoints.
- **Determinism is environment-scoped.** Bit-exact repeatability describes the fixed bundled graph on the same GPU and driver; other environments use tolerances.
- **Device handles must remain region-local.** Mixed dtypes, asynchronous streams, and multi-GPU scheduling are not provided.
- **Shipped slice function parameters support Float64 and Int64.** `Slice<Float32>` parameters are still rejected despite runtime view storage.
- **Embedded hardware remains configuration-dependent.** Pin availability, display controllers, touch wiring, sensors, and network credentials require the appropriate board/peripheral setup.
- **Plotting scales are linear.** Font coverage and native resource limits remain bounded; sample acquisition and synchronization belong to the application.
- **`vraylib` is compiled desktop-only.** Rendering needs OpenGL; arbitrary foreign callbacks and unsupported OpenGL 4.3 compute/SSBO operations are unavailable.
- **Only one managed graphics application/context owner may be live at once.** Raw and managed render-pass composition must follow the documented state rules.
- **Bodycam and Sector Shift require local media.** Their historical assets are not included in the repository.
- **Existing AD limitations remain.** This release does not expand implicit differentiation to vector residuals or remove the documented straight-line AD restrictions.

---

## Contributors

- **Tuncay Gafarli** - all commits in this release.
- **Zeka Shamilli** - Founding Sponsor, recognized in `SPONSORS.md`.

## Full commit list

See `git log v0.7.0..HEAD` for the raw list. The 28 commits this release covers span `cacd78a` (2026-11-11) through `774aec1` (2026-11-15), including release housekeeping, merged feature work, examples, tests, documentation, and repository maintenance.

**Full Changelog**: <https://github.com/t2ncay/vyne/compare/v0.7.0...v0.8.0>

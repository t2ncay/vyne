# Focused throw-state audit

## Confirmed and fixed

`compileZeroedRegion` in `compiler/codegen/regions.cpp` increments the thread-local
`g_arena_zero_mode`; `@deterministic` uses the same lowering. A `longjmp` bypasses
the decrement. The existing checkpoint/persistent restoration did not restore
this independent counter.

`compiler/codegen/exceptions.cpp` now snapshots zero-mode depth at try entry and
reduces it to that depth after a throw from try or catch. A finally throw lands in
the enclosing handler, which applies its own snapshot. Enclosing zeroed scopes
remain active; abandoned nested scopes do not affect subsequent allocations.

Evidence: the observed-C state test failed before the fix on the first nested
throw (`g_arena_zero_mode == 1` assertion). With the fix it passes at `-O3` for
exact depths 0/1/2, repeated recovery, a finally throw, and ordinary and managed
frame execution. Observations replace only explicit test marker calls with C
assertions; production APIs and exception lowering are not mocked.

## Bounded accounting and special allocator state

- `@bounded` has no global active-budget state to leak: start/budget are emitted
  automatic locals. The arena counter is retained rather than reset by catch
  restoration. Surviving enclosing bounded regions therefore still charge
  allocations from abandoned inner scopes. The focused test verifies this and
  successful normal closure of an enclosing bound.
- Abandoned pool/slab/ring scopes have local allocator structs and arena-backed
  storage, without a separate global allocator selector. Compiler context stacks
  are lexical, not runtime state. Checkpoint restoration balances the abandoned
  scopes; backing allocations remain alive until an enclosing rewind. The test
  abandons all three in a called function, then uses fresh instances successfully.
- A thrown-out `@bounded` scope still skips its exit-only ceiling check. This
  change does not add contract checks to exceptional exits.
- Surviving scopes have a separate pre-existing C `setjmp` limitation: an enclosing
  bounded scratch-budget local, or pool/slab/ring allocator struct, modified after
  `setjmp` is a non-volatile automatic object. Its value can become indeterminate
  after `longjmp`. The depth snapshots do not make such surviving mutable locals
  exception-safe or roll back their allocations. Addressing their declarations
  and storage would require the separately owned region emitter/runtime policies;
  no such redesign was made here.
- Device/stream frame cleanup and general defer-on-throw remain outside this fix.

## Production ownership across `longjmp`

`emitter.h:protectTryLocals` qualifies the handler's already-declared visible C
bindings with object-level `volatile`, including native integer handles, boxed
VyneValues and native POD resource wrappers. The qualifier follows the type so
it qualifies a pointer itself. True globals remain unchanged. Locals created
inside a try do not need this treatment for that handler; an inner try captures
and protects them when necessary. The mutable generated exception error/flag
locals and pending-return slots are also protected. Region snapshots are not
modified between setjmp and longjmp and retain their existing declarations.

The `exception_ownership.vy` regression performs real neutral-bridge faults after
raw allocations in try/catch and after partial App/Image startup. At `-O3`, finally
observes the exact allocated handle and every case leaves zero live handles. The
runner additionally requires the corresponding emitted volatile declarations,
so it fails on the previous compiler even where a particular GCC build happened
to spill the plain locals safely.

Production inspection in `build/runtime-ownership-hardening/studio-inspect.vy.c`
confirmed volatile `se_text_write.points`, `se_import_read_bytes.data`,
`se_preview_request.image` and `se_scene_editor.app`, with no VMGE source edits.
Generated bound counters and special allocator structs remain the separate
policy-local limitation described above.

## Native font failure ownership

Pinned raylib's `UnloadFont` already ignores its default texture, so the previous
fallback guard did not actually unload the default font. Managed loading now
checks fallback explicitly without invoking `UnloadFont` on it, and also rejects
an invalid GPU texture. Font metadata and cache records are allocated as tracked
bridge temporaries before loading; the empty font owner is registered before GPU
allocation. No bridge allocation can fail after a successful GPU font load.

`vr_temp` links its zeroed tracking node before allocating data, ensuring a failed
data allocation also frees the node. Ownership transfer detaches tracking nodes
only after the registry or cache owns their data. Failed font loading releases its
empty registered owner and leaves cache metadata to temporary cleanup.

Fault-injected tests cover every pre-load allocation, registry growth, an existing
unsupported file returning the default fallback, cached success and session close.
The default font is not unloaded; a successful custom font is unloaded once.

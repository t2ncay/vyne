# Ownership cleanup and atomic text writes

After rebuilding the compiler:

```sh
python vyne/runtime/tests/run_ownership_hardening.py
python vyne/runtime/tests/run_vmge_isolated.py --graphics --poison
```

The focused runner checks emitted longjmp-safe declarations and native faults
after raw handle, App and Image allocation at `-O3`. It also checks the compiled
`vfs.write_atomic` API and native injected short-write, close, fdopen and replace
failures: original contents retained, descriptor closed, no temporary left.
Windows additionally tests an existing sibling-name collision and its retention.
Ordinary `vfs.write` must report close failure too.

The acceptance wrapper runs the maintained VMGE runner. Outputs use
`build/runtime-ownership-hardening/vmge-acceptance`; two temporary fixture copies
change only hardcoded generated-file destinations and resolve imports to the
same production sources. No VMGE files are edited. The native cache is isolated
under `build/runtime-ownership-hardening/cache`. Set `VYNEC` for another compiler.

`vfs.write_atomic(path,text)` preserves the destination on failure and returns
`Bool`, using an exclusively created sibling and replacement after checked close.
Windows uses the existing narrow-path convention; POSIX uses `mkstemp`/`rename`.
The native tests have platform-specific fault hooks for both implementations.

See `REGION_THROW_AUDIT.md` for production declaration evidence and the remaining
special allocator/bounded-policy local-state scope.

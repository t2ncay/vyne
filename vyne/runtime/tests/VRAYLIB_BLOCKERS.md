# Focused managed runtime checks

Build the compiler after changing `compiler/codegen/exceptions.cpp`, then run:

```sh
python vyne/runtime/tests/run_vraylib_blockers.py
python vyne/runtime/tests/run_vraylib_blockers.py --font /path/to/Latin-extended.ttf
```

The second command also runs hidden-window graphics checks. The supplied font
must contain Azerbaijani/Turkish glyphs (Inter-Regular works). Set `VYNEC` to use
a different compiler binary. Binaries and the native cache use
`build/runtime-vraylib-blockers`; the existing managed test's generated PNG uses
its original `build/vraylib-tests` fixture path. No VMGE runner is invoked.

- `vraylib_recovery.c`: native validation failure leaves depth untouched, balanced
  nesting and underflow/overflow, frame/close/shutdown reset, stale App closure,
  4096 bounded short-string promotions, ring rethrow, 1023/1024-byte boundary,
  long/structured payload survival.
- `vraylib_recovery.vy`: 256 recoveries through called nested regions in one frame,
  catch-to-finally checkpoint restoration, rethrow, persistent nesting, unchanged
  commit-arena bytes for short recovery messages, plus ordinary exceptions without
  a managed promoter and ordinary persistent payload survival.
- `vraylib_font.vy`: Unicode glyph coverage, single/multiline measurement, raster
  size caching, missing/invalid loads, drawing, stale aliases, reload/cleanup and
  orthographic view preservation.
- Existing managed cleanup and try/finally tests, plus generated-binding `--check`.
- `region_exception_state.vy` / `check_region_state.py`: assert exact runtime
  zero-mode depths in test-only emitted C at `-O3`; nested zeroed/deterministic
  scopes, 128 repeated recoveries, finally throw, ordinary/managed recovery,
  retained enclosing bound accounting and fresh pool/slab/ring use after throw.
- `vraylib_font_failure.c`: compile the real bridge with allocation fault hooks;
  all eight pre-load temporary/metadata failures and registry-growth failure leave
  heap/handle state unchanged; unsupported-file fallback does not unload the
  default font; successful cached custom font is unloaded exactly once.

Catch restoration intentionally pops abandoned checkpoints without rewinding
their arena: ordinary exception payloads can still refer to those allocations.
Enclosing regions and managed frames reclaim them later. Device/stream-specific
resource unwinding and general defer-on-throw behavior are outside this focused
compiler change.

See `REGION_THROW_AUDIT.md` for the narrow scope and remaining policy limitations.

## Deferred native close (standalone focused checks)

`vraylib_deferred_close.c` compiles the real bridge with a local synthetic
`WindowShouldClose` hook. Link it against the cached `libraylib.a` (not
`libvyne_raylib.a`, since the bridge is included). It checks default termination,
pending-event consumption, unread event retention, consecutive consuming-backend
events, cancellation after GLFW-style event polling, disabling the option,
definitive stop/frame limits, actual window loss and session cleanup.

`vraylib_deferred_close.vy` checks the two App wrapper methods and headless
stop/limit behavior. These can be compiled and run independently of the full
runner. The existing `vraylib_font.vy` and `vraylib_font_failure.c` provide focused
font cleanup checks after changes to the managed dispatcher.

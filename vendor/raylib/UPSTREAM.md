# raylib upstream provenance

- Repository: https://github.com/raysan5/raylib
- Release: **6.2** (2026-10-15)
- Commit: `02566453880e06bec97f5b8160fa6fe1e98397ca`
- License: upstream zlib/libpng license; see `LICENSE` and individual bundled dependencies.

The upstream build files, `cmake/`, and `src/` are vendored verbatim. Vyne's
binding and build logic lives outside this directory. Update the pin and rerun
`python scripts/generate_vraylib.py` when updating upstream.

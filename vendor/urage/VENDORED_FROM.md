URAGE vendored source
=====================

Source repository: https://github.com/abdullahtnz/urage
Vendored branch: `main`

Included components:
- `core/include/*`
- `core/src/*`
- `bindings/cpp/urage.hpp`
- `LICENSE`

Purpose in Vyne:
- Build a bundled `liburage.so` (Linux/macOS) or `urage.dll` (Windows) during Vyne build.
- Allow the `vurage` native module to work out-of-the-box without manual external install.

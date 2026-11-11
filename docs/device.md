# `@device` regions — CUDA accelerator backend

`@device` is the region policy that scopes work to an accelerator and back. The
first implementation ships a **direct CUDA driver backend** with a CPU fallback.
It does not depend on ggml or on CUDA headers/link flags in the generated C.
**Float32 is the default device dtype**; `device_alloc_f64` keeps the Float64
path available.

```vyne
@device<cuda> region gpu {
    n :: Int64 := 1024;

    a :: Array<Float32> := [];
    b :: Array<Float32> := [];
    c :: Array<Float32> := [];
    through i :: 0..n-1 -> loop {
        a.push(float32(i));
        b.push(float32(2 * i));
        c.push(0.0f);
    };

    da :: Int64 := device_alloc(n);       # Float32 by default
    db :: Int64 := device_alloc(n);
    dc :: Int64 := device_alloc(n);

    device_upload(da, a, n);
    device_upload(db, b, n);
    device_add(dc, da, db, n);
    device_download(dc, c, n);

    out("backend: " + device_backend());
};
```

## Syntax

```text
@device region name { ... };            # backend = auto
@device<cuda> region name { ... };      # CUDA, CPU fallback if unavailable
@device<cpu> region name { ... };       # host execution
@device<auto> region name { ... };      # CUDA if available, else CPU
@device<backend, cuda> region name { ... };  # equivalent to @device<cuda>
```

Policy arguments are identifiers/integers, so the backend name is an identifier
(`cuda`, `cpu`, `auto`), not `backend: cuda`. Unknown names fall back to CPU with
a warning.

`@device` is also a normal region for host allocations: the region takes a host
arena checkpoint on entry and rewinds it on exit. Device allocations live in the
device arena and are released when the device frame closes. The device frame is
closed on early returns and on `break`/`continue` before the host rewind.

`@device` is rejected on the AVR target.

## Intrinsics

All handles are `Int64` pointer values. Host buffers must be typed
`Array<Float32>` or `Array<Float64>` values. **Float32 is the default device
dtype**; use `device_alloc_f64(n)` for a double-precision allocation. An
operation's handles must all have the same dtype.

Float32 scalars are written with the `f` suffix (`1.0f`) or with the
`float32(x)` conversion. Float32 stays Float32 only when both operands are
Float32; mixing with `Float64` or `Int64` promotes to `Float64`.

| Intrinsic | Meaning |
| --- | --- |
| `device_alloc(n)` | allocate `n` **Float32** elements in the device arena |
| `device_alloc_f64(n)` | allocate `n` Float64 elements (opt-in) |
| `device_upload(h, host, n)` | copy `n` elements from a host typed array to `h`; dtype must match |
| `device_download(h, host, n)` | copy `n` elements from `h` to a host typed array; dtype must match |
| `device_add(out, a, b, n)` | `out = a + b`, dtype-dispatched |
| `device_sub(out, a, b, n)` | `out = a - b`, dtype-dispatched |
| `device_mul(out, a, b, n)` | `out = a * b`, dtype-dispatched |
| `device_scale(out, a, s, n)` | `out = a * s`, dtype-dispatched |
| `device_relu(out, a, n)` | `out = max(a, 0)`, dtype-dispatched |
| `device_fill(h, v, n)` | fill with `v`, dtype-dispatched |
| `device_mse_grad(grad, partial, pred, target, n)` | MSE gradient and per-element mean-loss contributions, dtype-dispatched |
| `device_sgd_step(param, grad, lr, n)` | in-place SGD update, dtype-dispatched |
| `device_matmul(out, a, b, m, n, k)` | `C[m,n] = A[m,k] * B[k,n]`, row-major, dtype-dispatched |
| `device_conv2d(out, x, w, b, in_h, in_w, in_c, out_h, out_w, out_c, k, stride, pad, has_bias)` | HWC conv2d, OIHW weights, square kernel, zero padding, dtype-dispatched |
| `device_sync()` | wait for outstanding device work |
| `device_bytes()` | live device-arena bytes |
| `device_peak_bytes()` | peak device-arena bytes |
| `device_backend()` | `"cuda"` or `"cpu"` |
| `device_available()` | whether CUDA initialized successfully |

The intrinsics are intercepted by name only while a `@device` region is active,
so a user-defined `device_add()` outside the region still resolves normally.

## Device arena semantics

- The region opens a device frame on entry and stores the current allocation-list
  mark.
- `device_alloc` allocates on the active backend and records the allocation.
- At region close, the device frame frees every allocation made after the mark,
  in reverse order, then restores the previous backend.
- Nested `@device` frames are supported (up to 16); inner frames free their own
  allocations.
- A handle is invalid after its region closes. Do not store a device handle in a
  variable that outlives the region.
- Host arrays allocated inside the region keep the normal host lifetime and are
  reclaimed by the host rewind.

## Backends

### CUDA (dynamic driver API)

- Loads `nvcuda.dll` on Windows or `libcuda.so.1` on Linux at first use.
- Uses `cuInit`, `cuDeviceGet`, `cuCtxCreate_v2`, `cuMemAlloc_v2`,
  `cuMemcpyHtoD_v2` / `cuMemcpyDtoH_v2`, `cuModuleLoadDataEx`,
  `cuModuleGetFunction`, `cuLaunchKernel`, and `cuCtxSynchronize`.
- Kernels are compiled to PTX at build time and embedded in
  `vyne/runtime/detail/vyne_cuda_ptx.h`; the driver JIT-compiles the PTX on the
  target GPU.
- The generated Vyne C does **not** link against CUDA and needs no CUDA include
  path.
- If the driver, a device, or the PTX module is unavailable, `@device<cuda>`
  prints a warning and falls back to the CPU backend.

### CPU fallback

- Allocations use `malloc`; operations use scalar loops.
- Exists so device-region source is portable and CI can exercise the region
  lifecycle without a GPU.

## Regenerating the PTX

The kernels live in `vyne/runtime/device/cuda/kernels.cu`. The embedded header is
generated by:

```powershell
pwsh -File scripts/build_cuda_ptx.ps1
```

The script enters the Visual Studio developer environment (`vcvars64.bat`) and
invokes `nvcc -ptx -arch=compute_80`. Requirements: the CUDA Toolkit,
`nvcc`, and the Visual Studio C++ build tools. The generated header is committed
so ordinary builds do not need nvcc.

## Examples and tests

```bash
./vynec --compile examples/device/gpu_elementwise.vy
./examples/device/gpu_elementwise

./vynec --compile examples/device/gpu_matmul.vy
./examples/device/gpu_matmul

./vynec --compile examples/benchmark/device_matmul_bench.vy
./examples/benchmark/device_matmul_bench

./vynec --compile examples/test/device_test.vy
./examples/test/device_test

./vynec --compile examples/test/float32_test.vy
./examples/test/float32_test
```

### Performance honesty

Measured on an RTX 5060 (CUDA 13.4) and an i7-14700 with OpenBLAS. The default
**Float32** path is compared against `sgemm`:

| Workload | GPU kernel | GPU download | GPU end-to-end | OpenBLAS sgemm | Naive Vyne f32 loop |
| --- | --- | --- | --- | --- | --- |
| 512x512 | 167 us (1607 GFLOP/s) | 155 us | 322 us | 626 us (429 GFLOP/s) | 60.7 ms (4.42 GFLOP/s) |
| 1024x1024 | 1409 us (1524 GFLOP/s) | 420 us | 1829 us | 2615 us (821 GFLOP/s) | 2263 ms (0.95 GFLOP/s) |

Reading the table:

- At 512² the FP32 GPU is ~1.9x faster than OpenBLAS `sgemm` end-to-end and
  ~3.8x faster kernel-only. At 1024² it is ~1.4x faster end-to-end and ~1.9x
  faster kernel-only.
- The GPU kernel is ~1.5 TFLOP/s, far below the consumer FP32 peak. The next
  performance levers are vectorized loads, larger tiles, register blocking, and
  asynchronous copies; the current kernel is the same simple 16x16 tile as the
  FP64 path.
- The naive Vyne CPU loop is not a fair baseline; it is shown only to make the
  staging cost visible.
- Keeping tensors device-resident across many kernels amortizes the
  0.16-0.42 ms download cost.

**Float64 is opt-in** via `device_alloc_f64`. On this hardware it is roughly at
parity with OpenBLAS `dgemm` (see the earlier finding: ~1.8x faster at 512²,
~8% slower at 1024²), because consumer Blackwell FP64 throughput is heavily
reduced. Use Float64 only when double precision is required.

Maximum element error is around 7.6e-6 at 512² and 1.5e-5 at 1024² for Float32,
and around 1.4e-14 / 2.8e-14 for Float64.
## Limitations and next steps

- **Explicit intrinsics only.** This release does not automatically classify
  staged tensors or build an operation list from arbitrary region arithmetic.
  The report's automatic staging/op-list codegen is the next milestone.
- **Float32 default, Float64 opt-in.** Float16/BFloat16 are not implemented; the
  device API rejects mixed-dtype handle sets rather than silently converting.
- **No device pointers in host data structures.** Handles are integers; the
  escape checker does not know they name device memory. Keep them region-local.
- **`scratch` is host-only.** Do not use `scratch` inside a `@device` region.
- **No async streams or multi-GPU scheduling yet.** The implementation uses the
  default stream and device 0. `device_sync` is synchronous.
- **No ggml dependency.** The runtime boundary (`vyne/runtime/modules/vdevice.h`)
  is the place a ggml backend or scheduler would plug in. The explicit
  stage-in/compute/stage-out lifecycle maps directly onto ggml's backend API, so
  the report's ggml integration remains a valid follow-up rather than a rewrite.
- **AVR is not a target.** `@device` requires a desktop runtime.

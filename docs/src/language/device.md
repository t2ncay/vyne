---
id: device
title: CUDA & Devices
group: Language Reference
icon: fa-microchip
order: 14
description: The @device region policy, device intrinsics, the CUDA driver backend, and the CPU fallback.
---

# CUDA & Devices

`@device` is a region policy that scopes work to an accelerator and back. The implementation ships a **direct CUDA driver backend** with a CPU fallback. It does not depend on CUDA headers or link flags in the generated C. **Float32 is the default device dtype**; `device_alloc_f64` opts into doubles.

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
@device region name { ... };                   # backend = auto
@device<cuda> region name { ... };             # CUDA, CPU fallback if unavailable
@device<cpu> region name { ... };              # host execution
@device<auto> region name { ... };             # CUDA if available, else CPU
@device<backend, cuda> region name { ... };    # equivalent to @device<cuda>
@device_fn fn helper(...) { ... }              # requires an active device frame
```

Policy arguments are identifiers or integers, so the backend is `cuda`, `cpu`, or `auto`, not `backend: cuda`. Unknown names fall back to CPU with a warning.

A `@device` region is also a normal host region: it takes a host arena checkpoint on entry and rewinds it on exit. Device allocations live in the device arena and are released when the frame closes, including early returns and `break` / `continue`.

`@device` is rejected on AVR.

## Intrinsics

All handles are `Int64` pointer values. Host buffers must be typed `Array<Float32>` or `Array<Float64>`. An operation's handles must all have the same dtype.

| Intrinsic | Meaning |
| :--- | :--- |
| `device_alloc(n)` | Allocate `n` **Float32** elements |
| `device_alloc_f64(n)` | Allocate `n` Float64 elements |
| `device_upload(h, host, n)` | Copy `n` elements from a host array to `h` |
| `device_download(h, host, n)` | Copy `n` elements from `h` to a host array |
| `device_add(out, a, b, n)` | `out = a + b` |
| `device_sub(out, a, b, n)` | `out = a - b` |
| `device_mul(out, a, b, n)` | `out = a * b` |
| `device_scale(out, a, s, n)` | `out = a * s` |
| `device_relu(out, a, n)` | `out = max(a, 0)` |
| `device_fill(h, v, n)` | Fill with `v` |
| `device_sum(out, input, n)` | Reduce `n` elements to one preallocated device scalar |
| `device_copy_slice(dst, src, src_offset, n)` | Stage a contiguous packed-buffer section on the device |
| `device_mse_grad(grad, partial, pred, target, n)` | MSE gradient and per-element mean-loss contributions |
| `device_sgd_step(param, grad, lr, n)` | In-place device SGD update |
| `device_adamw_step(param, m, v, grad, lr, beta1, beta2, eps, weight_decay, step, n)` | In-place AdamW with device-resident moments |
| `device_matmul(out, a, b, m, n, k)` | Row-major `C[m,n] = A[m,k] * B[k,n]` |
| `device_conv2d(out, x, w, b, in_h, in_w, in_c, out_h, out_w, out_c, k, stride, pad, has_bias)` | HWC conv2d with OIHW weights, square kernel, zero padding |
| `device_conv2d_backward_input(out, grad, weight, in_h, in_w, in_c, out_h, out_w, out_c, k, stride, pad)` | Preallocated HWC input gradient |
| `device_conv2d_backward_weight(out, input, grad, in_h, in_w, in_c, out_h, out_w, out_c, k, stride, pad)` | Preallocated OIHW weight gradient |
| `device_conv2d_backward_bias(out, grad, out_h, out_w, out_c)` | Preallocated bias gradient |
| `device_relu_grad(out, grad, pre, n)` | ReLU backward using saved preactivations |
| `device_pixel_shuffle(out, input, h, w, c, r)` | ESPCN depth-to-space |
| `device_pixel_shuffle_backward(out, grad, h, w, c, r)` | ESPCN pixel-shuffle backward |
| `device_sync()` | Wait for outstanding work |
| `device_bytes()` | Live device-arena bytes |
| `device_peak_bytes()` | Peak device-arena bytes |
| `device_backend()` | `"cuda"` or `"cpu"` |
| `device_available()` | Whether CUDA initialized successfully |

The intrinsics are intercepted by name only while a `@device` region is active, so a user-defined `device_add()` outside the region still resolves normally.

## Reusable `@device_fn` helpers

`@device_fn` marks a host-side helper allowed to issue device intrinsics. It is
not an arbitrary GPU kernel definition. Call it inside an active `@device`
region or another `@device_fn`. Helpers can live in imported files and call one
another. A normal top-level call is rejected because there is no device frame.
Pass caller-owned handles and dimensions; keep handles within their region.

```vyne title="device-helper.vy"
@device_fn
fn twice(dst :: Int64, src :: Int64, n :: Int64) {
    device_scale(dst, src, 2.0, n);
}

input :: Array<Float32> := [1.0f, 2.0f, 3.0f];
output :: Array<Float32> := [0.0f, 0.0f, 0.0f];
@device<cuda> region work {
    src :: Int64 := device_alloc(3);
    dst :: Int64 := device_alloc(3);
    device_upload(src, input, 3);
    twice(dst, src, 3);
    device_download(dst, output, 3);
};
out(output);  # [2, 4, 6]
```

## Device tensor and neural-network layers

| Layer | Use it for |
| :--- | :--- |
| [vml](page:vml) + [vlin](page:vlin) | CPU dense/MLP models over typed matrices |
| [voptim](page:voptim) | CPU optimizer states over flat arrays |
| [vtensor](page:vtensor) | Device handle plus HWC dimensions and element count; no ownership |
| [vnn](page:vnn) | Explicit preallocated conv/ReLU/pixel-shuffle, loss, backward, and AdamW blocks |
| [vconv](page:vconv) | CPU tensor wrappers, model loading, and explicit device primitives |

Device training is explicit. Allocate parameters, activations, saved intermediates,
gradients, moments, and loss buffers once inside the device region. Upload inputs
and parameters, run forward/backward/update steps, and download only results,
loss scalars, or checkpoints. `device_sum` reduces loss without downloading an
entire image. `device_copy_slice` stages a crop from an already uploaded buffer.
Initialize moments explicitly with `device_fill` before the first AdamW step.
There is no automatic tensor AD or implicit CPU/GPU dispatch for `vconv.conv2d`.

The [ESPCN showcase](page:espcn) exercises the full graph and checkpoint reload.
On the same GPU and driver its fixed bundled graph is documented as bit-exact
deterministic; compare different devices/drivers using metric tolerances.

## Device arena semantics

- The region opens a device frame and stores the current allocation mark.
- `device_alloc` allocates on the active backend and records the allocation.
- At region close, the frame frees every allocation made after the mark in reverse order, then restores the previous backend.
- Nested `@device` frames are supported up to 16; inner frames free their own allocations.
- A handle is invalid after its region closes. Do not store it in a variable that outlives the region.
- Host arrays allocated inside the region keep normal host lifetime and are reclaimed by the host rewind.

## Backends

### CUDA (dynamic driver API)

- Loads `nvcuda.dll` on Windows or `libcuda.so.1` on Linux at first use.
- Uses `cuInit`, `cuDeviceGet`, `cuCtxCreate_v2`, `cuMemAlloc_v2`, `cuMemcpyHtoD_v2` / `cuMemcpyDtoH_v2`, `cuModuleLoadDataEx`, `cuModuleGetFunction`, `cuLaunchKernel`, and `cuCtxSynchronize`.
- Kernels are compiled to PTX at build time and embedded in `vyne/runtime/detail/vyne_cuda_ptx.h`; the driver JIT-compiles the PTX on the target GPU.
- Generated Vyne C does **not** link against CUDA and needs no CUDA include path.
- If the driver, a device, or the PTX module is unavailable, `@device<cuda>` prints a warning and falls back to the CPU backend.

### CPU fallback

- Allocations use `malloc`; operations use host implementations, including optimized convolution paths.
- Exists so device-region source is portable and CI can exercise the region lifecycle without a GPU.

## Regenerating the PTX

The kernels live in `vyne/runtime/device/cuda/kernels.cu`. The embedded header is generated by:

```powershell
pwsh -File scripts/build_cuda_ptx.ps1
```

The script enters the Visual Studio developer environment and invokes `nvcc -ptx -arch=compute_80`. The generated header is committed, so ordinary builds do not need `nvcc`.

## Performance

Measured on an RTX 5060 (CUDA 13.4) and an i7-14700 with OpenBLAS. The default **Float32** path is compared against `sgemm`:

| Workload | GPU kernel | GPU download | GPU end-to-end | OpenBLAS sgemm | Naive Vyne f32 loop |
| :--- | ---: | ---: | ---: | ---: | ---: |
| 512×512 | 167 µs (1607 GFLOP/s) | 155 µs | 322 µs | 626 µs (429 GFLOP/s) | 60.7 ms (4.42 GFLOP/s) |
| 1024×1024 | 1409 µs (1524 GFLOP/s) | 420 µs | 1829 µs | 2615 µs (821 GFLOP/s) | 2263 ms (0.95 GFLOP/s) |

Reading the table:

- At 512² the FP32 GPU is ~1.9× faster than OpenBLAS `sgemm` end-to-end and ~3.8× faster kernel-only.
- At 1024² it is ~1.4× faster end-to-end and ~1.9× faster kernel-only.
- Keeping tensors device-resident across many kernels amortizes the 0.16–0.42 ms download cost.

**Float64 is opt-in** via `device_alloc_f64`. On consumer Blackwell it is roughly at parity with OpenBLAS `dgemm`: ~1.8× faster at 512² and ~8% slower at 1024². Use Float32 unless double precision is required.

Maximum element error is around 7.6e-6 at 512² and 1.5e-5 at 1024² for Float32, and around 1.4e-14 / 2.8e-14 for Float64.

## Limitations

- **Explicit intrinsics only.** The compiler does not automatically classify arbitrary region arithmetic into device operations. You stage, launch, and download explicitly.
- **Float32 default, Float64 opt-in.** Float16/BFloat16 are not implemented; mixed-dtype handle sets are rejected rather than silently converted.
- **No device pointers in host data structures.** Handles are integers; the escape checker does not know they name device memory. Keep them region-local.
- **`scratch` is host-only.** Do not use `scratch` inside a `@device` region.
- **No async streams or multi-GPU scheduling.** The implementation uses the default stream and device 0; `device_sync` is synchronous.
- **No ggml dependency.** The runtime boundary is the place a ggml backend would plug in.
- **AVR is not a target.** `@device` requires a desktop runtime.

## See also

- [Memory & Regions](page:regions)
- [SRCNN Showcase](page:srcnn)
- [ESPCN Inference & Training](page:espcn)
- [vtensor](page:vtensor) and [vnn](page:vnn)
- [Installation](page:install)

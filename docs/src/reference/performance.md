---
id: performance
title: Performance
group: Reference
icon: fa-gauge-high
order: 5
description: Measured interpreter, transpiler, BLAS, CUDA, and Arduino numbers — with the caveats.
---

# Performance

All numbers below are from the repository's benchmarks and release notes. Re-run them on your own hardware; the point is the shape of the result, not a universal claim.

## Recursive Fibonacci (n = 30)

Wall time of the `fib(30)` call, measured in-process. Process startup and codegen are excluded.

| Backend | Time | Relative |
| :--- | ---: | :--- |
| Interpreter | 54.52 ms | 1.00× |
| C transpiler | 19.87 ms | 2.74× faster |

Measured on an i7-14700, Windows 11. Transpiler build: `--blas --native`, GCC `-O3 -march=native`. Source is untyped recursion `fn fib(n)`; adding `:: Int64` does not change the transpiler result (the native path is already taken) but roughly halves interpreter time.

## Dense matmul (1024×1024, 100 iterations)

The same Vyne source is compiled three ways:

| Build | Kernel |
| :--- | :--- |
| `vynec --compile` | emitted C triple loop |
| `vynec --compile --blas` | `cblas_dgemm` |
| `vynec --compile --blas --native` | `cblas_dgemm` + host `-march=native` |

See `examples/benchmark/matmul_1024_vlin.vy`. The source is byte-identical across modes; only the lowering of `vlin.multiply` changes.

## End-to-end ML workload

`examples/training/ml_seq.vy` trains a 64→16→12→1 classifier on 240 RNA sequences for 1000 epochs. With regions scoping per-iteration allocations, peak RSS stays flat regardless of epoch count.

## CUDA matmul

Measured on an RTX 5060 (CUDA 13.4) and an i7-14700 with OpenBLAS. Float32 is the default device dtype.

| Workload | GPU kernel | GPU download | GPU end-to-end | OpenBLAS sgemm | Naive Vyne f32 loop |
| :--- | ---: | ---: | ---: | ---: | ---: |
| 512×512 | 167 µs (1607 GFLOP/s) | 155 µs | 322 µs | 626 µs (429 GFLOP/s) | 60.7 ms (4.42 GFLOP/s) |
| 1024×1024 | 1409 µs (1524 GFLOP/s) | 420 µs | 1829 µs | 2615 µs (821 GFLOP/s) | 2263 ms (0.95 GFLOP/s) |

At 512² the FP32 GPU is ~1.9× faster than OpenBLAS `sgemm` end-to-end and ~3.8× faster kernel-only. At 1024² it is ~1.4× faster end-to-end and ~1.9× faster kernel-only. The GPU kernel is ~1.5 TFLOP/s, far below the consumer FP32 peak; the current kernel is a simple 16×16 tile. Keeping tensors device-resident across many kernels amortizes the 0.16–0.42 ms download cost.

Float64 is opt-in via `device_alloc_f64`. On consumer Blackwell it is roughly at parity with OpenBLAS `dgemm`: ~1.8× faster at 512² and ~8% slower at 1024².

## TinyML XOR on an Uno R3

`examples/arduino/tinyml_xor.vy` trains a 2→3→1 MLP for 30,000 online SGD steps on an ATmega328P at 16 MHz.

| Metric | Value |
| :--- | :--- |
| Training time (30,000 steps) | ~18–25 s |
| Arena peak during training | 0 B |
| Stack peak during training | < 200 B |
| Final XOR accuracy (10 held-out) | 91% |
| Serial output per 1,000 steps | 1 line (~40 bytes) |

All arithmetic is 32-bit `double` (AVR's `double` is a 32-bit float; the mantissa is 24 bits, and beyond ~4096 averaging iterations further accumulation stops helping). The 91% plateau is a ReLU dead-unit artifact, not a runtime issue — see the demo header and the [Arduino & AVR](page:arduino) page.

The important numbers are the ones that stay flat: `arena_used` is 0 B at every reporting step across 30,000 iterations, and `drift` is 0 B after the probe region. Model state, PRNG, and serial output all fit in the C stack.

## Benchmarking your own code

```shell
vynec bench
vynec bench src/main.vy --warmup 10 --iterations 100
```

`vcore.now_ns()` is the monotonic clock used by the harness. `vmem.peak_allocated()` and `vcore.memory_usage` are the memory counters. For region-level checks, compare `vmem.total_allocated()` before and after the region.

## See also

- [CLI Reference](page:cli)
- [CUDA & Devices](page:device)
- [SRCNN Showcase](page:srcnn)

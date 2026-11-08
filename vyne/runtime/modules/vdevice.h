// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tuncay Gafarli
//
// This file is part of the Vyne runtime library, distributed under the
// MIT License. See LICENSE-MIT at the repository root for the full text.

#pragma once
// ============================================================================
// @device runtime — a device arena with a CUDA driver backend and CPU fallback.
// ----------------------------------------------------------------------------
// The `@device<backend>` region opens a device frame. Device allocations are
// made with device_alloc(n), staged with device_upload/download, and freed at
// the region's closing brace by the device arena rewind.
//
// The CUDA backend is loaded dynamically from nvcuda.dll (Windows) or
// libcuda.so.1 (Linux) and executes PTX compiled from
// vyne/runtime/device/cuda/kernels.cu. No CUDA link flags or headers are
// required in the generated C. If the driver or a device is unavailable, the
// backend falls back to CPU and reports the fallback.
//
// The CPU backend uses ordinary host allocations and scalar loops. It exists
// so device-region source is portable and so CI can exercise the region
// lifecycle without a GPU.
//
// All tensors are Float64. This is a deliberate first step that matches Vyne's
// current type system; Float32/Float16 arrive with the precision-types work.
// ============================================================================

#include "../vyne_runtime.h"
#include "../detail/vyne_cuda_ptx.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
typedef HMODULE VyneCudaLib;
static inline VyneCudaLib vyne_dev_open_lib(const char* name) {
    return LoadLibraryA(name);
}
static inline void* vyne_dev_sym(VyneCudaLib lib, const char* name) {
    return (void*)GetProcAddress(lib, name);
}
#else
#include <dlfcn.h>
typedef void* VyneCudaLib;
static inline VyneCudaLib vyne_dev_open_lib(const char* name) {
    return dlopen(name, RTLD_NOW | RTLD_GLOBAL);
}
static inline void* vyne_dev_sym(VyneCudaLib lib, const char* name) {
    return dlsym(lib, name);
}
#endif

// --- minimal CUDA driver API ------------------------------------------------
typedef int CUresult;
typedef int CUdevice;
typedef unsigned long long CUdeviceptr;
typedef void* CUcontext;
typedef void* CUmodule;
typedef void* CUfunction;
typedef void* CUstream;

typedef CUresult (*VyneFn_cuInit)(unsigned int);
typedef CUresult (*VyneFn_cuDeviceGetCount)(int*);
typedef CUresult (*VyneFn_cuDeviceGet)(CUdevice*, int);
typedef CUresult (*VyneFn_cuCtxCreate)(CUcontext*, unsigned int, CUdevice);
typedef CUresult (*VyneFn_cuMemAlloc)(CUdeviceptr*, size_t);
typedef CUresult (*VyneFn_cuMemFree)(CUdeviceptr);
typedef CUresult (*VyneFn_cuMemcpyHtoD)(CUdeviceptr, const void*, size_t);
typedef CUresult (*VyneFn_cuMemcpyDtoH)(void*, CUdeviceptr, size_t);
typedef CUresult (*VyneFn_cuModuleLoadDataEx)(
    CUmodule*, const void*, unsigned int, int*, void**
);
typedef CUresult (*VyneFn_cuModuleGetFunction)(
    CUfunction*, CUmodule, const char*
);
typedef CUresult (*VyneFn_cuLaunchKernel)(
    CUfunction, unsigned int, unsigned int, unsigned int, unsigned int,
    unsigned int, unsigned int, unsigned int, CUstream, void**, void**
);
typedef CUresult (*VyneFn_cuCtxSynchronize)(void);
typedef CUresult (*VyneFn_cuGetErrorName)(CUresult, const char**);

// --- state ------------------------------------------------------------------
typedef struct {
    int backend;   // 0 = CPU, 1 = CUDA
    void* ptr;
    size_t bytes;
    int64_t n;     // elements
} VyneDeviceAlloc;

typedef struct {
    size_t mark;
    int prev_backend;
} VyneDeviceFrame;

static VyneDeviceAlloc* g_vyne_dev_allocs = NULL;
static size_t g_vyne_dev_count = 0;
static size_t g_vyne_dev_cap = 0;
static size_t g_vyne_dev_live = 0;
static size_t g_vyne_dev_peak = 0;
static int g_vyne_dev_backend = 0;
static int g_vyne_dev_depth = 0;
static VyneDeviceFrame g_vyne_dev_frames[16];

static int g_vyne_dev_ready = 0;
static int g_vyne_dev_cuda_ok = 0;
static VyneCudaLib g_vyne_cuda_lib = NULL;
static CUcontext g_vyne_cuda_ctx = NULL;
static CUmodule g_vyne_cuda_module = NULL;

static VyneFn_cuInit g_cuInit = NULL;
static VyneFn_cuDeviceGetCount g_cuDeviceGetCount = NULL;
static VyneFn_cuDeviceGet g_cuDeviceGet = NULL;
static VyneFn_cuCtxCreate g_cuCtxCreate_v2 = NULL;
static VyneFn_cuMemAlloc g_cuMemAlloc_v2 = NULL;
static VyneFn_cuMemFree g_cuMemFree_v2 = NULL;
static VyneFn_cuMemcpyHtoD g_cuMemcpyHtoD_v2 = NULL;
static VyneFn_cuMemcpyDtoH g_cuMemcpyDtoH_v2 = NULL;
static VyneFn_cuModuleLoadDataEx g_cuModuleLoadDataEx = NULL;
static VyneFn_cuModuleGetFunction g_cuModuleGetFunction = NULL;
static VyneFn_cuLaunchKernel g_cuLaunchKernel = NULL;
static VyneFn_cuCtxSynchronize g_cuCtxSynchronize = NULL;
static VyneFn_cuGetErrorName g_cuGetErrorName = NULL;

static CUfunction g_k_vec_add = NULL;
static CUfunction g_k_vec_sub = NULL;
static CUfunction g_k_vec_mul = NULL;
static CUfunction g_k_vec_scale = NULL;
static CUfunction g_k_vec_relu = NULL;
static CUfunction g_k_vec_fill = NULL;
static CUfunction g_k_matmul = NULL;

#define VYNE_CUDA_LOAD(field, name)                                      \
    do {                                                                 \
        *(void**)(&g_##field) = vyne_dev_sym(g_vyne_cuda_lib, name);     \
        if (!g_##field)                                                  \
            return 0;                                                    \
    } while (0)

static int vyne_device_cuda_init(void) {
    if (g_vyne_dev_ready)
        return g_vyne_dev_cuda_ok;
    g_vyne_dev_ready = 1;
    g_vyne_dev_cuda_ok = 0;

#if defined(_WIN32)
    g_vyne_cuda_lib = vyne_dev_open_lib("nvcuda.dll");
#else
    g_vyne_cuda_lib = vyne_dev_open_lib("libcuda.so.1");
#endif
    if (!g_vyne_cuda_lib)
        return 0;

    VYNE_CUDA_LOAD(cuInit, "cuInit");
    VYNE_CUDA_LOAD(cuDeviceGetCount, "cuDeviceGetCount");
    VYNE_CUDA_LOAD(cuDeviceGet, "cuDeviceGet");
    VYNE_CUDA_LOAD(cuCtxCreate_v2, "cuCtxCreate_v2");
    VYNE_CUDA_LOAD(cuMemAlloc_v2, "cuMemAlloc_v2");
    VYNE_CUDA_LOAD(cuMemFree_v2, "cuMemFree_v2");
    VYNE_CUDA_LOAD(cuMemcpyHtoD_v2, "cuMemcpyHtoD_v2");
    VYNE_CUDA_LOAD(cuMemcpyDtoH_v2, "cuMemcpyDtoH_v2");
    VYNE_CUDA_LOAD(cuModuleLoadDataEx, "cuModuleLoadDataEx");
    VYNE_CUDA_LOAD(cuModuleGetFunction, "cuModuleGetFunction");
    VYNE_CUDA_LOAD(cuLaunchKernel, "cuLaunchKernel");
    VYNE_CUDA_LOAD(cuCtxSynchronize, "cuCtxSynchronize");
    *(void**)(&g_cuGetErrorName) = vyne_dev_sym(g_vyne_cuda_lib, "cuGetErrorName");

    if (g_cuInit(0) != 0)
        return 0;

    int count = 0;
    if (g_cuDeviceGetCount(&count) != 0 || count <= 0)
        return 0;

    CUdevice dev;
    if (g_cuDeviceGet(&dev, 0) != 0)
        return 0;
    if (g_cuCtxCreate_v2(&g_vyne_cuda_ctx, 0, dev) != 0)
        return 0;
    if (g_cuModuleLoadDataEx(&g_vyne_cuda_module, VYNE_CUDA_PTX, 0, NULL, NULL) != 0)
        return 0;

    if (g_cuModuleGetFunction(&g_k_vec_add, g_vyne_cuda_module, "vec_add_f64") != 0)
        return 0;
    if (g_cuModuleGetFunction(&g_k_vec_sub, g_vyne_cuda_module, "vec_sub_f64") != 0)
        return 0;
    if (g_cuModuleGetFunction(&g_k_vec_mul, g_vyne_cuda_module, "vec_mul_f64") != 0)
        return 0;
    if (g_cuModuleGetFunction(&g_k_vec_scale, g_vyne_cuda_module, "vec_scale_f64") != 0)
        return 0;
    if (g_cuModuleGetFunction(&g_k_vec_relu, g_vyne_cuda_module, "vec_relu_f64") != 0)
        return 0;
    if (g_cuModuleGetFunction(&g_k_vec_fill, g_vyne_cuda_module, "vec_fill_f64") != 0)
        return 0;
    if (g_cuModuleGetFunction(&g_k_matmul, g_vyne_cuda_module, "matmul_f64") != 0)
        return 0;

    g_vyne_dev_cuda_ok = 1;
    return 1;
}

static const char* vyne_dev_error_name(CUresult r) {
    const char* name = NULL;
    if (g_cuGetErrorName)
        g_cuGetErrorName(r, &name);
    return name ? name : "unknown";
}

static void vyne_dev_check(CUresult r, const char* what) {
    if (r != 0) {
        fprintf(stderr, "vyne device: %s failed: %s (%d)\n", what,
                vyne_dev_error_name(r), (int)r);
        exit(1);
    }
}

static void vyne_dev_track(void* ptr, int backend, size_t bytes, int64_t n) {
    if (g_vyne_dev_count == g_vyne_dev_cap) {
        size_t cap = g_vyne_dev_cap ? g_vyne_dev_cap * 2 : 64;
        VyneDeviceAlloc* p = (VyneDeviceAlloc*)realloc(
            g_vyne_dev_allocs, cap * sizeof(VyneDeviceAlloc)
        );
        if (!p) {
            fprintf(stderr, "vyne device: out of memory tracking allocations\n");
            exit(1);
        }
        g_vyne_dev_allocs = p;
        g_vyne_dev_cap = cap;
    }
    VyneDeviceAlloc* a = &g_vyne_dev_allocs[g_vyne_dev_count++];
    a->backend = backend;
    a->ptr = ptr;
    a->bytes = bytes;
    a->n = n;
    g_vyne_dev_live += bytes;
    if (g_vyne_dev_live > g_vyne_dev_peak)
        g_vyne_dev_peak = g_vyne_dev_live;
}

static VyneDeviceAlloc* vyne_dev_find(int64_t handle, int64_t n,
                                      const char* what) {
    for (size_t i = 0; i < g_vyne_dev_count; ++i) {
        if ((int64_t)(uintptr_t)g_vyne_dev_allocs[i].ptr == handle) {
            if (n >= 0 && n > g_vyne_dev_allocs[i].n) {
                fprintf(
                    stderr,
                    "vyne device: %s range %lld exceeds allocation %lld\n",
                    what, (long long)n, (long long)g_vyne_dev_allocs[i].n
                );
                exit(1);
            }
            return &g_vyne_dev_allocs[i];
        }
    }
    fprintf(stderr, "vyne device: %s used an invalid device handle\n", what);
    exit(1);
}

static inline void vyne_device_enter(const char* backend) {
    if (g_vyne_dev_depth >= 16) {
        fprintf(stderr, "vyne device: nesting exceeds 16 device regions\n");
        exit(1);
    }
    int prev = g_vyne_dev_backend;
    int target = 0;
    if (backend == NULL || strcmp(backend, "auto") == 0) {
        target = vyne_device_cuda_init() ? 1 : 0;
    } else if (strcmp(backend, "cuda") == 0) {
        if (vyne_device_cuda_init())
            target = 1;
        else {
            fprintf(stderr,
                    "vyne device: CUDA unavailable; falling back to CPU\n");
            target = 0;
        }
    } else if (strcmp(backend, "cpu") == 0) {
        target = 0;
    } else {
        fprintf(stderr, "vyne device: unknown backend '%s'; using CPU\n",
                backend);
        target = 0;
    }
    g_vyne_dev_frames[g_vyne_dev_depth].mark = g_vyne_dev_count;
    g_vyne_dev_frames[g_vyne_dev_depth].prev_backend = prev;
    g_vyne_dev_depth++;
    g_vyne_dev_backend = target;
}

static inline void vyne_device_leave(void) {
    if (g_vyne_dev_depth <= 0) {
        fprintf(stderr, "vyne device: unmatched region exit\n");
        exit(1);
    }
    g_vyne_dev_depth--;
    VyneDeviceFrame* f = &g_vyne_dev_frames[g_vyne_dev_depth];
    if (g_vyne_dev_backend == 1 && g_cuCtxSynchronize)
        g_cuCtxSynchronize();
    while (g_vyne_dev_count > f->mark) {
        VyneDeviceAlloc* a = &g_vyne_dev_allocs[g_vyne_dev_count - 1];
        if (a->backend == 1) {
            if (g_cuCtxSynchronize)
                g_cuCtxSynchronize();
            vyne_dev_check(g_cuMemFree_v2((CUdeviceptr)(uintptr_t)a->ptr),
                           "cuMemFree");
        } else {
            free(a->ptr);
        }
        g_vyne_dev_live -= a->bytes;
        g_vyne_dev_count--;
    }
    g_vyne_dev_backend = f->prev_backend;
}

// --- allocation / staging ----------------------------------------------------
static inline int64_t vyne_device_alloc_f64(int64_t n) {
    if (n < 0 || (uint64_t)n > (uint64_t)SIZE_MAX / sizeof(double)) {
        fprintf(stderr, "vyne device: invalid allocation length\n");
        exit(1);
    }
    size_t bytes = (size_t)n * sizeof(double);
    if (bytes == 0)
        bytes = sizeof(double);
    void* p = NULL;
    if (g_vyne_dev_backend == 1) {
        if (!g_vyne_dev_cuda_ok && !vyne_device_cuda_init()) {
            fprintf(stderr, "vyne device: CUDA was not initialized\n");
            exit(1);
        }
        CUdeviceptr d = 0;
        vyne_dev_check(g_cuMemAlloc_v2(&d, bytes), "cuMemAlloc");
        p = (void*)(uintptr_t)d;
    } else {
        p = malloc(bytes);
        if (!p) {
            fprintf(stderr, "vyne device: host allocation failed\n");
            exit(1);
        }
    }
    vyne_dev_track(p, g_vyne_dev_backend, bytes, n);
    return (int64_t)(uintptr_t)p;
}

static inline void vyne_device_upload_f64(int64_t handle, const double* host,
                                          int64_t n) {
    (void)vyne_dev_find(handle, n, "device_upload");
    size_t bytes = (size_t)n * sizeof(double);
    if (bytes == 0)
        return;
    if (g_vyne_dev_backend == 1) {
        vyne_dev_check(
            g_cuMemcpyHtoD_v2((CUdeviceptr)(uintptr_t)handle, host, bytes),
            "cuMemcpyHtoD"
        );
    } else {
        memcpy((void*)(uintptr_t)handle, host, bytes);
    }
}

static inline void vyne_device_download_f64(int64_t handle, double* host,
                                            int64_t n) {
    (void)vyne_dev_find(handle, n, "device_download");
    size_t bytes = (size_t)n * sizeof(double);
    if (bytes == 0)
        return;
    if (g_vyne_dev_backend == 1) {
        vyne_dev_check(
            g_cuMemcpyDtoH_v2(host, (CUdeviceptr)(uintptr_t)handle, bytes),
            "cuMemcpyDtoH"
        );
    } else {
        memcpy(host, (void*)(uintptr_t)handle, bytes);
    }
}

// --- launches ---------------------------------------------------------------
static inline void vyne_dev_launch(CUfunction fn, unsigned int gx,
                                   unsigned int gy, unsigned int gz,
                                   unsigned int bx, unsigned int by,
                                   unsigned int bz, void** args) {
    vyne_dev_check(
        g_cuLaunchKernel(fn, gx, gy, gz, bx, by, bz, 0, NULL, args, NULL),
        "cuLaunchKernel"
    );
}

static inline void vyne_device_add_f64(int64_t out, int64_t a, int64_t b,
                                       int64_t n) {
    (void)vyne_dev_find(out, n, "device_add");
    (void)vyne_dev_find(a, n, "device_add");
    (void)vyne_dev_find(b, n, "device_add");
    if (g_vyne_dev_backend == 1) {
        double* op = (double*)(uintptr_t)out;
        const double* ap = (const double*)(uintptr_t)a;
        const double* bp = (const double*)(uintptr_t)b;
        long long nn = (long long)n;
        void* args[] = { &op, &ap, &bp, &nn };
        unsigned int threads = 256;
        unsigned int blocks = (unsigned int)((n + 255) / 256);
        if (blocks == 0) blocks = 1;
        vyne_dev_launch(g_k_vec_add, blocks, 1, 1, threads, 1, 1, args);
    } else {
        double* op = (double*)(uintptr_t)out;
        const double* ap = (const double*)(uintptr_t)a;
        const double* bp = (const double*)(uintptr_t)b;
        for (int64_t i = 0; i < n; ++i) op[i] = ap[i] + bp[i];
    }
}

static inline void vyne_device_sub_f64(int64_t out, int64_t a, int64_t b,
                                       int64_t n) {
    (void)vyne_dev_find(out, n, "device_sub");
    (void)vyne_dev_find(a, n, "device_sub");
    (void)vyne_dev_find(b, n, "device_sub");
    if (g_vyne_dev_backend == 1) {
        double* op = (double*)(uintptr_t)out;
        const double* ap = (const double*)(uintptr_t)a;
        const double* bp = (const double*)(uintptr_t)b;
        long long nn = (long long)n;
        void* args[] = { &op, &ap, &bp, &nn };
        unsigned int threads = 256;
        unsigned int blocks = (unsigned int)((n + 255) / 256);
        if (blocks == 0) blocks = 1;
        vyne_dev_launch(g_k_vec_sub, blocks, 1, 1, threads, 1, 1, args);
    } else {
        double* op = (double*)(uintptr_t)out;
        const double* ap = (const double*)(uintptr_t)a;
        const double* bp = (const double*)(uintptr_t)b;
        for (int64_t i = 0; i < n; ++i) op[i] = ap[i] - bp[i];
    }
}

static inline void vyne_device_mul_f64(int64_t out, int64_t a, int64_t b,
                                       int64_t n) {
    (void)vyne_dev_find(out, n, "device_mul");
    (void)vyne_dev_find(a, n, "device_mul");
    (void)vyne_dev_find(b, n, "device_mul");
    if (g_vyne_dev_backend == 1) {
        double* op = (double*)(uintptr_t)out;
        const double* ap = (const double*)(uintptr_t)a;
        const double* bp = (const double*)(uintptr_t)b;
        long long nn = (long long)n;
        void* args[] = { &op, &ap, &bp, &nn };
        unsigned int threads = 256;
        unsigned int blocks = (unsigned int)((n + 255) / 256);
        if (blocks == 0) blocks = 1;
        vyne_dev_launch(g_k_vec_mul, blocks, 1, 1, threads, 1, 1, args);
    } else {
        double* op = (double*)(uintptr_t)out;
        const double* ap = (const double*)(uintptr_t)a;
        const double* bp = (const double*)(uintptr_t)b;
        for (int64_t i = 0; i < n; ++i) op[i] = ap[i] * bp[i];
    }
}

static inline void vyne_device_scale_f64(int64_t out, int64_t a, double s,
                                         int64_t n) {
    (void)vyne_dev_find(out, n, "device_scale");
    (void)vyne_dev_find(a, n, "device_scale");
    if (g_vyne_dev_backend == 1) {
        double* op = (double*)(uintptr_t)out;
        const double* ap = (const double*)(uintptr_t)a;
        double ss = s;
        long long nn = (long long)n;
        void* args[] = { &op, &ap, &ss, &nn };
        unsigned int threads = 256;
        unsigned int blocks = (unsigned int)((n + 255) / 256);
        if (blocks == 0) blocks = 1;
        vyne_dev_launch(g_k_vec_scale, blocks, 1, 1, threads, 1, 1, args);
    } else {
        double* op = (double*)(uintptr_t)out;
        const double* ap = (const double*)(uintptr_t)a;
        for (int64_t i = 0; i < n; ++i) op[i] = ap[i] * s;
    }
}

static inline void vyne_device_relu_f64(int64_t out, int64_t a, int64_t n) {
    (void)vyne_dev_find(out, n, "device_relu");
    (void)vyne_dev_find(a, n, "device_relu");
    if (g_vyne_dev_backend == 1) {
        double* op = (double*)(uintptr_t)out;
        const double* ap = (const double*)(uintptr_t)a;
        long long nn = (long long)n;
        void* args[] = { &op, &ap, &nn };
        unsigned int threads = 256;
        unsigned int blocks = (unsigned int)((n + 255) / 256);
        if (blocks == 0) blocks = 1;
        vyne_dev_launch(g_k_vec_relu, blocks, 1, 1, threads, 1, 1, args);
    } else {
        double* op = (double*)(uintptr_t)out;
        const double* ap = (const double*)(uintptr_t)a;
        for (int64_t i = 0; i < n; ++i) op[i] = ap[i] > 0.0 ? ap[i] : 0.0;
    }
}

static inline void vyne_device_fill_f64(int64_t out, double v, int64_t n) {
    (void)vyne_dev_find(out, n, "device_fill");
    if (g_vyne_dev_backend == 1) {
        double* op = (double*)(uintptr_t)out;
        double vv = v;
        long long nn = (long long)n;
        void* args[] = { &op, &vv, &nn };
        unsigned int threads = 256;
        unsigned int blocks = (unsigned int)((n + 255) / 256);
        if (blocks == 0) blocks = 1;
        vyne_dev_launch(g_k_vec_fill, blocks, 1, 1, threads, 1, 1, args);
    } else {
        double* op = (double*)(uintptr_t)out;
        for (int64_t i = 0; i < n; ++i) op[i] = v;
    }
}

static inline void vyne_device_matmul_f64(int64_t out, int64_t a, int64_t b,
                                          int64_t m, int64_t n, int64_t k) {
    if (m < 0 || n < 0 || k < 0 ||
        m > 2147483647LL || n > 2147483647LL || k > 2147483647LL) {
        fprintf(stderr, "vyne device: matmul dimensions are out of range\n");
        exit(1);
    }
    (void)vyne_dev_find(out, m * n, "device_matmul(out)");
    (void)vyne_dev_find(a, m * k, "device_matmul(A)");
    (void)vyne_dev_find(b, k * n, "device_matmul(B)");
    if (m == 0 || n == 0 || k == 0)
        return;
    if (g_vyne_dev_backend == 1) {
        double* cp = (double*)(uintptr_t)out;
        const double* ap = (const double*)(uintptr_t)a;
        const double* bp = (const double*)(uintptr_t)b;
        int M = (int)m, N = (int)n, K = (int)k;
        void* args[] = { &cp, &ap, &bp, &M, &N, &K };
        unsigned int gx = (unsigned int)((N + 15) / 16);
        unsigned int gy = (unsigned int)((M + 15) / 16);
        vyne_dev_launch(g_k_matmul, gx, gy, 1, 16, 16, 1, args);
    } else {
        double* C = (double*)(uintptr_t)out;
        const double* A = (const double*)(uintptr_t)a;
        const double* B = (const double*)(uintptr_t)b;
        for (int64_t i = 0; i < m; ++i) {
            for (int64_t j = 0; j < n; ++j) {
                double s = 0.0;
                for (int64_t p = 0; p < k; ++p)
                    s += A[i * k + p] * B[p * n + j];
                C[i * n + j] = s;
            }
        }
    }
}

static inline void vyne_device_sync(void) {
    if (g_vyne_dev_backend == 1 && g_cuCtxSynchronize)
        g_cuCtxSynchronize();
}

static inline VyneValue vyne_device_backend(void) {
    return vyne_string_static(g_vyne_dev_backend == 1 ? "cuda" : "cpu");
}

static inline VyneValue vyne_device_available(void) {
    return vyne_bool(vyne_device_cuda_init() ? 1 : 0);
}

static inline VyneValue vyne_device_bytes(void) {
    return vyne_int((int64_t)g_vyne_dev_live);
}

static inline VyneValue vyne_device_peak_bytes(void) {
    return vyne_int((int64_t)g_vyne_dev_peak);
}

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
    int elem_size; // 4 = Float32 (default), 8 = Float64
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
static CUfunction g_k_conv2d = NULL;
static CUfunction g_k_conv2d_bwd_input = NULL;
static CUfunction g_k_conv2d_bwd_weight = NULL;
static CUfunction g_k_conv2d_bwd_bias = NULL;
static CUfunction g_k_relu_grad = NULL;
static CUfunction g_k_pixel_shuffle = NULL;
static CUfunction g_k_pixel_shuffle_backward = NULL;
static CUfunction g_k_mse_grad = NULL;
static CUfunction g_k_sgd_step = NULL;

static CUfunction g_k_vec_add_f32 = NULL;
static CUfunction g_k_vec_sub_f32 = NULL;
static CUfunction g_k_vec_mul_f32 = NULL;
static CUfunction g_k_vec_scale_f32 = NULL;
static CUfunction g_k_vec_relu_f32 = NULL;
static CUfunction g_k_vec_fill_f32 = NULL;
static CUfunction g_k_matmul_f32 = NULL;
static CUfunction g_k_conv2d_f32 = NULL;
static CUfunction g_k_conv2d_bwd_input_f32 = NULL;
static CUfunction g_k_conv2d_bwd_weight_f32 = NULL;
static CUfunction g_k_conv2d_bwd_bias_f32 = NULL;
static CUfunction g_k_relu_grad_f32 = NULL;
static CUfunction g_k_pixel_shuffle_f32 = NULL;
static CUfunction g_k_pixel_shuffle_backward_f32 = NULL;
static CUfunction g_k_mse_grad_f32 = NULL;
static CUfunction g_k_sgd_step_f32 = NULL;

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
    if (g_cuModuleGetFunction(&g_k_conv2d, g_vyne_cuda_module, "conv2d_f64") != 0)
        return 0;
    if (g_cuModuleGetFunction(&g_k_conv2d_bwd_input, g_vyne_cuda_module,
                              "conv2d_backward_input_f64") != 0)
        return 0;
    if (g_cuModuleGetFunction(&g_k_conv2d_bwd_weight, g_vyne_cuda_module,
                              "conv2d_backward_weight_f64") != 0)
        return 0;
    if (g_cuModuleGetFunction(&g_k_conv2d_bwd_bias, g_vyne_cuda_module,
                              "conv2d_backward_bias_f64") != 0)
        return 0;
    if (g_cuModuleGetFunction(&g_k_relu_grad, g_vyne_cuda_module,
                              "vec_relu_grad_f64") != 0)
        return 0;
    if (g_cuModuleGetFunction(&g_k_pixel_shuffle, g_vyne_cuda_module,
                              "pixel_shuffle_f64") != 0)
        return 0;
    if (g_cuModuleGetFunction(&g_k_pixel_shuffle_backward, g_vyne_cuda_module,
                              "pixel_shuffle_backward_f64") != 0)
        return 0;
    if (g_cuModuleGetFunction(&g_k_mse_grad, g_vyne_cuda_module,
                              "mse_grad_f64") != 0)
        return 0;
    if (g_cuModuleGetFunction(&g_k_sgd_step, g_vyne_cuda_module,
                              "sgd_step_f64") != 0)
        return 0;

    if (g_cuModuleGetFunction(&g_k_vec_add_f32, g_vyne_cuda_module, "vec_add_f32") != 0)
        return 0;
    if (g_cuModuleGetFunction(&g_k_vec_sub_f32, g_vyne_cuda_module, "vec_sub_f32") != 0)
        return 0;
    if (g_cuModuleGetFunction(&g_k_vec_mul_f32, g_vyne_cuda_module, "vec_mul_f32") != 0)
        return 0;
    if (g_cuModuleGetFunction(&g_k_vec_scale_f32, g_vyne_cuda_module, "vec_scale_f32") != 0)
        return 0;
    if (g_cuModuleGetFunction(&g_k_vec_relu_f32, g_vyne_cuda_module, "vec_relu_f32") != 0)
        return 0;
    if (g_cuModuleGetFunction(&g_k_vec_fill_f32, g_vyne_cuda_module, "vec_fill_f32") != 0)
        return 0;
    if (g_cuModuleGetFunction(&g_k_matmul_f32, g_vyne_cuda_module, "matmul_f32") != 0)
        return 0;
    if (g_cuModuleGetFunction(&g_k_conv2d_f32, g_vyne_cuda_module, "conv2d_f32") != 0)
        return 0;
    if (g_cuModuleGetFunction(&g_k_conv2d_bwd_input_f32, g_vyne_cuda_module,
                              "conv2d_backward_input_f32") != 0)
        return 0;
    if (g_cuModuleGetFunction(&g_k_conv2d_bwd_weight_f32, g_vyne_cuda_module,
                              "conv2d_backward_weight_f32") != 0)
        return 0;
    if (g_cuModuleGetFunction(&g_k_conv2d_bwd_bias_f32, g_vyne_cuda_module,
                              "conv2d_backward_bias_f32") != 0)
        return 0;
    if (g_cuModuleGetFunction(&g_k_relu_grad_f32, g_vyne_cuda_module,
                              "vec_relu_grad_f32") != 0)
        return 0;
    if (g_cuModuleGetFunction(&g_k_pixel_shuffle_f32, g_vyne_cuda_module,
                              "pixel_shuffle_f32") != 0)
        return 0;
    if (g_cuModuleGetFunction(&g_k_pixel_shuffle_backward_f32, g_vyne_cuda_module,
                              "pixel_shuffle_backward_f32") != 0)
        return 0;
    if (g_cuModuleGetFunction(&g_k_mse_grad_f32, g_vyne_cuda_module,
                              "mse_grad_f32") != 0)
        return 0;
    if (g_cuModuleGetFunction(&g_k_sgd_step_f32, g_vyne_cuda_module,
                              "sgd_step_f32") != 0)
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

static void vyne_dev_track(void* ptr, int backend, size_t bytes, int64_t n,
                           int elem_size) {
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
    a->elem_size = elem_size;
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
static inline int64_t vyne_dev_alloc(int64_t n, int elem_size) {
    const size_t es = (size_t)elem_size;
    if (n < 0 || es == 0 || (uint64_t)n > (uint64_t)SIZE_MAX / es) {
        fprintf(stderr, "vyne device: invalid allocation length\n");
        exit(1);
    }
    size_t bytes = (size_t)n * es;
    if (bytes == 0)
        bytes = es;
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
    vyne_dev_track(p, g_vyne_dev_backend, bytes, n, elem_size);
    return (int64_t)(uintptr_t)p;
}

// Float32 is the default device dtype.
static inline int64_t vyne_device_alloc(int64_t n) {
    return vyne_dev_alloc(n, (int)sizeof(float));
}

static inline int64_t vyne_device_alloc_f32(int64_t n) {
    return vyne_dev_alloc(n, (int)sizeof(float));
}

static inline int64_t vyne_device_alloc_f64(int64_t n) {
    return vyne_dev_alloc(n, (int)sizeof(double));
}

static inline void vyne_device_upload_f64(int64_t handle, const double* host,
                                          int64_t n) {
    VyneDeviceAlloc* a = vyne_dev_find(handle, n, "device_upload");
    if (a->elem_size != (int)sizeof(double)) {
        fprintf(stderr, "vyne device: device_upload dtype mismatch (handle is Float32)\n");
        exit(1);
    }
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
    VyneDeviceAlloc* a = vyne_dev_find(handle, n, "device_download");
    if (a->elem_size != (int)sizeof(double)) {
        fprintf(stderr, "vyne device: device_download dtype mismatch (handle is Float32)\n");
        exit(1);
    }
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

static inline void vyne_device_upload_f32(int64_t handle, const float* host,
                                          int64_t n) {
    VyneDeviceAlloc* a = vyne_dev_find(handle, n, "device_upload");
    if (a->elem_size != (int)sizeof(float)) {
        fprintf(stderr, "vyne device: device_upload dtype mismatch (handle is Float64)\n");
        exit(1);
    }
    size_t bytes = (size_t)n * sizeof(float);
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

static inline void vyne_device_download_f32(int64_t handle, float* host,
                                            int64_t n) {
    VyneDeviceAlloc* a = vyne_dev_find(handle, n, "device_download");
    if (a->elem_size != (int)sizeof(float)) {
        fprintf(stderr, "vyne device: device_download dtype mismatch (handle is Float64)\n");
        exit(1);
    }
    size_t bytes = (size_t)n * sizeof(float);
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

// --- dtype-dispatched elementwise ops (Float32 default, Float64 opt-in) ----
static inline int vyne_dev_elementwise_dtype(int64_t out, int64_t a,
                                             int64_t b, int64_t n,
                                             const char* what) {
    VyneDeviceAlloc* o = vyne_dev_find(out, n, what);
    VyneDeviceAlloc* x = vyne_dev_find(a, n, what);
    VyneDeviceAlloc* y = (b != 0) ? vyne_dev_find(b, n, what) : x;
    if (x->elem_size != o->elem_size || y->elem_size != o->elem_size) {
        fprintf(stderr, "vyne device: %s mixed element sizes\n", what);
        exit(1);
    }
    return o->elem_size;
}

static inline void vyne_device_add(int64_t out, int64_t a, int64_t b,
                                   int64_t n) {
    int es = vyne_dev_elementwise_dtype(out, a, b, n, "device_add");
    if (g_vyne_dev_backend == 1) {
        unsigned int threads = 256;
        unsigned int blocks = (unsigned int)((n + 255) / 256);
        if (blocks == 0) blocks = 1;
        long long nn = (long long)n;
        if (es == 4) {
            float* op = (float*)(uintptr_t)out;
            const float* ap = (const float*)(uintptr_t)a;
            const float* bp = (const float*)(uintptr_t)b;
            void* args[] = { &op, &ap, &bp, &nn };
            vyne_dev_launch(g_k_vec_add_f32, blocks, 1, 1, threads, 1, 1, args);
        } else {
            double* op = (double*)(uintptr_t)out;
            const double* ap = (const double*)(uintptr_t)a;
            const double* bp = (const double*)(uintptr_t)b;
            void* args[] = { &op, &ap, &bp, &nn };
            vyne_dev_launch(g_k_vec_add, blocks, 1, 1, threads, 1, 1, args);
        }
    } else if (es == 4) {
        float* op = (float*)(uintptr_t)out;
        const float* ap = (const float*)(uintptr_t)a;
        const float* bp = (const float*)(uintptr_t)b;
        for (int64_t i = 0; i < n; ++i) op[i] = ap[i] + bp[i];
    } else {
        double* op = (double*)(uintptr_t)out;
        const double* ap = (const double*)(uintptr_t)a;
        const double* bp = (const double*)(uintptr_t)b;
        for (int64_t i = 0; i < n; ++i) op[i] = ap[i] + bp[i];
    }
}

static inline void vyne_device_sub(int64_t out, int64_t a, int64_t b,
                                   int64_t n) {
    int es = vyne_dev_elementwise_dtype(out, a, b, n, "device_sub");
    if (g_vyne_dev_backend == 1) {
        unsigned int threads = 256;
        unsigned int blocks = (unsigned int)((n + 255) / 256);
        if (blocks == 0) blocks = 1;
        long long nn = (long long)n;
        if (es == 4) {
            float* op = (float*)(uintptr_t)out;
            const float* ap = (const float*)(uintptr_t)a;
            const float* bp = (const float*)(uintptr_t)b;
            void* args[] = { &op, &ap, &bp, &nn };
            vyne_dev_launch(g_k_vec_sub_f32, blocks, 1, 1, threads, 1, 1, args);
        } else {
            double* op = (double*)(uintptr_t)out;
            const double* ap = (const double*)(uintptr_t)a;
            const double* bp = (const double*)(uintptr_t)b;
            void* args[] = { &op, &ap, &bp, &nn };
            vyne_dev_launch(g_k_vec_sub, blocks, 1, 1, threads, 1, 1, args);
        }
    } else if (es == 4) {
        float* op = (float*)(uintptr_t)out;
        const float* ap = (const float*)(uintptr_t)a;
        const float* bp = (const float*)(uintptr_t)b;
        for (int64_t i = 0; i < n; ++i) op[i] = ap[i] - bp[i];
    } else {
        double* op = (double*)(uintptr_t)out;
        const double* ap = (const double*)(uintptr_t)a;
        const double* bp = (const double*)(uintptr_t)b;
        for (int64_t i = 0; i < n; ++i) op[i] = ap[i] - bp[i];
    }
}

static inline void vyne_device_mul(int64_t out, int64_t a, int64_t b,
                                   int64_t n) {
    int es = vyne_dev_elementwise_dtype(out, a, b, n, "device_mul");
    if (g_vyne_dev_backend == 1) {
        unsigned int threads = 256;
        unsigned int blocks = (unsigned int)((n + 255) / 256);
        if (blocks == 0) blocks = 1;
        long long nn = (long long)n;
        if (es == 4) {
            float* op = (float*)(uintptr_t)out;
            const float* ap = (const float*)(uintptr_t)a;
            const float* bp = (const float*)(uintptr_t)b;
            void* args[] = { &op, &ap, &bp, &nn };
            vyne_dev_launch(g_k_vec_mul_f32, blocks, 1, 1, threads, 1, 1, args);
        } else {
            double* op = (double*)(uintptr_t)out;
            const double* ap = (const double*)(uintptr_t)a;
            const double* bp = (const double*)(uintptr_t)b;
            void* args[] = { &op, &ap, &bp, &nn };
            vyne_dev_launch(g_k_vec_mul, blocks, 1, 1, threads, 1, 1, args);
        }
    } else if (es == 4) {
        float* op = (float*)(uintptr_t)out;
        const float* ap = (const float*)(uintptr_t)a;
        const float* bp = (const float*)(uintptr_t)b;
        for (int64_t i = 0; i < n; ++i) op[i] = ap[i] * bp[i];
    } else {
        double* op = (double*)(uintptr_t)out;
        const double* ap = (const double*)(uintptr_t)a;
        const double* bp = (const double*)(uintptr_t)b;
        for (int64_t i = 0; i < n; ++i) op[i] = ap[i] * bp[i];
    }
}

static inline void vyne_device_scale(int64_t out, int64_t a, double s,
                                     int64_t n) {
    int es = vyne_dev_elementwise_dtype(out, a, 0, n, "device_scale");
    if (g_vyne_dev_backend == 1) {
        unsigned int threads = 256;
        unsigned int blocks = (unsigned int)((n + 255) / 256);
        if (blocks == 0) blocks = 1;
        long long nn = (long long)n;
        if (es == 4) {
            float* op = (float*)(uintptr_t)out;
            const float* ap = (const float*)(uintptr_t)a;
            float ss = (float)s;
            void* args[] = { &op, &ap, &ss, &nn };
            vyne_dev_launch(g_k_vec_scale_f32, blocks, 1, 1, threads, 1, 1, args);
        } else {
            double* op = (double*)(uintptr_t)out;
            const double* ap = (const double*)(uintptr_t)a;
            double ss = s;
            void* args[] = { &op, &ap, &ss, &nn };
            vyne_dev_launch(g_k_vec_scale, blocks, 1, 1, threads, 1, 1, args);
        }
    } else if (es == 4) {
        float* op = (float*)(uintptr_t)out;
        const float* ap = (const float*)(uintptr_t)a;
        float ss = (float)s;
        for (int64_t i = 0; i < n; ++i) op[i] = ap[i] * ss;
    } else {
        double* op = (double*)(uintptr_t)out;
        const double* ap = (const double*)(uintptr_t)a;
        for (int64_t i = 0; i < n; ++i) op[i] = ap[i] * s;
    }
}

static inline void vyne_device_relu(int64_t out, int64_t a, int64_t n) {
    int es = vyne_dev_elementwise_dtype(out, a, 0, n, "device_relu");
    if (g_vyne_dev_backend == 1) {
        unsigned int threads = 256;
        unsigned int blocks = (unsigned int)((n + 255) / 256);
        if (blocks == 0) blocks = 1;
        long long nn = (long long)n;
        if (es == 4) {
            float* op = (float*)(uintptr_t)out;
            const float* ap = (const float*)(uintptr_t)a;
            void* args[] = { &op, &ap, &nn };
            vyne_dev_launch(g_k_vec_relu_f32, blocks, 1, 1, threads, 1, 1, args);
        } else {
            double* op = (double*)(uintptr_t)out;
            const double* ap = (const double*)(uintptr_t)a;
            void* args[] = { &op, &ap, &nn };
            vyne_dev_launch(g_k_vec_relu, blocks, 1, 1, threads, 1, 1, args);
        }
    } else if (es == 4) {
        float* op = (float*)(uintptr_t)out;
        const float* ap = (const float*)(uintptr_t)a;
        for (int64_t i = 0; i < n; ++i) op[i] = ap[i] > 0.0f ? ap[i] : 0.0f;
    } else {
        double* op = (double*)(uintptr_t)out;
        const double* ap = (const double*)(uintptr_t)a;
        for (int64_t i = 0; i < n; ++i) op[i] = ap[i] > 0.0 ? ap[i] : 0.0;
    }
}

static inline void vyne_device_fill(int64_t out, double v, int64_t n) {
    int es = vyne_dev_elementwise_dtype(out, out, 0, n, "device_fill");
    if (g_vyne_dev_backend == 1) {
        unsigned int threads = 256;
        unsigned int blocks = (unsigned int)((n + 255) / 256);
        if (blocks == 0) blocks = 1;
        long long nn = (long long)n;
        if (es == 4) {
            float* op = (float*)(uintptr_t)out;
            float vv = (float)v;
            void* args[] = { &op, &vv, &nn };
            vyne_dev_launch(g_k_vec_fill_f32, blocks, 1, 1, threads, 1, 1, args);
        } else {
            double* op = (double*)(uintptr_t)out;
            double vv = v;
            void* args[] = { &op, &vv, &nn };
            vyne_dev_launch(g_k_vec_fill, blocks, 1, 1, threads, 1, 1, args);
        }
    } else if (es == 4) {
        float* op = (float*)(uintptr_t)out;
        float vv = (float)v;
        for (int64_t i = 0; i < n; ++i) op[i] = vv;
    } else {
        double* op = (double*)(uintptr_t)out;
        for (int64_t i = 0; i < n; ++i) op[i] = v;
    }
}

static inline void vyne_device_mse_grad(int64_t grad, int64_t partial,
                                        int64_t pred, int64_t target,
                                        int64_t n) {
    int es = vyne_dev_elementwise_dtype(grad, pred, target, n, "device_mse_grad");
    VyneDeviceAlloc* P = vyne_dev_find(partial, n, "device_mse_grad(partial)");
    if (P->elem_size != es) {
        fprintf(stderr, "vyne device: device_mse_grad mixed element sizes\n");
        exit(1);
    }
    if (g_vyne_dev_backend == 1) {
        unsigned int threads = 256;
        unsigned int blocks = (unsigned int)((n + 255) / 256);
        if (blocks == 0) blocks = 1;
        long long nn = (long long)n;
        if (es == 4) {
            float* gp = (float*)(uintptr_t)grad;
            float* pp = (float*)(uintptr_t)partial;
            const float* yp = (const float*)(uintptr_t)pred;
            const float* tp = (const float*)(uintptr_t)target;
            void* args[] = { &gp, &pp, &yp, &tp, &nn };
            vyne_dev_launch(g_k_mse_grad_f32, blocks, 1, 1, threads, 1, 1, args);
        } else {
            double* gp = (double*)(uintptr_t)grad;
            double* pp = (double*)(uintptr_t)partial;
            const double* yp = (const double*)(uintptr_t)pred;
            const double* tp = (const double*)(uintptr_t)target;
            void* args[] = { &gp, &pp, &yp, &tp, &nn };
            vyne_dev_launch(g_k_mse_grad, blocks, 1, 1, threads, 1, 1, args);
        }
    } else if (es == 4) {
        float* g = (float*)(uintptr_t)grad;
        float* p = (float*)(uintptr_t)partial;
        const float* y = (const float*)(uintptr_t)pred;
        const float* t = (const float*)(uintptr_t)target;
        float inv = n > 0 ? 1.0f / (float)n : 0.0f;
        for (int64_t i = 0; i < n; ++i) {
            float d = y[i] - t[i];
            g[i] = 2.0f * d * inv;
            p[i] = d * d * inv;
        }
    } else {
        double* g = (double*)(uintptr_t)grad;
        double* p = (double*)(uintptr_t)partial;
        const double* y = (const double*)(uintptr_t)pred;
        const double* t = (const double*)(uintptr_t)target;
        double inv = n > 0 ? 1.0 / (double)n : 0.0;
        for (int64_t i = 0; i < n; ++i) {
            double d = y[i] - t[i];
            g[i] = 2.0 * d * inv;
            p[i] = d * d * inv;
        }
    }
}

static inline void vyne_device_sgd_step(int64_t param, int64_t grad,
                                        double lr, int64_t n) {
    int es = vyne_dev_elementwise_dtype(param, param, grad, n, "device_sgd_step");
    if (g_vyne_dev_backend == 1) {
        unsigned int threads = 256;
        unsigned int blocks = (unsigned int)((n + 255) / 256);
        if (blocks == 0) blocks = 1;
        long long nn = (long long)n;
        if (es == 4) {
            float* pp = (float*)(uintptr_t)param;
            const float* gp = (const float*)(uintptr_t)grad;
            float llr = (float)lr;
            void* args[] = { &pp, &gp, &llr, &nn };
            vyne_dev_launch(g_k_sgd_step_f32, blocks, 1, 1, threads, 1, 1, args);
        } else {
            double* pp = (double*)(uintptr_t)param;
            const double* gp = (const double*)(uintptr_t)grad;
            double llr = lr;
            void* args[] = { &pp, &gp, &llr, &nn };
            vyne_dev_launch(g_k_sgd_step, blocks, 1, 1, threads, 1, 1, args);
        }
    } else if (es == 4) {
        float* p = (float*)(uintptr_t)param;
        const float* g = (const float*)(uintptr_t)grad;
        float llr = (float)lr;
        for (int64_t i = 0; i < n; ++i) p[i] -= llr * g[i];
    } else {
        double* p = (double*)(uintptr_t)param;
        const double* g = (const double*)(uintptr_t)grad;
        for (int64_t i = 0; i < n; ++i) p[i] -= lr * g[i];
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

// Generic matmul: Float32 handles use the F32 kernel, Float64 handles use
// the F64 kernel. Handles must all share the same dtype.
static inline void vyne_device_matmul(int64_t out, int64_t a, int64_t b,
                                      int64_t m, int64_t n, int64_t k) {
    if (m < 0 || n < 0 || k < 0 ||
        m > 2147483647LL || n > 2147483647LL || k > 2147483647LL) {
        fprintf(stderr, "vyne device: matmul dimensions are out of range\n");
        exit(1);
    }
    VyneDeviceAlloc* o = vyne_dev_find(out, m * n, "device_matmul(out)");
    VyneDeviceAlloc* A = vyne_dev_find(a, m * k, "device_matmul(A)");
    VyneDeviceAlloc* B = vyne_dev_find(b, k * n, "device_matmul(B)");
    if (o->elem_size != A->elem_size || o->elem_size != B->elem_size) {
        fprintf(stderr, "vyne device: device_matmul mixed element sizes\n");
        exit(1);
    }
    if (m == 0 || n == 0 || k == 0)
        return;
    int es = o->elem_size;
    if (g_vyne_dev_backend == 1) {
        int M = (int)m, N = (int)n, K = (int)k;
        unsigned int gx = (unsigned int)((N + 15) / 16);
        unsigned int gy = (unsigned int)((M + 15) / 16);
        if (es == 4) {
            float* cp = (float*)(uintptr_t)out;
            const float* ap = (const float*)(uintptr_t)a;
            const float* bp = (const float*)(uintptr_t)b;
            void* args[] = { &cp, &ap, &bp, &M, &N, &K };
            vyne_dev_launch(g_k_matmul_f32, gx, gy, 1, 16, 16, 1, args);
        } else {
            double* cp = (double*)(uintptr_t)out;
            const double* ap = (const double*)(uintptr_t)a;
            const double* bp = (const double*)(uintptr_t)b;
            void* args[] = { &cp, &ap, &bp, &M, &N, &K };
            vyne_dev_launch(g_k_matmul, gx, gy, 1, 16, 16, 1, args);
        }
    } else if (es == 4) {
        float* C = (float*)(uintptr_t)out;
        const float* A = (const float*)(uintptr_t)a;
        const float* B = (const float*)(uintptr_t)b;
        for (int64_t i = 0; i < m; ++i) {
            for (int64_t j = 0; j < n; ++j) {
                float s = 0.0f;
                for (int64_t p = 0; p < k; ++p)
                    s += A[i * k + p] * B[p * n + j];
                C[i * n + j] = s;
            }
        }
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

// Generic conv2d: Float32 handles use the F32 kernel, Float64 handles the
// F64 kernel. Square kernels, zero padding, HWC tensors, OIHW weights.
static inline void vyne_device_conv2d(
    int64_t out, int64_t x, int64_t w, int64_t b,
    int64_t in_h, int64_t in_w, int64_t in_c,
    int64_t out_h, int64_t out_w, int64_t out_c,
    int64_t k, int64_t stride, int64_t pad, int64_t has_bias) {
    if (in_h <= 0 || in_w <= 0 || in_c <= 0 ||
        out_h <= 0 || out_w <= 0 || out_c <= 0 || k <= 0) {
        fprintf(stderr, "vyne device: conv2d dimensions are invalid\n");
        exit(1);
    }
    if (stride < 1) stride = 1;
    if (pad < 0) pad = 0;

    VyneDeviceAlloc* o = vyne_dev_find(out, out_h * out_w * out_c, "device_conv2d(out)");
    VyneDeviceAlloc* X = vyne_dev_find(x, in_h * in_w * in_c, "device_conv2d(input)");
    VyneDeviceAlloc* W = vyne_dev_find(w, out_c * in_c * k * k, "device_conv2d(weight)");
    VyneDeviceAlloc* B = NULL;
    if (has_bias)
        B = vyne_dev_find(b, out_c, "device_conv2d(bias)");
    if (o->elem_size != X->elem_size || o->elem_size != W->elem_size ||
        (B && o->elem_size != B->elem_size)) {
        fprintf(stderr, "vyne device: device_conv2d mixed element sizes\n");
        exit(1);
    }

    int total = (int)(out_h * out_w * out_c);
    int es = o->elem_size;
    if (g_vyne_dev_backend == 1) {
        int IH = (int)in_h, IW = (int)in_w, IC = (int)in_c;
        int OH = (int)out_h, OW = (int)out_w, OC = (int)out_c;
        int K = (int)k, S = (int)stride, P = (int)pad, HB = (int)has_bias;
        unsigned int threads = 256;
        unsigned int blocks = (unsigned int)((total + 255) / 256);
        if (blocks == 0) blocks = 1;
        if (es == 4) {
            float* cp = (float*)(uintptr_t)out;
            const float* xp = (const float*)(uintptr_t)x;
            const float* wp = (const float*)(uintptr_t)w;
            const float* bp = has_bias ? (const float*)(uintptr_t)b : NULL;
            void* args[] = { &cp, &xp, &wp, &bp, &IH, &IW, &IC,
                             &OH, &OW, &OC, &K, &S, &P, &HB };
            vyne_dev_launch(g_k_conv2d_f32, blocks, 1, 1, threads, 1, 1, args);
        } else {
            double* cp = (double*)(uintptr_t)out;
            const double* xp = (const double*)(uintptr_t)x;
            const double* wp = (const double*)(uintptr_t)w;
            const double* bp = has_bias ? (const double*)(uintptr_t)b : NULL;
            void* args[] = { &cp, &xp, &wp, &bp, &IH, &IW, &IC,
                             &OH, &OW, &OC, &K, &S, &P, &HB };
            vyne_dev_launch(g_k_conv2d, blocks, 1, 1, threads, 1, 1, args);
        }
    } else if (es == 4) {
        float* C = (float*)(uintptr_t)out;
        const float* Xp = (const float*)(uintptr_t)x;
        const float* Wp = (const float*)(uintptr_t)w;
        const float* Bp = has_bias ? (const float*)(uintptr_t)b : NULL;
        for (int64_t oy = 0; oy < out_h; ++oy)
            for (int64_t ox = 0; ox < out_w; ++ox)
                for (int64_t oc = 0; oc < out_c; ++oc) {
                    float acc = has_bias ? Bp[oc] : 0.0f;
                    int64_t wb = ((oc * in_c) * k) * k;
                    for (int64_t ic = 0; ic < in_c; ++ic)
                        for (int64_t ky = 0; ky < k; ++ky) {
                            int64_t iy = oy * stride - pad + ky;
                            if (iy < 0 || iy >= in_h) continue;
                            for (int64_t kx = 0; kx < k; ++kx) {
                                int64_t ix = ox * stride - pad + kx;
                                if (ix < 0 || ix >= in_w) continue;
                                acc += Xp[(iy * in_w + ix) * in_c + ic] *
                                       Wp[wb + ((ic * k) + ky) * k + kx];
                            }
                        }
                    C[(oy * out_w + ox) * out_c + oc] = acc;
                }
    } else {
        double* C = (double*)(uintptr_t)out;
        const double* Xp = (const double*)(uintptr_t)x;
        const double* Wp = (const double*)(uintptr_t)w;
        const double* Bp = has_bias ? (const double*)(uintptr_t)b : NULL;
        for (int64_t oy = 0; oy < out_h; ++oy)
            for (int64_t ox = 0; ox < out_w; ++ox)
                for (int64_t oc = 0; oc < out_c; ++oc) {
                    double acc = has_bias ? Bp[oc] : 0.0;
                    int64_t wb = ((oc * in_c) * k) * k;
                    for (int64_t ic = 0; ic < in_c; ++ic)
                        for (int64_t ky = 0; ky < k; ++ky) {
                            int64_t iy = oy * stride - pad + ky;
                            if (iy < 0 || iy >= in_h) continue;
                            for (int64_t kx = 0; kx < k; ++kx) {
                                int64_t ix = ox * stride - pad + kx;
                                if (ix < 0 || ix >= in_w) continue;
                                acc += Xp[(iy * in_w + ix) * in_c + ic] *
                                       Wp[wb + ((ic * k) + ky) * k + kx];
                            }
                        }
                    C[(oy * out_w + ox) * out_c + oc] = acc;
                }
    }
}
// Generic conv2d backward-input: grad_X[in_h,in_w,in_c] from
// grad_out[out_h,out_w,out_c] and the OIHW weight. Zero padding, square
// kernel, HWC tensors. The F32/F64 kernel is selected by the handle dtype.
static inline void vyne_device_conv2d_backward_input(
    int64_t grad_x, int64_t grad_out, int64_t w,
    int64_t in_h, int64_t in_w, int64_t in_c,
    int64_t out_h, int64_t out_w, int64_t out_c,
    int64_t k, int64_t stride, int64_t pad) {
    if (in_h <= 0 || in_w <= 0 || in_c <= 0 ||
        out_h <= 0 || out_w <= 0 || out_c <= 0 || k <= 0) {
        fprintf(stderr, "vyne device: conv2d backward-input dimensions are invalid\n");
        exit(1);
    }
    if (stride < 1) stride = 1;
    if (pad < 0) pad = 0;

    VyneDeviceAlloc* GX = vyne_dev_find(grad_x, in_h * in_w * in_c,
                                        "device_conv2d_backward_input(grad_x)");
    VyneDeviceAlloc* GO = vyne_dev_find(grad_out, out_h * out_w * out_c,
                                        "device_conv2d_backward_input(grad_out)");
    VyneDeviceAlloc* W = vyne_dev_find(w, out_c * in_c * k * k,
                                       "device_conv2d_backward_input(weight)");
    if (GX->elem_size != GO->elem_size || GX->elem_size != W->elem_size) {
        fprintf(stderr, "vyne device: device_conv2d_backward_input mixed element sizes\n");
        exit(1);
    }

    int total = (int)(in_h * in_w * in_c);
    int es = GX->elem_size;
    if (g_vyne_dev_backend == 1) {
        int IH = (int)in_h, IW = (int)in_w, IC = (int)in_c;
        int OH = (int)out_h, OW = (int)out_w, OC = (int)out_c;
        int K = (int)k, S = (int)stride, P = (int)pad;
        unsigned int threads = 256;
        unsigned int blocks = (unsigned int)((total + 255) / 256);
        if (blocks == 0) blocks = 1;
        if (es == 4) {
            float* gxp = (float*)(uintptr_t)grad_x;
            const float* gop = (const float*)(uintptr_t)grad_out;
            const float* wp = (const float*)(uintptr_t)w;
            void* args[] = { &gxp, &gop, &wp, &IH, &IW, &IC,
                             &OH, &OW, &OC, &K, &S, &P };
            vyne_dev_launch(g_k_conv2d_bwd_input_f32, blocks, 1, 1, threads, 1, 1, args);
        } else {
            double* gxp = (double*)(uintptr_t)grad_x;
            const double* gop = (const double*)(uintptr_t)grad_out;
            const double* wp = (const double*)(uintptr_t)w;
            void* args[] = { &gxp, &gop, &wp, &IH, &IW, &IC,
                             &OH, &OW, &OC, &K, &S, &P };
            vyne_dev_launch(g_k_conv2d_bwd_input, blocks, 1, 1, threads, 1, 1, args);
        }
    } else if (es == 4) {
        float* gx = (float*)(uintptr_t)grad_x;
        const float* go = (const float*)(uintptr_t)grad_out;
        const float* wp = (const float*)(uintptr_t)w;
        for (int64_t iy = 0; iy < in_h; ++iy)
            for (int64_t ix = 0; ix < in_w; ++ix)
                for (int64_t ic = 0; ic < in_c; ++ic) {
                    float acc = 0.0f;
                    for (int64_t oc = 0; oc < out_c; ++oc)
                        for (int64_t ky = 0; ky < k; ++ky) {
                            int64_t oy_num = iy + pad - ky;
                            if (oy_num < 0 || (oy_num % stride) != 0) continue;
                            int64_t oy = oy_num / stride;
                            if (oy >= out_h) continue;
                            for (int64_t kx = 0; kx < k; ++kx) {
                                int64_t ox_num = ix + pad - kx;
                                if (ox_num < 0 || (ox_num % stride) != 0) continue;
                                int64_t ox = ox_num / stride;
                                if (ox >= out_w) continue;
                                acc += go[(oy * out_w + ox) * out_c + oc] *
                                       wp[((oc * in_c) + ic) * k * k + ky * k + kx];
                            }
                        }
                    gx[(iy * in_w + ix) * in_c + ic] = acc;
                }
    } else {
        double* gx = (double*)(uintptr_t)grad_x;
        const double* go = (const double*)(uintptr_t)grad_out;
        const double* wp = (const double*)(uintptr_t)w;
        for (int64_t iy = 0; iy < in_h; ++iy)
            for (int64_t ix = 0; ix < in_w; ++ix)
                for (int64_t ic = 0; ic < in_c; ++ic) {
                    double acc = 0.0;
                    for (int64_t oc = 0; oc < out_c; ++oc)
                        for (int64_t ky = 0; ky < k; ++ky) {
                            int64_t oy_num = iy + pad - ky;
                            if (oy_num < 0 || (oy_num % stride) != 0) continue;
                            int64_t oy = oy_num / stride;
                            if (oy >= out_h) continue;
                            for (int64_t kx = 0; kx < k; ++kx) {
                                int64_t ox_num = ix + pad - kx;
                                if (ox_num < 0 || (ox_num % stride) != 0) continue;
                                int64_t ox = ox_num / stride;
                                if (ox >= out_w) continue;
                                acc += go[(oy * out_w + ox) * out_c + oc] *
                                       wp[((oc * in_c) + ic) * k * k + ky * k + kx];
                            }
                        }
                    gx[(iy * in_w + ix) * in_c + ic] = acc;
                }
    }
}

// Generic conv2d backward-weight: grad_W[out_c,in_c,k,k] from the HWC input
// and grad_out. Zero padding, square kernel.
static inline void vyne_device_conv2d_backward_weight(
    int64_t grad_w, int64_t x, int64_t grad_out,
    int64_t in_h, int64_t in_w, int64_t in_c,
    int64_t out_h, int64_t out_w, int64_t out_c,
    int64_t k, int64_t stride, int64_t pad) {
    if (in_h <= 0 || in_w <= 0 || in_c <= 0 ||
        out_h <= 0 || out_w <= 0 || out_c <= 0 || k <= 0) {
        fprintf(stderr, "vyne device: conv2d backward-weight dimensions are invalid\n");
        exit(1);
    }
    if (stride < 1) stride = 1;
    if (pad < 0) pad = 0;

    VyneDeviceAlloc* GW = vyne_dev_find(grad_w, out_c * in_c * k * k,
                                        "device_conv2d_backward_weight(grad_w)");
    VyneDeviceAlloc* X = vyne_dev_find(x, in_h * in_w * in_c,
                                       "device_conv2d_backward_weight(input)");
    VyneDeviceAlloc* GO = vyne_dev_find(grad_out, out_h * out_w * out_c,
                                        "device_conv2d_backward_weight(grad_out)");
    if (GW->elem_size != X->elem_size || GW->elem_size != GO->elem_size) {
        fprintf(stderr, "vyne device: device_conv2d_backward_weight mixed element sizes\n");
        exit(1);
    }

    int total = (int)(out_c * in_c * k * k);
    int es = GW->elem_size;
    if (g_vyne_dev_backend == 1) {
        int IH = (int)in_h, IW = (int)in_w, IC = (int)in_c;
        int OH = (int)out_h, OW = (int)out_w, OC = (int)out_c;
        int K = (int)k, S = (int)stride, P = (int)pad;
        unsigned int threads = 256;
        unsigned int blocks = (unsigned int)((total + 255) / 256);
        if (blocks == 0) blocks = 1;
        if (es == 4) {
            float* gwp = (float*)(uintptr_t)grad_w;
            const float* xp = (const float*)(uintptr_t)x;
            const float* gop = (const float*)(uintptr_t)grad_out;
            void* args[] = { &gwp, &xp, &gop, &IH, &IW, &IC,
                             &OH, &OW, &OC, &K, &S, &P };
            vyne_dev_launch(g_k_conv2d_bwd_weight_f32, blocks, 1, 1, threads, 1, 1, args);
        } else {
            double* gwp = (double*)(uintptr_t)grad_w;
            const double* xp = (const double*)(uintptr_t)x;
            const double* gop = (const double*)(uintptr_t)grad_out;
            void* args[] = { &gwp, &xp, &gop, &IH, &IW, &IC,
                             &OH, &OW, &OC, &K, &S, &P };
            vyne_dev_launch(g_k_conv2d_bwd_weight, blocks, 1, 1, threads, 1, 1, args);
        }
    } else if (es == 4) {
        float* gw = (float*)(uintptr_t)grad_w;
        const float* xp = (const float*)(uintptr_t)x;
        const float* go = (const float*)(uintptr_t)grad_out;
        for (int64_t oc = 0; oc < out_c; ++oc)
            for (int64_t ic = 0; ic < in_c; ++ic)
                for (int64_t ky = 0; ky < k; ++ky)
                    for (int64_t kx = 0; kx < k; ++kx) {
                        float acc = 0.0f;
                        for (int64_t oy = 0; oy < out_h; ++oy) {
                            int64_t iy = oy * stride - pad + ky;
                            if (iy < 0 || iy >= in_h) continue;
                            for (int64_t ox = 0; ox < out_w; ++ox) {
                                int64_t ix = ox * stride - pad + kx;
                                if (ix < 0 || ix >= in_w) continue;
                                acc += go[(oy * out_w + ox) * out_c + oc] *
                                       xp[(iy * in_w + ix) * in_c + ic];
                            }
                        }
                        gw[((oc * in_c) + ic) * k * k + ky * k + kx] = acc;
                    }
    } else {
        double* gw = (double*)(uintptr_t)grad_w;
        const double* xp = (const double*)(uintptr_t)x;
        const double* go = (const double*)(uintptr_t)grad_out;
        for (int64_t oc = 0; oc < out_c; ++oc)
            for (int64_t ic = 0; ic < in_c; ++ic)
                for (int64_t ky = 0; ky < k; ++ky)
                    for (int64_t kx = 0; kx < k; ++kx) {
                        double acc = 0.0;
                        for (int64_t oy = 0; oy < out_h; ++oy) {
                            int64_t iy = oy * stride - pad + ky;
                            if (iy < 0 || iy >= in_h) continue;
                            for (int64_t ox = 0; ox < out_w; ++ox) {
                                int64_t ix = ox * stride - pad + kx;
                                if (ix < 0 || ix >= in_w) continue;
                                acc += go[(oy * out_w + ox) * out_c + oc] *
                                       xp[(iy * in_w + ix) * in_c + ic];
                            }
                        }
                        gw[((oc * in_c) + ic) * k * k + ky * k + kx] = acc;
                    }
    }
}

// Generic conv2d backward-bias: grad_B[out_c] = sum over spatial of grad_out.
static inline void vyne_device_conv2d_backward_bias(
    int64_t grad_b, int64_t grad_out,
    int64_t out_h, int64_t out_w, int64_t out_c) {
    if (out_h <= 0 || out_w <= 0 || out_c <= 0) {
        fprintf(stderr, "vyne device: conv2d backward-bias dimensions are invalid\n");
        exit(1);
    }
    VyneDeviceAlloc* GB = vyne_dev_find(grad_b, out_c,
                                        "device_conv2d_backward_bias(grad_b)");
    VyneDeviceAlloc* GO = vyne_dev_find(grad_out, out_h * out_w * out_c,
                                        "device_conv2d_backward_bias(grad_out)");
    if (GB->elem_size != GO->elem_size) {
        fprintf(stderr, "vyne device: device_conv2d_backward_bias mixed element sizes\n");
        exit(1);
    }

    int es = GB->elem_size;
    if (g_vyne_dev_backend == 1) {
        int OH = (int)out_h, OW = (int)out_w, OC = (int)out_c;
        unsigned int threads = 256;
        unsigned int blocks = (unsigned int)((out_c + 255) / 256);
        if (blocks == 0) blocks = 1;
        if (es == 4) {
            float* gbp = (float*)(uintptr_t)grad_b;
            const float* gop = (const float*)(uintptr_t)grad_out;
            void* args[] = { &gbp, &gop, &OH, &OW, &OC };
            vyne_dev_launch(g_k_conv2d_bwd_bias_f32, blocks, 1, 1, threads, 1, 1, args);
        } else {
            double* gbp = (double*)(uintptr_t)grad_b;
            const double* gop = (const double*)(uintptr_t)grad_out;
            void* args[] = { &gbp, &gop, &OH, &OW, &OC };
            vyne_dev_launch(g_k_conv2d_bwd_bias, blocks, 1, 1, threads, 1, 1, args);
        }
    } else if (es == 4) {
        float* gb = (float*)(uintptr_t)grad_b;
        const float* go = (const float*)(uintptr_t)grad_out;
        for (int64_t oc = 0; oc < out_c; ++oc) {
            float acc = 0.0f;
            for (int64_t i = 0; i < out_h * out_w; ++i)
                acc += go[i * out_c + oc];
            gb[oc] = acc;
        }
    } else {
        double* gb = (double*)(uintptr_t)grad_b;
        const double* go = (const double*)(uintptr_t)grad_out;
        for (int64_t oc = 0; oc < out_c; ++oc) {
            double acc = 0.0;
            for (int64_t i = 0; i < out_h * out_w; ++i)
                acc += go[i * out_c + oc];
            gb[oc] = acc;
        }
    }
}

// ReLU backward on device: out = pre > 0 ? grad : 0, element-wise.
static inline void vyne_device_relu_grad(int64_t out, int64_t grad,
                                         int64_t pre, int64_t n) {
    int es = vyne_dev_elementwise_dtype(out, grad, pre, n, "device_relu_grad");
    if (g_vyne_dev_backend == 1) {
        unsigned int threads = 256;
        unsigned int blocks = (unsigned int)((n + 255) / 256);
        if (blocks == 0) blocks = 1;
        long long nn = (long long)n;
        if (es == 4) {
            float* op = (float*)(uintptr_t)out;
            const float* gp = (const float*)(uintptr_t)grad;
            const float* pp = (const float*)(uintptr_t)pre;
            void* args[] = { &op, &gp, &pp, &nn };
            vyne_dev_launch(g_k_relu_grad_f32, blocks, 1, 1, threads, 1, 1, args);
        } else {
            double* op = (double*)(uintptr_t)out;
            const double* gp = (const double*)(uintptr_t)grad;
            const double* pp = (const double*)(uintptr_t)pre;
            void* args[] = { &op, &gp, &pp, &nn };
            vyne_dev_launch(g_k_relu_grad, blocks, 1, 1, threads, 1, 1, args);
        }
    } else if (es == 4) {
        float* op = (float*)(uintptr_t)out;
        const float* gp = (const float*)(uintptr_t)grad;
        const float* pp = (const float*)(uintptr_t)pre;
        for (int64_t i = 0; i < n; ++i) op[i] = pp[i] > 0.0f ? gp[i] : 0.0f;
    } else {
        double* op = (double*)(uintptr_t)out;
        const double* gp = (const double*)(uintptr_t)grad;
        const double* pp = (const double*)(uintptr_t)pre;
        for (int64_t i = 0; i < n; ++i) op[i] = pp[i] > 0.0 ? gp[i] : 0.0;
    }
}

// ESPCN depth-to-space on device. HWC, in channels c must be divisible by r*r.
static inline void vyne_device_pixel_shuffle(
    int64_t out, int64_t in, int64_t h, int64_t w, int64_t c, int64_t r) {
    if (h <= 0 || w <= 0 || c <= 0 || r <= 0) {
        fprintf(stderr, "vyne device: pixel_shuffle dimensions are invalid\n");
        exit(1);
    }
    int64_t rr = r * r;
    if (c % rr != 0) {
        fprintf(stderr, "vyne device: pixel_shuffle channels not divisible by r*r\n");
        exit(1);
    }
    int64_t out_c = c / rr;
    int64_t out_h = h * r;
    int64_t out_w = w * r;

    VyneDeviceAlloc* O = vyne_dev_find(out, out_h * out_w * out_c,
                                       "device_pixel_shuffle(out)");
    VyneDeviceAlloc* I = vyne_dev_find(in, h * w * c,
                                       "device_pixel_shuffle(input)");
    if (O->elem_size != I->elem_size) {
        fprintf(stderr, "vyne device: device_pixel_shuffle mixed element sizes\n");
        exit(1);
    }

    int total = (int)(out_h * out_w * out_c);
    int es = O->elem_size;
    if (g_vyne_dev_backend == 1) {
        int H = (int)h, W = (int)w, C = (int)c, R = (int)r;
        unsigned int threads = 256;
        unsigned int blocks = (unsigned int)((total + 255) / 256);
        if (blocks == 0) blocks = 1;
        if (es == 4) {
            float* op = (float*)(uintptr_t)out;
            const float* ip = (const float*)(uintptr_t)in;
            void* args[] = { &op, &ip, &H, &W, &C, &R };
            vyne_dev_launch(g_k_pixel_shuffle_f32, blocks, 1, 1, threads, 1, 1, args);
        } else {
            double* op = (double*)(uintptr_t)out;
            const double* ip = (const double*)(uintptr_t)in;
            void* args[] = { &op, &ip, &H, &W, &C, &R };
            vyne_dev_launch(g_k_pixel_shuffle, blocks, 1, 1, threads, 1, 1, args);
        }
    } else if (es == 4) {
        float* op = (float*)(uintptr_t)out;
        const float* ip = (const float*)(uintptr_t)in;
        for (int64_t y = 0; y < h; ++y)
            for (int64_t x = 0; x < w; ++x)
                for (int64_t i = 0; i < r; ++i)
                    for (int64_t j = 0; j < r; ++j)
                        for (int64_t oc = 0; oc < out_c; ++oc)
                            op[((y * r + i) * out_w + (x * r + j)) * out_c + oc] =
                                ip[((y * w + x) * c) + oc * rr + i * r + j];
    } else {
        double* op = (double*)(uintptr_t)out;
        const double* ip = (const double*)(uintptr_t)in;
        for (int64_t y = 0; y < h; ++y)
            for (int64_t x = 0; x < w; ++x)
                for (int64_t i = 0; i < r; ++i)
                    for (int64_t j = 0; j < r; ++j)
                        for (int64_t oc = 0; oc < out_c; ++oc)
                            op[((y * r + i) * out_w + (x * r + j)) * out_c + oc] =
                                ip[((y * w + x) * c) + oc * rr + i * r + j];
    }
}

static inline void vyne_device_pixel_shuffle_backward(
    int64_t grad_in, int64_t grad_out, int64_t h, int64_t w, int64_t c, int64_t r) {
    if (h <= 0 || w <= 0 || c <= 0 || r <= 0) {
        fprintf(stderr, "vyne device: pixel_shuffle_backward dimensions are invalid\n");
        exit(1);
    }
    int64_t rr = r * r;
    if (c % rr != 0) {
        fprintf(stderr, "vyne device: pixel_shuffle_backward channels not divisible by r*r\n");
        exit(1);
    }
    int64_t out_c = c / rr;
    int64_t out_h = h * r;
    int64_t out_w = w * r;

    VyneDeviceAlloc* GI = vyne_dev_find(grad_in, h * w * c,
                                        "device_pixel_shuffle_backward(grad_in)");
    VyneDeviceAlloc* GO = vyne_dev_find(grad_out, out_h * out_w * out_c,
                                        "device_pixel_shuffle_backward(grad_out)");
    if (GI->elem_size != GO->elem_size) {
        fprintf(stderr, "vyne device: device_pixel_shuffle_backward mixed element sizes\n");
        exit(1);
    }

    int total = (int)(h * w * c);
    int es = GI->elem_size;
    if (g_vyne_dev_backend == 1) {
        int H = (int)h, W = (int)w, C = (int)c, R = (int)r;
        unsigned int threads = 256;
        unsigned int blocks = (unsigned int)((total + 255) / 256);
        if (blocks == 0) blocks = 1;
        if (es == 4) {
            float* gip = (float*)(uintptr_t)grad_in;
            const float* gop = (const float*)(uintptr_t)grad_out;
            void* args[] = { &gip, &gop, &H, &W, &C, &R };
            vyne_dev_launch(g_k_pixel_shuffle_backward_f32, blocks, 1, 1, threads, 1, 1, args);
        } else {
            double* gip = (double*)(uintptr_t)grad_in;
            const double* gop = (const double*)(uintptr_t)grad_out;
            void* args[] = { &gip, &gop, &H, &W, &C, &R };
            vyne_dev_launch(g_k_pixel_shuffle_backward, blocks, 1, 1, threads, 1, 1, args);
        }
    } else if (es == 4) {
        float* gi = (float*)(uintptr_t)grad_in;
        const float* go = (const float*)(uintptr_t)grad_out;
        for (int64_t y = 0; y < h; ++y)
            for (int64_t x = 0; x < w; ++x)
                for (int64_t sc = 0; sc < c; ++sc) {
                    int64_t oc = sc / rr;
                    int64_t rem = sc % rr;
                    int64_t i = rem / r;
                    int64_t j = rem % r;
                    gi[(y * w + x) * c + sc] =
                        go[((y * r + i) * out_w + (x * r + j)) * out_c + oc];
                }
    } else {
        double* gi = (double*)(uintptr_t)grad_in;
        const double* go = (const double*)(uintptr_t)grad_out;
        for (int64_t y = 0; y < h; ++y)
            for (int64_t x = 0; x < w; ++x)
                for (int64_t sc = 0; sc < c; ++sc) {
                    int64_t oc = sc / rr;
                    int64_t rem = sc % rr;
                    int64_t i = rem / r;
                    int64_t j = rem % r;
                    gi[(y * w + x) * c + sc] =
                        go[((y * r + i) * out_w + (x * r + j)) * out_c + oc];
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

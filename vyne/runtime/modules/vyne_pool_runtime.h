// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tuncay Gafarli
//
// This file is part of the Vyne runtime library, distributed under the
// MIT License. See LICENSE-MIT at the repository root for the full text.

#pragma once
// ============================================================================
// Pool runtime for `@pool<T, N>` regions.
// ----------------------------------------------------------------------------
// A pool is a fixed-capacity set of same-sized buffers, drawn from the arena
// exactly once (at region entry) and recycled through a LIFO free stack.
// Exhaustion is a hard runtime error, not a silent grow — the whole point of
// a pool is that its peak is a compile-time fact.
//
// LIFO recycling matches the KV-cache shape: alloc k, alloc v, use, free v,
// free k, alloc k, ... hits the same two slots forever.
//
// The pool's arena chunk dies with the enclosing region's rewind; there is
// no explicit destroy step. See codegen/regions.cpp:compilePoolRegion for
// the ordering guarantee (checkpoint before pool create).
// ============================================================================

#include "../vyne_runtime.h"   // same base as vmem.h: types, arrays, arena
#include <stddef.h>

// --- Float64 ---------------------------------------------------------------
typedef struct {
    double*  backing;
    int32_t* free_stack;
    int32_t  free_top;
    int64_t  elems_per_slot;
    int64_t  capacity;
} VynePool_f64;

static inline VynePool_f64 vyne_pool_f64_create(int64_t elems_per_slot,
                                                int64_t capacity) {
    VynePool_f64 p;
    p.elems_per_slot = elems_per_slot;
    p.capacity       = capacity;
    p.backing    = (double*) arena_alloc(sizeof(double)  * (size_t)(elems_per_slot * capacity));
    p.free_stack = (int32_t*)arena_alloc(sizeof(int32_t) * (size_t)capacity);
    for (int64_t i = 0; i < capacity; ++i) p.free_stack[i] = (int32_t)i;
    p.free_top = (int32_t)capacity - 1;
    return p;
}

static inline VyneArray_f64 vyne_pool_f64_alloc(VynePool_f64* p) {
    if (p->free_top < 0) {
        fprintf(stderr,
            "Runtime error (VNE-082): @pool region exhausted "
            "(capacity %lld buffers of %lld elements); "
            "raise the pool size or free slots inside the region\n",
            (long long)p->capacity, (long long)p->elems_per_slot);
        exit(1);
    }
    int32_t idx = p->free_stack[p->free_top--];
    VyneArray_f64 a;
    a.data = p->backing + (int64_t)idx * p->elems_per_slot;
    a.size = p->elems_per_slot;
    a.cap  = p->elems_per_slot;
    return a;
}

static inline void vyne_pool_f64_free(VynePool_f64* p, VyneArray_f64* a) {
    if (!a || !a->data) return;
    ptrdiff_t off = a->data - p->backing;
    int32_t   idx = (int32_t)(off / p->elems_per_slot);
    p->free_stack[++p->free_top] = idx;
    a->data = NULL; a->size = 0; a->cap = 0;
}

// --- Int64 -----------------------------------------------------------------
typedef struct {
    int64_t* backing;
    int32_t* free_stack;
    int32_t  free_top;
    int64_t  elems_per_slot;
    int64_t  capacity;
} VynePool_i64;

static inline VynePool_i64 vyne_pool_i64_create(int64_t elems_per_slot,
                                                int64_t capacity) {
    VynePool_i64 p;
    p.elems_per_slot = elems_per_slot;
    p.capacity       = capacity;
    p.backing    = (int64_t*) arena_alloc(sizeof(int64_t) * (size_t)(elems_per_slot * capacity));
    p.free_stack = (int32_t*)arena_alloc(sizeof(int32_t) * (size_t)capacity);
    for (int64_t i = 0; i < capacity; ++i) p.free_stack[i] = (int32_t)i;
    p.free_top = (int32_t)capacity - 1;
    return p;
}

static inline VyneArray_i64 vyne_pool_i64_alloc(VynePool_i64* p) {
    if (p->free_top < 0) {
        fprintf(stderr,
            "Runtime error (VNE-082): @pool region exhausted "
            "(capacity %lld buffers of %lld elements); "
            "raise the pool size or free slots inside the region\n",
            (long long)p->capacity, (long long)p->elems_per_slot);
        exit(1);
    }
    int32_t idx = p->free_stack[p->free_top--];
    VyneArray_i64 a;
    a.data = p->backing + (int64_t)idx * p->elems_per_slot;
    a.size = p->elems_per_slot;
    a.cap  = p->elems_per_slot;
    return a;
}

static inline void vyne_pool_i64_free(VynePool_i64* p, VyneArray_i64* a) {
    if (!a || !a->data) return;
    ptrdiff_t off = a->data - p->backing;
    int32_t   idx = (int32_t)(off / p->elems_per_slot);
    p->free_stack[++p->free_top] = idx;
    a->data = NULL; a->size = 0; a->cap = 0;
}

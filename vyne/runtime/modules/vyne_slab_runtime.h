// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tuncay Gafarli
//
// This file is part of the Vyne runtime library, distributed under the
// MIT License. See LICENSE-MIT at the repository root for the full text.

#pragma once
// ============================================================================
// Slab runtime for `@slab<T, MAX, CAP>` regions.
// ----------------------------------------------------------------------------
// A slab is a size-class allocator scoped to one region. Unlike `@pool`, which
// hands out buffers of a single fixed length, `slab_alloc(n)` rounds `n` up to
// the next power-of-two class (8, 16, 32, ... elements) and recycles a slot from
// that class's free list. Allocation and free are O(1); the backing store for
// each class is drawn from the arena lazily, on the class's first use, so an
// unused class costs nothing.
//
// The whole slab - every class's backing store and free list - sits above the
// region's checkpoint, so the region rewind reclaims it along with everything
// else the body allocated. There is no explicit destroy step.
//
// `slab_alloc(n)` zero-fills the requested `n` elements, matching
// `Array<Float64>` / `Array<Int64>` construction. Reusing a slot does not leak
// stale values into the caller.
//
// Slab buffers are fixed-size: allocate the final length once. Growing one
// with `push` detaches it from the slab slot, and a later `slab_free` on the
// grown buffer is rejected.
//
// See codegen/regions.cpp:compileSlabRegion for the checkpoint ordering.
// ============================================================================

#include "../vyne_runtime.h"  // same base as vmem.h: types, arrays, arena

// Number of power-of-two classes from 8 elements to 8 << 23 elements. The
// codegen front end enforces the same ceiling so an over-large `MAX` is a
// compile error, not a silent truncation.
#define VYNE_SLAB_MAX_CLASSES 24
#define VYNE_SLAB_MIN_ELEMS 8

static inline int64_t vyne_slab_pow2_ceil(int64_t n) {
    int64_t p = VYNE_SLAB_MIN_ELEMS;
    while (p < n)
        p <<= 1;
    return p;
}

static inline int vyne_slab_class_index(int64_t elems) {
    int i = 0;
    int64_t e = VYNE_SLAB_MIN_ELEMS;
    while (e < elems) {
        e <<= 1;
        ++i;
    }
    return i;
}

static inline int vyne_slab_class_count(int64_t max_elems) {
    int64_t top = vyne_slab_pow2_ceil(max_elems);
    int n = 0;
    for (int64_t e = VYNE_SLAB_MIN_ELEMS; e <= top; e <<= 1)
        ++n;
    return n;
}

// --- Float64 ---------------------------------------------------------------
typedef struct {
    double* backing;
    int32_t* free_stack;
    int32_t free_top;
    int64_t elems;     // elements per slot for this class
    int64_t capacity;  // number of slots in this class
    int initialized;
} VyneSlabClass_f64;

typedef struct {
    VyneSlabClass_f64 classes[VYNE_SLAB_MAX_CLASSES];
    int nclasses;
    int64_t max_elems;
    int64_t capacity;
} VyneSlab_f64;

static inline VyneSlab_f64 vyne_slab_f64_create(
    int64_t max_elems,
    int64_t capacity
) {
    VyneSlab_f64 s;
    memset(&s, 0, sizeof s);
    if (max_elems < VYNE_SLAB_MIN_ELEMS)
        max_elems = VYNE_SLAB_MIN_ELEMS;
    if (capacity < 1)
        capacity = 1;
    if (capacity > 2147483647LL) {
        fprintf(stderr, "Runtime error (VNE-080): @slab capacity exceeds the "
                        "Int32 free-list limit\n");
        exit(1);
    }
    s.max_elems = vyne_slab_pow2_ceil(max_elems);
    s.capacity = capacity;
    s.nclasses = vyne_slab_class_count(max_elems);
    if (s.nclasses > VYNE_SLAB_MAX_CLASSES) {
        fprintf(
            stderr,
            "Runtime error (VNE-080): @slab max elements exceeds the "
            "supported %d size classes\n",
            VYNE_SLAB_MAX_CLASSES
        );
        exit(1);
    }
    return s;
}

static inline void vyne_slab_f64_init_class(VyneSlab_f64* s, int idx) {
    VyneSlabClass_f64* c = &s->classes[idx];
    if (c->initialized)
        return;
    int64_t elems = VYNE_SLAB_MIN_ELEMS << idx;
    c->elems = elems;
    c->capacity = s->capacity;
    c->backing = (double*)arena_alloc_aligned(
        sizeof(double) * (size_t)(elems * s->capacity),
        VYNE_TYPED_ARRAY_ALIGN
    );
    c->free_stack =
        (int32_t*)arena_alloc(sizeof(int32_t) * (size_t)s->capacity);
    for (int64_t i = 0; i < s->capacity; ++i)
        c->free_stack[i] = (int32_t)i;
    c->free_top = (int32_t)(s->capacity - 1);
    c->initialized = 1;
}

static inline VyneArray_f64 vyne_slab_f64_alloc(VyneSlab_f64* s, int64_t n) {
    if (n <= 0) {
        fprintf(stderr, "Runtime error (VNE-083): slab_alloc(n) requires n > 0\n");
        exit(1);
    }
    if (n > s->max_elems) {
        fprintf(
            stderr,
            "Runtime error (VNE-082): @slab allocation of %lld elements "
            "exceeds the region's max of %lld\n",
            (long long)n,
            (long long)s->max_elems
        );
        exit(1);
    }

    int64_t elems = vyne_slab_pow2_ceil(n);
    int idx = vyne_slab_class_index(elems);
    if (idx < 0 || idx >= s->nclasses) {
        fprintf(stderr, "Runtime error (VNE-083): invalid @slab size class\n");
        exit(1);
    }
    vyne_slab_f64_init_class(s, idx);

    VyneSlabClass_f64* c = &s->classes[idx];
    if (c->free_top < 0) {
        fprintf(
            stderr,
            "Runtime error (VNE-082): @slab exhausted for the %lld-element "
            "class (capacity %lld); raise the region capacity or free slots\n",
            (long long)c->elems,
            (long long)c->capacity
        );
        exit(1);
    }

    int32_t slot = c->free_stack[c->free_top--];
    VyneArray_f64 a;
    a.data = c->backing + (int64_t)slot * c->elems;
    a.size = n;
    a.cap = c->elems;
    memset(a.data, 0, sizeof(double) * (size_t)n);
    return a;
}

static inline void vyne_slab_f64_free(VyneSlab_f64* s, VyneArray_f64* a) {
    if (!a || !a->data)
        return;
    int64_t cap = a->cap;
    if (cap < VYNE_SLAB_MIN_ELEMS || (cap & (cap - 1)) != 0) {
        fprintf(stderr, "Runtime error (VNE-083): slab_free() got a buffer that "
                        "did not come from an @slab allocation\n");
        exit(1);
    }
    int idx = vyne_slab_class_index(cap);
    if (idx < 0 || idx >= s->nclasses || !s->classes[idx].initialized) {
        fprintf(stderr, "Runtime error (VNE-083): slab_free() got a buffer from "
                        "the wrong @slab region\n");
        exit(1);
    }
    VyneSlabClass_f64* c = &s->classes[idx];
    ptrdiff_t off = a->data - c->backing;
    if (off < 0 || (off % c->elems) != 0) {
        fprintf(stderr, "Runtime error (VNE-083): slab_free() got a buffer from "
                        "the wrong @slab region\n");
        exit(1);
    }
    int64_t slot = off / c->elems;
    if (slot < 0 || slot >= c->capacity) {
        fprintf(stderr, "Runtime error (VNE-083): slab_free() slot out of range\n");
        exit(1);
    }
    if (c->free_top + 1 >= c->capacity) {
        fprintf(stderr, "Runtime error (VNE-083): double slab_free() detected\n");
        exit(1);
    }
    c->free_stack[++c->free_top] = (int32_t)slot;
    a->data = NULL;
    a->size = 0;
    a->cap = 0;
}

// --- Int64 -----------------------------------------------------------------
typedef struct {
    int64_t* backing;
    int32_t* free_stack;
    int32_t free_top;
    int64_t elems;
    int64_t capacity;
    int initialized;
} VyneSlabClass_i64;

typedef struct {
    VyneSlabClass_i64 classes[VYNE_SLAB_MAX_CLASSES];
    int nclasses;
    int64_t max_elems;
    int64_t capacity;
} VyneSlab_i64;

static inline VyneSlab_i64 vyne_slab_i64_create(
    int64_t max_elems,
    int64_t capacity
) {
    VyneSlab_i64 s;
    memset(&s, 0, sizeof s);
    if (max_elems < VYNE_SLAB_MIN_ELEMS)
        max_elems = VYNE_SLAB_MIN_ELEMS;
    if (capacity < 1)
        capacity = 1;
    if (capacity > 2147483647LL) {
        fprintf(stderr, "Runtime error (VNE-080): @slab capacity exceeds the "
                        "Int32 free-list limit\n");
        exit(1);
    }
    s.max_elems = vyne_slab_pow2_ceil(max_elems);
    s.capacity = capacity;
    s.nclasses = vyne_slab_class_count(max_elems);
    if (s.nclasses > VYNE_SLAB_MAX_CLASSES) {
        fprintf(
            stderr,
            "Runtime error (VNE-080): @slab max elements exceeds the "
            "supported %d size classes\n",
            VYNE_SLAB_MAX_CLASSES
        );
        exit(1);
    }
    return s;
}

static inline void vyne_slab_i64_init_class(VyneSlab_i64* s, int idx) {
    VyneSlabClass_i64* c = &s->classes[idx];
    if (c->initialized)
        return;
    int64_t elems = VYNE_SLAB_MIN_ELEMS << idx;
    c->elems = elems;
    c->capacity = s->capacity;
    c->backing = (int64_t*)arena_alloc_aligned(
        sizeof(int64_t) * (size_t)(elems * s->capacity),
        VYNE_TYPED_ARRAY_ALIGN
    );
    c->free_stack =
        (int32_t*)arena_alloc(sizeof(int32_t) * (size_t)s->capacity);
    for (int64_t i = 0; i < s->capacity; ++i)
        c->free_stack[i] = (int32_t)i;
    c->free_top = (int32_t)(s->capacity - 1);
    c->initialized = 1;
}

static inline VyneArray_i64 vyne_slab_i64_alloc(VyneSlab_i64* s, int64_t n) {
    if (n <= 0) {
        fprintf(stderr, "Runtime error (VNE-083): slab_alloc(n) requires n > 0\n");
        exit(1);
    }
    if (n > s->max_elems) {
        fprintf(
            stderr,
            "Runtime error (VNE-082): @slab allocation of %lld elements "
            "exceeds the region's max of %lld\n",
            (long long)n,
            (long long)s->max_elems
        );
        exit(1);
    }

    int64_t elems = vyne_slab_pow2_ceil(n);
    int idx = vyne_slab_class_index(elems);
    if (idx < 0 || idx >= s->nclasses) {
        fprintf(stderr, "Runtime error (VNE-083): invalid @slab size class\n");
        exit(1);
    }
    vyne_slab_i64_init_class(s, idx);

    VyneSlabClass_i64* c = &s->classes[idx];
    if (c->free_top < 0) {
        fprintf(
            stderr,
            "Runtime error (VNE-082): @slab exhausted for the %lld-element "
            "class (capacity %lld); raise the region capacity or free slots\n",
            (long long)c->elems,
            (long long)c->capacity
        );
        exit(1);
    }

    int32_t slot = c->free_stack[c->free_top--];
    VyneArray_i64 a;
    a.data = c->backing + (int64_t)slot * c->elems;
    a.size = n;
    a.cap = c->elems;
    memset(a.data, 0, sizeof(int64_t) * (size_t)n);
    return a;
}

static inline void vyne_slab_i64_free(VyneSlab_i64* s, VyneArray_i64* a) {
    if (!a || !a->data)
        return;
    int64_t cap = a->cap;
    if (cap < VYNE_SLAB_MIN_ELEMS || (cap & (cap - 1)) != 0) {
        fprintf(stderr, "Runtime error (VNE-083): slab_free() got a buffer that "
                        "did not come from an @slab allocation\n");
        exit(1);
    }
    int idx = vyne_slab_class_index(cap);
    if (idx < 0 || idx >= s->nclasses || !s->classes[idx].initialized) {
        fprintf(stderr, "Runtime error (VNE-083): slab_free() got a buffer from "
                        "the wrong @slab region\n");
        exit(1);
    }
    VyneSlabClass_i64* c = &s->classes[idx];
    ptrdiff_t off = a->data - c->backing;
    if (off < 0 || (off % c->elems) != 0) {
        fprintf(stderr, "Runtime error (VNE-083): slab_free() got a buffer from "
                        "the wrong @slab region\n");
        exit(1);
    }
    int64_t slot = off / c->elems;
    if (slot < 0 || slot >= c->capacity) {
        fprintf(stderr, "Runtime error (VNE-083): slab_free() slot out of range\n");
        exit(1);
    }
    if (c->free_top + 1 >= c->capacity) {
        fprintf(stderr, "Runtime error (VNE-083): double slab_free() detected\n");
        exit(1);
    }
    c->free_stack[++c->free_top] = (int32_t)slot;
    a->data = NULL;
    a->size = 0;
    a->cap = 0;
}

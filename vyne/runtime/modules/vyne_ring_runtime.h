// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tuncay Gafarli
//
// This file is part of the Vyne runtime library, distributed under the
// MIT License. See LICENSE-MIT at the repository root for the full text.

#pragma once
// ============================================================================
// Ring runtime for `@ring<T, N, MAX>` regions.
// ----------------------------------------------------------------------------
// A ring is a fixed set of N slots, each MAX elements wide, allocated once from
// the region arena. `ring_alloc(n)` hands out the next slot and advances the
// cursor; after N allocations the oldest slot is overwritten. There is no
// `ring_free`: the discipline is "no value lives longer than N allocations".
//
// This is the streaming companion to @pool and @slab. A sliding window, a
// bounded KV history, or a producer/consumer pipeline with lag <= N can be
// written as one `@ring` region enclosing the loop:
//
//     @ring<Float64, 4, 512> region stream {
//         through step :: 0..N -> loop {
//             buf = ring_alloc(512);   // rotates across iterations
//         };
//     };
//
// The ring never grows; the backing store is above the region checkpoint, so
// the rewind reclaims it. `ring_alloc(n)` zero-fills the requested n elements.
// Slots are fixed-size: do not grow a ring buffer with `push`.
//
// The compiler cannot yet prove that no value outlives N allocations; that is
// the documented programmer contract for this policy.
// ============================================================================

#include "../vyne_runtime.h"  // same base as vmem.h: types, arrays, arena

static inline int vyne_ring_size_ok(
    int64_t slots,
    int64_t elem_count,
    size_t elem_size
) {
    if (slots <= 0 || elem_count <= 0)
        return 0;
    uint64_t s = (uint64_t)slots;
    uint64_t e = (uint64_t)elem_count;
    if (s > (uint64_t)SIZE_MAX / e)
        return 0;
    uint64_t total = s * e;
    if (total > (uint64_t)SIZE_MAX / (uint64_t)elem_size)
        return 0;
    return 1;
}

// --- Float64 ---------------------------------------------------------------
typedef struct {
    double* backing;
    int64_t slots;
    int64_t slot_elems;
    int64_t next; // next slot to overwrite
} VyneRing_f64;

static inline VyneRing_f64 vyne_ring_f64_create(
    int64_t slots,
    int64_t slot_elems
) {
    VyneRing_f64 r;
    memset(&r, 0, sizeof r);
    if (!vyne_ring_size_ok(slots, slot_elems, sizeof(double))) {
        fprintf(
            stderr,
            "Runtime error (VNE-080): @ring slot count/elements invalid or "
            "too large\n"
        );
        exit(1);
    }
    r.slots = slots;
    r.slot_elems = slot_elems;
    r.next = 0;
    r.backing = (double*)arena_alloc_aligned(
        sizeof(double) * (size_t)slots * (size_t)slot_elems,
        VYNE_TYPED_ARRAY_ALIGN
    );
    return r;
}

static inline VyneArray_f64 vyne_ring_f64_alloc(
    VyneRing_f64* r,
    int64_t n
) {
    if (n <= 0 || n > r->slot_elems) {
        fprintf(
            stderr,
            "Runtime error (VNE-083): ring_alloc(n) requires "
            "0 < n <= %lld (the slot size)\n",
            (long long)r->slot_elems
        );
        exit(1);
    }
    int64_t slot = r->next;
    r->next = (r->next + 1 >= r->slots) ? 0 : r->next + 1;

    VyneArray_f64 a;
    a.data = r->backing + (size_t)slot * (size_t)r->slot_elems;
    a.size = n;
    a.cap = r->slot_elems;
    memset(a.data, 0, sizeof(double) * (size_t)n);
    return a;
}

// --- Int64 -----------------------------------------------------------------
typedef struct {
    int64_t* backing;
    int64_t slots;
    int64_t slot_elems;
    int64_t next;
} VyneRing_i64;

static inline VyneRing_i64 vyne_ring_i64_create(
    int64_t slots,
    int64_t slot_elems
) {
    VyneRing_i64 r;
    memset(&r, 0, sizeof r);
    if (!vyne_ring_size_ok(slots, slot_elems, sizeof(int64_t))) {
        fprintf(
            stderr,
            "Runtime error (VNE-080): @ring slot count/elements invalid or "
            "too large\n"
        );
        exit(1);
    }
    r.slots = slots;
    r.slot_elems = slot_elems;
    r.next = 0;
    r.backing = (int64_t*)arena_alloc_aligned(
        sizeof(int64_t) * (size_t)slots * (size_t)slot_elems,
        VYNE_TYPED_ARRAY_ALIGN
    );
    return r;
}

static inline VyneArray_i64 vyne_ring_i64_alloc(
    VyneRing_i64* r,
    int64_t n
) {
    if (n <= 0 || n > r->slot_elems) {
        fprintf(
            stderr,
            "Runtime error (VNE-083): ring_alloc(n) requires "
            "0 < n <= %lld (the slot size)\n",
            (long long)r->slot_elems
        );
        exit(1);
    }
    int64_t slot = r->next;
    r->next = (r->next + 1 >= r->slots) ? 0 : r->next + 1;

    VyneArray_i64 a;
    a.data = r->backing + (size_t)slot * (size_t)r->slot_elems;
    a.size = n;
    a.cap = r->slot_elems;
    memset(a.data, 0, sizeof(int64_t) * (size_t)n);
    return a;
}

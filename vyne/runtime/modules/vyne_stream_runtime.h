// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tuncay Gafarli
//
// This file is part of the Vyne runtime library, distributed under the
// MIT License. See LICENSE-MIT at the repository root for the full text.

#pragma once
// ============================================================================
// Stream runtime for `@streamed<T, MAX>` regions.
// ----------------------------------------------------------------------------
// A streamed region owns a separate bump arena backed by page-locked
// ("pinned") host memory where the platform supports it. The pinned buffer is
// the kind of memory a device can read without an explicit copy; when no
// device backend is linked, the same allocation still works as ordinary host
// memory and `stream_is_pinned()` reports whether the lock succeeded.
//
// `streamed_alloc(n)` draws n elements of the region's element type from the
// stream arena. The region's MAX is a byte budget (MAX * sizeof(T)); exceeding
// it is a VNE-082 runtime error. `stream_enter` saves a checkpoint and the
// budget, and `stream_leave` rewinds the stream arena, so the pinned bytes are
// released at the region's closing brace.
//
// The stream arena is independent of the main arena. `vmem.total_allocated()`
// reports the main arena; `vmem.stream_total_allocated()` reports pinned bytes.
//
// Platform providers:
//   Windows  VirtualAlloc + VirtualLock (falls back to unlocked pages if the
//            working-set quota rejects the lock)
//   Linux/   mmap + mlock (falls back to unlocked mapping if mlock fails)
//   macOS
//   other    malloc (no pinning)
//
// The compiler rejects @streamed on the AVR target; this header is desktop-only.
// ============================================================================

#include "../vyne_runtime.h"  // types, arrays, arena, VYNE_TYPED_ARRAY_ALIGN

#if defined(__linux__) || defined(__APPLE__)
#include <sys/mman.h>
#endif

#define VYNE_STREAM_MAX_DEPTH 16
#define VYNE_STREAM_BLOCK_SIZE (64 * 1024)

typedef struct VyneStreamBlock {
    uint8_t* data;
    size_t used;
    size_t capacity;
    int pinned;
    struct VyneStreamBlock* next;
} VyneStreamBlock;

typedef struct {
    VyneStreamBlock* head;
    size_t total_allocated;
    size_t peak_allocated;
} VyneStreamArena;

static VyneStreamArena g_stream_arena = {NULL, 0, 0};
static int g_stream_pinned_blocks = 0;

typedef struct {
    VyneStreamBlock* block;
    size_t offset;
    size_t total_allocated;
    size_t budget;
} VyneStreamFrame;

static VyneStreamFrame g_stream_frames[VYNE_STREAM_MAX_DEPTH];
static int g_stream_depth = 0;

// --- Pinned block provider -------------------------------------------------
static inline uint8_t* vyne_stream_block_alloc(size_t cap, int* pinned) {
    *pinned = 0;
#if defined(_WIN32)
    void* p =
        VirtualAlloc(NULL, cap, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!p)
        return NULL;
    if (VirtualLock(p, cap))
        *pinned = 1;
    return (uint8_t*)p;
#elif defined(__linux__) || defined(__APPLE__)
    void* p = mmap(
        NULL,
        cap,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0
    );
    if (p == MAP_FAILED)
        return NULL;
    if (mlock(p, cap) == 0)
        *pinned = 1;
    return (uint8_t*)p;
#else
    (void)pinned;
    return (uint8_t*)malloc(cap);
#endif
}

static inline void vyne_stream_block_free(VyneStreamBlock* b) {
    if (!b)
        return;
#if defined(_WIN32)
    if (b->pinned)
        VirtualUnlock(b->data, b->capacity);
    VirtualFree(b->data, 0, MEM_RELEASE);
#elif defined(__linux__) || defined(__APPLE__)
    if (b->pinned)
        munlock(b->data, b->capacity);
    munmap(b->data, b->capacity);
#else
    free(b->data);
#endif
    if (b->pinned && g_stream_pinned_blocks > 0)
        g_stream_pinned_blocks--;
    free(b);
}

// --- Arena -----------------------------------------------------------------
static inline void* vyne_stream_alloc(size_t size, size_t align) {
    if (g_stream_depth <= 0) {
        fprintf(
            stderr,
            "Runtime error (VNE-083): streamed_alloc() used outside a "
            "@streamed region\n"
        );
        exit(1);
    }
    size = (size + 7) & ~(size_t)7;
    if (align < 8)
        align = 8;

    VyneStreamFrame* frame = &g_stream_frames[g_stream_depth - 1];
    size_t used_before =
        g_stream_arena.total_allocated - frame->total_allocated;
    if (used_before + size > frame->budget) {
        fprintf(
            stderr,
            "Runtime error (VNE-082): @streamed budget exceeded: "
            "%zu + %zu > %zu bytes\n",
            used_before,
            size,
            frame->budget
        );
        exit(1);
    }

    uint8_t* p = NULL;
    size_t pad = 0;

    if (g_stream_arena.head) {
        VyneStreamBlock* b = g_stream_arena.head;
        uintptr_t addr = (uintptr_t)b->data + b->used;
        pad = ((addr + align - 1) & ~(uintptr_t)(align - 1)) - addr;
        if (b->used + pad + size <= b->capacity) {
            p = b->data + b->used + pad;
            b->used += pad + size;
        }
    }

    if (!p) {
        size_t cap = size + align;
        if (cap < VYNE_STREAM_BLOCK_SIZE)
            cap = VYNE_STREAM_BLOCK_SIZE;
        int pinned = 0;
        uint8_t* data = vyne_stream_block_alloc(cap, &pinned);
        if (!data) {
            fprintf(stderr, "vyne: out of memory (stream arena)\n");
            exit(1);
        }
        VyneStreamBlock* b = (VyneStreamBlock*)malloc(sizeof *b);
        if (!b) {
            fprintf(stderr, "vyne: out of memory (stream block)\n");
            exit(1);
        }
        b->data = data;
        b->capacity = cap;
        b->pinned = pinned;
        uintptr_t addr = (uintptr_t)data;
        pad = ((addr + align - 1) & ~(uintptr_t)(align - 1)) - addr;
        b->used = pad + size;
        b->next = g_stream_arena.head;
        g_stream_arena.head = b;
        if (pinned)
            g_stream_pinned_blocks++;
        p = data + pad;
    }

    if (used_before + pad + size > frame->budget) {
        fprintf(
            stderr,
            "Runtime error (VNE-082): @streamed budget exceeded after "
            "alignment padding: %zu + %zu + %zu > %zu bytes\n",
            used_before,
            pad,
            size,
            frame->budget
        );
        exit(1);
    }

    g_stream_arena.total_allocated += pad + size;
    if (g_stream_arena.total_allocated > g_stream_arena.peak_allocated)
        g_stream_arena.peak_allocated = g_stream_arena.total_allocated;
    return p;
}

static inline void vyne_stream_enter(size_t budget) {
    if (g_stream_depth >= VYNE_STREAM_MAX_DEPTH) {
        fprintf(
            stderr,
            "Runtime error (VNE-080): @streamed nesting exceeds %d\n",
            VYNE_STREAM_MAX_DEPTH
        );
        exit(1);
    }
    VyneStreamFrame* f = &g_stream_frames[g_stream_depth++];
    f->block = g_stream_arena.head;
    f->offset = g_stream_arena.head ? g_stream_arena.head->used : 0;
    f->total_allocated = g_stream_arena.total_allocated;
    f->budget = budget;
}

static inline void vyne_stream_leave(void) {
    if (g_stream_depth <= 0) {
        fprintf(
            stderr,
            "Runtime error (VNE-083): unmatched @streamed region exit\n"
        );
        exit(1);
    }
    g_stream_depth--;
    VyneStreamFrame* f = &g_stream_frames[g_stream_depth];

    while (g_stream_arena.head != f->block && g_stream_arena.head) {
        VyneStreamBlock* next = g_stream_arena.head->next;
        vyne_stream_block_free(g_stream_arena.head);
        g_stream_arena.head = next;
    }
    if (f->block)
        f->block->used = f->offset;
    g_stream_arena.total_allocated = f->total_allocated;
}

// --- Typed allocation ------------------------------------------------------
static inline VyneArray_f64 vyne_stream_f64_alloc(int64_t n) {
    if (n <= 0 || (uint64_t)n > (uint64_t)SIZE_MAX / sizeof(double)) {
        fprintf(
            stderr,
            "Runtime error (VNE-083): streamed_alloc(n) requires a positive, "
            "representable element count\n"
        );
        exit(1);
    }
    size_t bytes = sizeof(double) * (size_t)n;
    double* p = (double*)vyne_stream_alloc(bytes, VYNE_TYPED_ARRAY_ALIGN);
    VyneArray_f64 a;
    a.data = p;
    a.size = n;
    a.cap = n;
    memset(p, 0, bytes);
    return a;
}

static inline VyneArray_i64 vyne_stream_i64_alloc(int64_t n) {
    if (n <= 0 || (uint64_t)n > (uint64_t)SIZE_MAX / sizeof(int64_t)) {
        fprintf(
            stderr,
            "Runtime error (VNE-083): streamed_alloc(n) requires a positive, "
            "representable element count\n"
        );
        exit(1);
    }
    size_t bytes = sizeof(int64_t) * (size_t)n;
    int64_t* p = (int64_t*)vyne_stream_alloc(bytes, VYNE_TYPED_ARRAY_ALIGN);
    VyneArray_i64 a;
    a.data = p;
    a.size = n;
    a.cap = n;
    memset(p, 0, bytes);
    return a;
}

// --- Introspection ---------------------------------------------------------
static inline size_t vyne_stream_total_allocated(void) {
    return g_stream_arena.total_allocated;
}

static inline size_t vyne_stream_peak_allocated(void) {
    return g_stream_arena.peak_allocated;
}

static inline int vyne_stream_is_pinned(void) {
    return g_stream_pinned_blocks > 0;
}

static inline size_t vyne_stream_block_count(void) {
    size_t n = 0;
    for (VyneStreamBlock* b = g_stream_arena.head; b; b = b->next)
        n++;
    return n;
}

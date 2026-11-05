// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tuncay Gafarli
//
// This file is part of the Vyne runtime library, distributed under the
// MIT License. See LICENSE-MIT at the repository root for the full text.

#pragma once
#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_MSC_VER)
#define VYNE_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#define VYNE_NOINLINE __attribute__((noinline))
#else
#define VYNE_NOINLINE
#endif

#if defined(__GNUC__) || defined(__clang__)
#define VYNE_LIKELY(x) __builtin_expect(!!(x), 1)
#define VYNE_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define VYNE_LIKELY(x) (x)
#define VYNE_UNLIKELY(x) (x)
#endif

// ---------------------------------------------------------------------------
// Block allocation shim. On Linux we use mmap + MADV_HUGEPAGE so the
// TLB footprint for ML-sized typed arrays stays small; the kernel
// promotes pages to 2 MB when the workload justifies it. Anywhere else
// we fall back to plain malloc. The block's payload is always zero on
// return on Linux; on the malloc path we cannot make that promise.
// ---------------------------------------------------------------------------
#if defined(__linux__)
#include <sys/mman.h>
static inline uint8_t* vyne_block_alloc(size_t cap) {
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
    madvise(p, cap, MADV_HUGEPAGE);
    return (uint8_t*)p;
}
static inline void vyne_block_free(uint8_t* p, size_t cap) {
    munmap(p, cap);
}
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static inline uint8_t* vyne_block_alloc(size_t cap) {
    void* p = VirtualAlloc(NULL, cap, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    return (uint8_t*)p;
}
static inline void vyne_block_free(uint8_t* p, size_t cap) {
    (void)cap;
    VirtualFree(p, 0, MEM_RELEASE);
}
#else
static inline uint8_t* vyne_block_alloc(size_t cap) {
    return (uint8_t*)malloc(cap);
}
static inline void vyne_block_free(uint8_t* p, size_t cap) {
    (void)cap;
    free(p);
}
#endif

#define VYNE_ARENA_BLOCK_SIZE (8 * 1024 * 1024)
// Method dispatch table size. 256 was too small the moment a program
// imported more than two or three `use external` modules. Every group
// function AND every interface method registers an entry. 4096 entries
// × ~32 B = ~128 KB of .bss, which is nothing against the arena.
#define VYNE_MAX_METHODS 4096
#define VYNE_MAX_FRAME_SIZE 1024

// ============================================================================
// ARENA ALLOCATOR
// ============================================================================

typedef struct ArenaBlock {
    uint8_t* data;
    size_t used;
    size_t capacity;
    struct ArenaBlock* next;
} ArenaBlock;

typedef struct {
    ArenaBlock* head;
    size_t total_allocated;
    size_t peak_allocated;
} VyneArena;

static VyneArena g_arena = {NULL, 0, 0};

// Fast-path bump pointers. Kept in sync with g_arena.head at all times.
// g_arena_cur is the next free byte; g_arena_end is one-past-the-end.
static uint8_t* g_arena_cur = NULL;
static uint8_t* g_arena_end = NULL;

// Zero-mode counter. When > 0, every arena_alloc zero-fills the returned
// buffer. Set by @zeroed / @deterministic region entry, cleared at exit.
// Nested @zeroed regions increment/decrement, so the outer one still holds
// after the inner one closes.
static int g_arena_zero_mode = 0;

// Set true for exactly the duration of the first allocation from a
// freshly mmap'd block. Consumers that would otherwise memset their
// region can read this to skip redundant zeroing. Reset to false at the
// end of every arena_alloc and arena_rewind, so a stale true never
// escapes to a subsequent allocation.
static int g_arena_last_alloc_was_fresh_page = 0;

typedef void (*VyneArenaCleanupFn)(void);
static VyneArenaCleanupFn g_arena_cleanup_hook = NULL;

// ============================================================================
// COMMIT ARENA — survives region rewinds
// ----------------------------------------------------------------------------
// A second bump arena, parallel to g_arena. `region.commit(x)` deep-clones x
// into this arena; region rewinds do not touch it, so the committed value
// stays valid after the region closes.
//
// Lifetime: committed values live until program exit. This is a deliberate
// trade-off for the v0 implementation — A2 (escape analysis) will eventually
// let us prove when a committed value can be freed earlier. Until then, a
// commit inside a hot loop grows this arena linearly with iterations.
//
// Freed by arena_free_all() alongside the main arena.
// ============================================================================
static VyneArena g_commit_arena = {NULL, 0, 0};
static uint8_t* g_commit_cur = NULL;
static uint8_t* g_commit_end = NULL;

static inline void* arena_alloc(size_t size) {
    size = (size + 7) & ~(size_t)7;

    uint8_t* p = g_arena_cur;
    if (VYNE_UNLIKELY(p == NULL || (size_t)(g_arena_end - p) < size)) {
        size_t cap =
            size > VYNE_ARENA_BLOCK_SIZE ? size : VYNE_ARENA_BLOCK_SIZE;
        ArenaBlock* b = (ArenaBlock*)malloc(sizeof(ArenaBlock));
        if (!b) {
            fprintf(stderr, "vyne: out of memory\n");
            exit(1);
        }
        b->data = vyne_block_alloc(cap);
        if (!b->data) {
            fprintf(stderr, "vyne: out of memory\n");
            exit(1);
        }
        b->used = size;
        b->capacity = cap;
        b->next = g_arena.head;
        g_arena.head = b;

        g_arena_cur = b->data + size;
        g_arena_end = b->data + cap;
        g_arena.total_allocated += size;
        if (g_arena.total_allocated > g_arena.peak_allocated)
            g_arena.peak_allocated = g_arena.total_allocated;
        if (VYNE_UNLIKELY(g_arena_zero_mode))
            memset(b->data, 0, size);
        return b->data;
    }

    g_arena_cur = p + size;
    g_arena.total_allocated += size;
    if (g_arena.total_allocated > g_arena.peak_allocated)
        g_arena.peak_allocated = g_arena.total_allocated;
    if (VYNE_UNLIKELY(g_arena_zero_mode))
        memset(p, 0, size);
    return p;
}

// Aligned variant. Pads to the requested boundary before bumping.
// Intended for typed-array data only; small values and structs should
// still use plain arena_alloc to avoid wasting space on padding.
static inline void* arena_alloc_aligned(size_t size, size_t align) {
    size = (size + 7) & ~(size_t)7;

    uint8_t* p = g_arena_cur;
    uintptr_t misalign = (uintptr_t)p & (align - 1);
    size_t pad = misalign ? (align - misalign) : 0;

    if (VYNE_UNLIKELY(p == NULL || (size_t)(g_arena_end - p) < size + pad)) {
        // Fresh block. Force the block start to be aligned so pad == 0
        // for every allocation after the first.
        size_t cap = size + pad > VYNE_ARENA_BLOCK_SIZE ? size + pad
                                                        : VYNE_ARENA_BLOCK_SIZE;
        ArenaBlock* b = (ArenaBlock*)malloc(sizeof(ArenaBlock));
        if (!b) {
            fprintf(stderr, "vyne: out of memory\n");
            exit(1);
        }
        b->data = vyne_block_alloc(cap);
        if (!b->data) {
            fprintf(stderr, "vyne: out of memory\n");
            exit(1);
        }
        b->used = size + pad;
        b->capacity = cap;
        b->next = g_arena.head;
        g_arena.head = b;

        // mmap/VirtualAlloc return page-aligned, which is >= align for
        // every align we care about (<= 4096). malloc may not be, so
        // pad the bump pointer to the boundary.
        uintptr_t base = (uintptr_t)b->data;
        uintptr_t mis = base & (align - 1);
        size_t fpad = mis ? (align - mis) : 0;

        g_arena_cur = b->data + fpad + size;
        g_arena_end = b->data + cap;
        g_arena.total_allocated += size + fpad;
        if (g_arena.total_allocated > g_arena.peak_allocated)
            g_arena.peak_allocated = g_arena.total_allocated;
        if (VYNE_UNLIKELY(g_arena_zero_mode))
            memset(b->data + fpad, 0, size);
        return b->data + fpad;
    }

    g_arena_cur = p + pad + size;
    g_arena.total_allocated += size + pad;
    if (g_arena.total_allocated > g_arena.peak_allocated)
        g_arena.peak_allocated = g_arena.total_allocated;
    if (VYNE_UNLIKELY(g_arena_zero_mode))
        memset(p + pad, 0, size);
    return p + pad;
}

// Reclaim the most-recent allocation iff it is still the arena tail.
static inline int arena_try_reclaim(void* ptr, size_t size) {
    if (ptr == NULL)
        return 0;
    size = (size + 7) & ~(size_t)7;
    uint8_t* p = (uint8_t*)ptr;
    if (p + size != g_arena_cur)
        return 0;
    g_arena_cur = p;
    g_arena.total_allocated -= size;
    return 1;
}

static inline void arena_free_all(void) {
    // Fire the cleanup hook if anything registered one. The intern
    // table registers itself on first allocation; its canonical
    // pointers into arena blocks would dangle after this function
    // returns, so clearing it here is mandatory, not optional.
    if (g_arena_cleanup_hook)
        g_arena_cleanup_hook();

    ArenaBlock* block = g_arena.head;
    while (block) {
        ArenaBlock* next = block->next;
        vyne_block_free(block->data, block->capacity);
        free(block);
        block = next;
    }
    g_arena.head = NULL;
    g_arena_cur = NULL;
    g_arena_end = NULL;
    g_arena.total_allocated = 0;
    g_arena.peak_allocated = 0;

    // also free the commit arena if exists ( see the region part )
    ArenaBlock* cblock = g_commit_arena.head;
    while (cblock) {
        ArenaBlock* next = cblock->next;
        vyne_block_free(cblock->data, cblock->capacity);
        free(cblock);
        cblock = next;
    }
    g_commit_arena.head = NULL;
    g_commit_cur = NULL;
    g_commit_end = NULL;
    g_commit_arena.total_allocated = 0;
    g_commit_arena.peak_allocated = 0;
}

// ============================================================================
// ARENA CHECKPOINT / REWIND
// ----------------------------------------------------------------------------
// Save a position in the arena, then rewind to it later. Rewind frees every
// block allocated since the checkpoint and resets the bump pointer inside
// the checkpoint's block. The checkpoint itself lives on the C stack.
//
// Constraints:
//   - Any pointer whose target was allocated after the checkpoint is
//     invalidated by rewind. Do not rewind while such pointers are live.
//   - Later checkpoints are invalidated when an earlier one is used.
// ============================================================================

typedef struct {
    ArenaBlock* block;      // the block we were bumping in at checkpoint time
    size_t offset;          // bytes used within that block
    size_t total_allocated; // for accounting
} ArenaCheckpoint;

static inline ArenaCheckpoint arena_checkpoint(void) {
    ArenaCheckpoint cp;
    cp.block = g_arena.head;
    cp.offset = (g_arena_cur != NULL && g_arena.head != NULL)
                    ? (size_t)(g_arena_cur - g_arena.head->data)
                    : 0;
    cp.total_allocated = g_arena.total_allocated;
    return cp;
}

static inline void arena_rewind(ArenaCheckpoint cp) {
    // Drop every block that was created after the checkpoint.
    while (g_arena.head != cp.block && g_arena.head != NULL) {
        ArenaBlock* next = g_arena.head->next;
        vyne_block_free(g_arena.head->data, g_arena.head->capacity);
        free(g_arena.head);
        g_arena.head = next;
    }
    // Reset the bump pointer inside the surviving block.
    if (cp.block != NULL) {
        g_arena_cur = cp.block->data + cp.offset;
        g_arena_end = cp.block->data + cp.block->capacity;
    } else {
        g_arena_cur = NULL;
        g_arena_end = NULL;
    }
    g_arena.total_allocated = cp.total_allocated;
}

// ============================================================================
// REGION BOUND PANIC
// ----------------------------------------------------------------------------
// Called by @bounded regions when the runtime byte counter exceeds the
// declared bound. Named for the region so the diagnostic points at the
// source construct, not a bare line number. Noreturn in practice (exit(1)).
// ============================================================================
static inline void vyne_panic_region_bound(
    const char* region_name,
    int line,
    size_t bound,
    size_t actual
) {
    fprintf(
        stderr,
        "Runtime error (VNE-102): region '%s' exceeded memory bound: "
        "%zu > %zu bytes (line %d)\n"
        "  Reduce the region's working set, raise the bound, or split the "
        "region.\n",
        region_name,
        actual,
        bound,
        line
    );
    exit(1);
}

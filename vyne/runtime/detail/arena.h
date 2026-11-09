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

// ---------------------------------------------------------------------------
// Thread-local storage. The region arena is per-thread: two OS threads
// allocating at the same time must never share a bump pointer. The commit
// arena remains process-global because it is the hand-off point between
// threads, and is protected by a spinlock (see below).
//
// MSVC and the GNU/Clang family both spell this differently; the C11
// fallback keeps freestanding C compilers working.
// ---------------------------------------------------------------------------
#if defined(__GNUC__) || defined(__clang__)
#define VYNE_THREAD_LOCAL __thread
#elif defined(_MSC_VER)
#define VYNE_THREAD_LOCAL __declspec(thread)
#else
#define VYNE_THREAD_LOCAL _Thread_local
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

// Preferred alignment for numeric buffer payloads (typed arrays and @slab
// slots). 32 bytes covers AVX2 and half an AVX-512 register. Override with
// -DVYNE_TYPED_ARRAY_ALIGN=64 for wider kernels. Small structs and boxed
// values keep the default 8-byte bump alignment.
#ifndef VYNE_TYPED_ARRAY_ALIGN
#define VYNE_TYPED_ARRAY_ALIGN 32
#endif
// Method dispatch table size. 256 was too small the moment a program
// imported more than two or three `use external` modules. Every group
// function AND every interface method registers an entry. 4096 entries
// × ~32 B = ~128 KB of .bss, which is nothing against the arena.
#define VYNE_MAX_METHODS 4096
#define VYNE_MAX_FRAME_SIZE 1024

// ============================================================================
// COMMIT-ARENA SPINLOCK
// ----------------------------------------------------------------------------
// The main arena is thread-local, so local allocation needs no locks. The
// commit arena is shared (it is how a worker publishes a value to the
// thread that will outlive it), so every read/modify/write of its bump
// pointers is serialized here.
//
// The lock is recursive for the owning thread: @persistent holds it for the
// whole region body, and commit helpers called from inside a persistent
// region must not deadlock on their own lock. `g_commit_lock_depth` is
// thread-local and counts nesting for that owner only.
// ============================================================================
#if defined(_MSC_VER)
#include <intrin.h>
typedef volatile long VyneSpinlock;
static inline void vyne_spin_lock(VyneSpinlock* l) {
    while (_InterlockedExchange(l, 1) != 0) {
        /* spin */
    }
}
static inline void vyne_spin_unlock(VyneSpinlock* l) {
    _InterlockedExchange(l, 0);
}
#else
typedef volatile int VyneSpinlock;
static inline void vyne_spin_lock(VyneSpinlock* l) {
#if defined(__GNUC__) || defined(__clang__)
    while (__sync_lock_test_and_set(l, 1)) {
        /* spin */
    }
#else
    while (*l) {
        /* spin */
    }
    *l = 1;
#endif
}
static inline void vyne_spin_unlock(VyneSpinlock* l) {
#if defined(__GNUC__) || defined(__clang__)
    __sync_lock_release(l);
#else
    *l = 0;
#endif
}
#endif

static VyneSpinlock g_commit_lock = 0;
static VYNE_THREAD_LOCAL int g_commit_lock_depth = 0;

static inline void vyne_commit_lock_acquire(void) {
    if (g_commit_lock_depth++ == 0)
        vyne_spin_lock(&g_commit_lock);
}

static inline void vyne_commit_lock_release(void) {
    if (--g_commit_lock_depth == 0)
        vyne_spin_unlock(&g_commit_lock);
}

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

// Per-thread region arena. Every OS thread that allocates gets its own
// block chain, bump pointers and byte counters. A thread publishes values
// to other threads only through the shared commit arena below.
static VYNE_THREAD_LOCAL VyneArena g_arena = {NULL, 0, 0};

// Fast-path bump pointers. Kept in sync with g_arena.head at all times.
// g_arena_cur is the next free byte; g_arena_end is one-past-the-end.
static VYNE_THREAD_LOCAL uint8_t* g_arena_cur = NULL;
static VYNE_THREAD_LOCAL uint8_t* g_arena_end = NULL;

// Zero-mode counter. When > 0, every arena_alloc zero-fills the returned
// buffer. Set by @zeroed / @deterministic region entry, cleared at exit.
// Nested @zeroed regions increment/decrement, so the outer one still holds
// after the inner one closes. Allocation context is per-thread, so this
// counter is too.
static VYNE_THREAD_LOCAL int g_arena_zero_mode = 0;

// Set true for exactly the duration of the first allocation from a
// freshly mmap'd block. Consumers that would otherwise memset their
// region can read this to skip redundant zeroing. Reset to false at the
// end of every arena_alloc and arena_rewind, so a stale true never
// escapes to a subsequent allocation.
static VYNE_THREAD_LOCAL int g_arena_last_alloc_was_fresh_page = 0;

// ---------------------------------------------------------------------------
// Optional poison-on-rewind (debug aid; C2 in the memory-model roadmap).
//
// When enabled, arena_rewind() and arena_try_reclaim() overwrite the bytes
// they release with a recognizable quiet-NaN word. A stale read then shows
// NaN in a Float64 result (or a legible sentinel in a hex dump) instead of
// silently returning a valid-looking value.
//
// Disabled by default: the checks compile to one predictable branch and no
// writes. Enable with VYNE_POISON_ARENA=1 in the environment, or call
// vyne_arena_set_poison(1) from a harness. Compile with
// -DVYNE_ARENA_POISON_DEFAULT=1 to turn it on unconditionally.
//
// This is a desktop runtime feature. The MCU arena is a separate header and
// is intentionally unchanged.
// ---------------------------------------------------------------------------
#ifndef VYNE_ARENA_POISON_DEFAULT
#define VYNE_ARENA_POISON_DEFAULT 0
#endif

#define VYNE_ARENA_POISON_WORD UINT64_C(0x7FF8DEADBEEFCAFE)
#define VYNE_ARENA_POISON_BYTE 0xA5

static int g_arena_poison_mode = -1; // -1 = uninitialized; 0 = off; 1 = on

static inline int vyne_arena_poison_enabled(void) {
    if (VYNE_UNLIKELY(g_arena_poison_mode < 0)) {
        const char* v = getenv("VYNE_POISON_ARENA");
        if (v != NULL && v[0] != '\0') {
            g_arena_poison_mode = (v[0] == '0' && v[1] == '\0') ? 0 : 1;
        } else {
            g_arena_poison_mode = VYNE_ARENA_POISON_DEFAULT ? 1 : 0;
        }
    }
    return g_arena_poison_mode;
}

static inline void vyne_arena_set_poison(int on) {
    g_arena_poison_mode = on ? 1 : 0;
}

// Fill [from, to) with the poison pattern. `from` is normally 8-byte aligned
// (arena allocations round to 8), so the whole range is written as words; a
// misaligned base or a trailing remainder falls back to a byte fill. memcpy
// is used instead of a uint64_t store to avoid strict-aliasing UB on memory
// whose effective type was a double or a struct.
static inline void arena_poison_range(uint8_t* from, uint8_t* to) {
    if (from == NULL || to <= from)
        return;
    size_t n = (size_t)(to - from);
    if (((uintptr_t)from & 7u) == 0) {
        uint64_t word = VYNE_ARENA_POISON_WORD;
        size_t words = n >> 3;
        for (size_t i = 0; i < words; ++i)
            memcpy(from + (i << 3), &word, sizeof word);
        size_t rem = n & 7u;
        if (rem != 0)
            memset(from + (words << 3), VYNE_ARENA_POISON_BYTE, rem);
    } else {
        memset(from, VYNE_ARENA_POISON_BYTE, n);
    }
}

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
        // Record the high-water mark of the block we are leaving. `used` is
        // otherwise only ever written, never read, so this is a safe place to
        // keep it for poison-on-rewind: a rewind to a checkpoint in this block
        // needs to know how far the block was filled after the checkpoint.
        if (g_arena.head != NULL && g_arena_cur != NULL)
            g_arena.head->used = (size_t)(g_arena_cur - g_arena.head->data);
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
        // Keep the high-water mark of the block we are leaving for
        // poison-on-rewind (see arena_alloc).
        if (g_arena.head != NULL && g_arena_cur != NULL)
            g_arena.head->used = (size_t)(g_arena_cur - g_arena.head->data);
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
//
// `size` is the payload size handed to arena_alloc / arena_alloc_aligned.
// The call is a no-op unless the allocation is still the tail of the
// current block, which is exactly the LIFO discipline `drop(var)` relies
// on. A successful reclaim moves the bump pointer backwards; it never
// frees the underlying block, because the block is reused by the next
// allocation (and released wholesale by a region rewind or
// arena_free_all()).
static inline int arena_try_reclaim(void* ptr, size_t size) {
    if (ptr == NULL)
        return 0;
    size = (size + 7) & ~(size_t)7;
    uint8_t* p = (uint8_t*)ptr;
    if (p + size != g_arena_cur)
        return 0;
    if (VYNE_UNLIKELY(vyne_arena_poison_enabled()))
        arena_poison_range(p, p + size);
    g_arena_cur = p;
    g_arena.total_allocated -= size;
    return 1;
}

// ============================================================================
// ZERO-COPY REGION MOVE
// ----------------------------------------------------------------------------
// Move one allocation from the region arena into the commit arena without
// copying its payload. The implementation works at the block level:
//
//   - The allocation must own the start of the *head* block and end exactly
//     at the current bump pointer. Those two conditions imply the block
//     contains no other live allocation, so the whole ArenaBlock can change
//     owners safely.
//   - The block is unlinked from g_arena, inserted into g_commit_arena's
//     block chain, and every byte counter is adjusted as if the object had
//     been allocated in the commit arena in the first place.
//   - Returning 0 means "not a dedicated tail block" — the caller must fall
//     back to a copy (the region.move high-level helpers do exactly that).
//
// The O(1) path is what a large model allocation gets: any single
// allocation larger than VYNE_ARENA_BLOCK_SIZE is served from its own
// block, so committing it no longer doubles peak RSS.
// ============================================================================
static inline int vyne_arena_move_to_commit(void* ptr, size_t size) {
    if (ptr == NULL || size == 0)
        return 0;
    size = (size + 7) & ~(size_t)7;

    ArenaBlock* b = g_arena.head;
    if (b == NULL)
        return 0;

    uint8_t* p = (uint8_t*)ptr;
    // The allocation must start at the block base (no earlier allocation in
    // this block) and end at the live bump pointer (no later allocation).
    if (p != b->data)
        return 0;
    size_t used = (g_arena_cur != NULL)
                      ? (size_t)(g_arena_cur - b->data)
                      : b->used;
    if (size != used)
        return 0;

    // Unlink from the region arena. The previous head's `used` field is the
    // high-water mark recorded when the allocator switched blocks, so it is
    // safe to resume bumping there.
    g_arena.head = b->next;
    if (g_arena.head != NULL) {
        g_arena_cur = g_arena.head->data + g_arena.head->used;
        g_arena_end = g_arena.head->data + g_arena.head->capacity;
    } else {
        g_arena_cur = NULL;
        g_arena_end = NULL;
    }
    if (g_arena.total_allocated >= size)
        g_arena.total_allocated -= size;
    else
        g_arena.total_allocated = 0;

    // Link into the commit arena. When the commit arena already has a live
    // bump block, splice the moved block immediately after it so the commit
    // bump pointers and checkpoint offsets stay valid. When it is empty, the
    // moved block becomes the head and may serve future commit allocations.
    // The commit arena is shared across threads; the region-side unlink
    // above only touched thread-local state, so the lock can be taken here.
    vyne_commit_lock_acquire();
    b->used = size;
    if (g_commit_arena.head == NULL) {
        b->next = NULL;
        g_commit_arena.head = b;
        g_commit_cur = b->data + size;
        g_commit_end = b->data + b->capacity;
    } else {
        b->next = g_commit_arena.head->next;
        g_commit_arena.head->next = b;
    }

    g_commit_arena.total_allocated += size;
    if (g_commit_arena.total_allocated > g_commit_arena.peak_allocated)
        g_commit_arena.peak_allocated = g_commit_arena.total_allocated;
    vyne_commit_lock_release();

    return 1;
}

// Release the calling thread's region arena only. Worker threads should
// call this before exiting; calling arena_free_all() from a worker would
// also free the shared commit arena out from under every other thread.
// The cleanup hook is per-process infrastructure, so it only fires from
// arena_free_all().
static inline void arena_free_thread(void) {
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
}

static inline void arena_free_all(void) {
    // Fire the cleanup hook if anything registered one. The intern
    // table registers itself on first allocation; its canonical
    // pointers into arena blocks would dangle after this function
    // returns, so clearing it here is mandatory, not optional.
    if (g_arena_cleanup_hook)
        g_arena_cleanup_hook();

    arena_free_thread();

    // The commit arena is shared. Only the process teardown path should
    // free it, and only under the lock: another thread may still be
    // publishing into it.
    vyne_commit_lock_acquire();
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
    vyne_commit_lock_release();
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
    // Poison the tail we are about to release before mutating the bump
    // pointers, so a stale read into the rewound window is observable.
    // If the checkpoint's block is still the head, the released range is
    // [cp.offset, current bump pointer). If a later block was created, this
    // block's high-water mark was recorded when the allocator switched away.
    if (VYNE_UNLIKELY(vyne_arena_poison_enabled()) && cp.block != NULL) {
        uint8_t* from = cp.block->data + cp.offset;
        uint8_t* to = (g_arena.head == cp.block && g_arena_cur != NULL)
                          ? g_arena_cur
                          : cp.block->data + cp.block->used;
        arena_poison_range(from, to);
    }
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

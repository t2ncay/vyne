// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tuncay Gafarli
//
// This file is part of the Vyne runtime library, distributed under the
// MIT License. See LICENSE-MIT at the repository root for the full text.

/* vyne/runtime/modules/vmem.h
 * -------------------------------------------------------------------
 * Vyne runtime memory module — transpiler target.
 *
 * The C runtime uses a bump-allocated arena (see vyne_runtime.h).
 * vmem exposes arena-scoped checkpointing so a hot loop can drop its
 * scratch allocations and reuse the same blocks every iteration,
 * keeping peak RSS flat regardless of iteration count.
 *
 * Naming convention: vmem_<name>
 *
 *   Methods:
 *     checkpoint()        -> Int64 handle
 *     rewind(handle)      -> null
 *     total_allocated()   -> Int64 bytes
 *     reset()             -> null    (DEV ONLY — drops the whole arena)
 * ------------------------------------------------------------------- */

#ifndef VYNE_VMEM_RT_H
#define VYNE_VMEM_RT_H

#include "../vyne_runtime.h"
#include "vyne_stream_runtime.h"

/* ===================================================================
 * CHECKPOINT STACK
 * -------------------------------------------------------------------
 * A fixed C-side stack of ArenaCheckpoint values. checkpoint() pushes
 * and returns an Int64 handle. rewind(h) unwinds the arena to slot h
 * and pops every slot from h upward — matching the stack discipline
 * of the arena itself.
 * =================================================================== */

#define VYNE_MAX_CHECKPOINTS 64

typedef struct {
    ArenaCheckpoint cp;
    int             active;
} VyneCheckpointSlot;

static VyneCheckpointSlot g_vmem_slots[VYNE_MAX_CHECKPOINTS];
static int                g_vmem_top = 0;

/* ===================================================================
 * @PERSISTENT FRAMES
 * -------------------------------------------------------------------
 * A @persistent region allocates from the commit arena and never
 * rewinds it. The outermost enter saves the enclosing arena; nested
 * enters only bump the depth. Every leave is a no-op except the
 * outermost, which publishes the advanced commit-arena bump pointer and
 * restores the enclosing arena.
 * =================================================================== */

#define VYNE_MAX_PERSISTENT_DEPTH 16

typedef struct {
    VyneArena arena;
    uint8_t*  cur;
    uint8_t*  end;
} VynePersistentFrame;

static VynePersistentFrame g_persistent_frames[VYNE_MAX_PERSISTENT_DEPTH];
static int                 g_persistent_depth = 0;

static inline void vyne_persistent_enter(void) {
    if (g_persistent_depth >= VYNE_MAX_PERSISTENT_DEPTH) {
        fprintf(stderr,
                "Runtime error (VNE-080): @persistent nesting exceeds %d\n",
                VYNE_MAX_PERSISTENT_DEPTH);
        exit(1);
    }
    if (g_persistent_depth == 0) {
        VynePersistentFrame* f = &g_persistent_frames[0];
        f->arena = g_arena;
        f->cur   = g_arena_cur;
        f->end   = g_arena_end;

        g_arena     = g_commit_arena;
        g_arena_cur = g_commit_cur;
        g_arena_end = g_commit_end;
    }
    g_persistent_depth++;
}

static inline void vyne_persistent_leave(void) {
    if (g_persistent_depth <= 0) {
        fprintf(stderr,
                "Runtime error (VNE-083): unmatched @persistent region exit\n");
        exit(1);
    }
    g_persistent_depth--;
    if (g_persistent_depth == 0) {
        /* Persistent allocations are never rewound: publish the advanced
         * commit-arena bump pointer, then restore the enclosing arena. */
        g_commit_arena = g_arena;
        g_commit_cur   = g_arena_cur;
        g_commit_end   = g_arena_end;

        VynePersistentFrame* f = &g_persistent_frames[0];
        g_arena     = f->arena;
        g_arena_cur = f->cur;
        g_arena_end = f->end;
    }
}

static inline VyneValue vmem_runtime_checkpoint(void) {
    if (g_vmem_top >= VYNE_MAX_CHECKPOINTS) {
        fprintf(stderr,
                "Runtime error: vmem.checkpoint() stack overflow (>%d)\n",
                VYNE_MAX_CHECKPOINTS);
        exit(1);
    }
    int64_t h = g_vmem_top++;
    g_vmem_slots[h].cp     = arena_checkpoint();
    g_vmem_slots[h].active = 1;
    return vyne_int(h);
}

static inline VyneValue vmem_runtime_rewind(VyneValue handle) {
    if (handle.type != V_INT64) {
        fprintf(stderr,
                "Runtime error: vmem.rewind() expects an Int64 handle\n");
        exit(1);
    }
    int64_t h = handle.as.i64;
    if (h < 0 || h >= g_vmem_top || !g_vmem_slots[h].active) {
        fprintf(stderr,
                "Runtime error: invalid or already-rewound checkpoint handle %lld\n",
                (long long)h);
        exit(1);
    }
    vyne_blas_invalidate_cache();
    arena_rewind(g_vmem_slots[h].cp);
    for (int i = (int)h; i < g_vmem_top; ++i)
        g_vmem_slots[i].active = 0;
    g_vmem_top = (int)h;
    return vyne_null();
}

/* ===================================================================
 * INSPECTION
 * =================================================================== */

static inline VyneValue vmem_runtime_total_allocated(void) {
    return vyne_int((int64_t)g_arena.total_allocated);
}

/* WARNING: drops the entire arena. Only safe at top level, when no
 * VyneValue on any C or Vyne stack refers to arena-backed storage. */
static inline VyneValue vmem_runtime_reset(void) {
    arena_free_all();      /* also resets both peaks */
    g_vmem_top = 0;
    g_persistent_depth = 0;
    return vyne_null();
}

static inline VyneValue vmem_deep_clone(VyneValue v) {
    switch (v.type) {
        case V_ARRAY:
        case V_F64_ARRAY:   // vyne_array_deepcopy handles all three
        case V_I64_ARRAY:
            return vyne_array_deepcopy(v);
        case V_MAP:   return vyne_map_deepcopy(v);
        case V_STRING: {
            if (v.as.str == NULL) return v;
            size_t len = strlen(v.as.str) + 1;
            char* buf = arena_alloc(len);
            memcpy(buf, v.as.str, len);
            return vyne_string_own(buf);
        }
        default: return v;
    }
}

static inline VyneValue vmem_runtime_commit(VyneValue v) {
    /* Inside @persistent the value already lives in the commit arena; a
     * second commit would copy it onto itself. Return it unchanged. */
    if (g_persistent_depth > 0)
        return v;

    VyneArena   saved_arena = g_arena;
    uint8_t*    saved_cur   = g_arena_cur;
    uint8_t*    saved_end   = g_arena_end;

    g_arena     = g_commit_arena;
    g_arena_cur = g_commit_cur;
    g_arena_end = g_commit_end;

    VyneValue copy = vmem_deep_clone(v);

    g_commit_arena = g_arena;
    g_commit_cur   = g_arena_cur;
    g_commit_end   = g_arena_end;

    g_arena     = saved_arena;
    g_arena_cur = saved_cur;
    g_arena_end = saved_end;

    return copy;
}

static inline void vmem_runtime_pop_checkpoints(int count) {
    while (count-- > 0 && g_vmem_top > 0) {
        g_vmem_top--;
        g_vmem_slots[g_vmem_top].active = 0;
    }
}

/* ===================================================================
 * PEAK INSPECTION
 * =================================================================== */

static inline VyneValue vmem_runtime_peak_allocated(void) {
    return vyne_int((int64_t)g_arena.peak_allocated);
}

static inline VyneValue vmem_runtime_commit_peak_allocated(void) {
    size_t peak = (g_persistent_depth > 0)
                      ? g_arena.peak_allocated
                      : g_commit_arena.peak_allocated;
    return vyne_int((int64_t)peak);
}

/* Zero the peaks without touching the arena. Useful between benchmark
 * runs: sample a warm-up, clear the peaks, then sample the real run. */
static inline VyneValue vmem_runtime_peak_reset(void) {
    g_arena.peak_allocated        = g_arena.total_allocated;
    g_commit_arena.peak_allocated = g_commit_arena.total_allocated;
    return vyne_null();
}

/* ===================================================================
 * LIVE STATE INSPECTION
 * -------------------------------------------------------------------
 * Cheap, non-allocating queries. All return Int64.
 * =================================================================== */

static inline VyneValue vmem_runtime_checkpoint_depth(void) {
    return vyne_int((int64_t)g_vmem_top);
}

static inline VyneValue vmem_runtime_checkpoint_capacity(void) {
    return vyne_int((int64_t)VYNE_MAX_CHECKPOINTS);
}

static inline VyneValue vmem_runtime_checkpoint_remaining(void) {
    return vyne_int((int64_t)(VYNE_MAX_CHECKPOINTS - g_vmem_top));
}

/* Number of 8MB (or oversized) blocks currently in the main arena.
 * Each block was malloc'd once and is reused until rewind drops it. */
static inline VyneValue vmem_runtime_block_count(void) {
    size_t n = 0;
    for (ArenaBlock* b = g_arena.head; b; b = b->next) n++;
    return vyne_int((int64_t)n);
}

/* Bytes consumed in the head block of the main arena. Reads directly
 * from the bump pointer — no walk. Returns 0 when no block exists. */
static inline VyneValue vmem_runtime_current_block_used(void) {
    if (!g_arena.head || !g_arena_cur) return vyne_int(0);
    return vyne_int((int64_t)(g_arena_cur - g_arena.head->data));
}

static inline VyneValue vmem_runtime_current_block_capacity(void) {
    if (!g_arena.head) return vyne_int(0);
    return vyne_int((int64_t)g_arena.head->capacity);
}

static inline VyneValue vmem_runtime_commit_total_allocated(void) {
    size_t total = (g_persistent_depth > 0)
                       ? g_arena.total_allocated
                       : g_commit_arena.total_allocated;
    return vyne_int((int64_t)total);
}

/* ===================================================================
 * CHECKPOINT MANAGEMENT
 * -------------------------------------------------------------------
 * `pop()` removes the innermost checkpoint without rewinding. Memory
 * allocated since the checkpoint stays alive; the handle becomes
 * invalid. Use when a scope has finished and its contents should
 * persist, but the stack slot should not.
 * =================================================================== */

static inline VyneValue vmem_runtime_pop(void) {
    if (g_vmem_top > 0) {
        g_vmem_top--;
        g_vmem_slots[g_vmem_top].active = 0;
    }
    return vyne_null();
}

/* Pop N checkpoints. Same as the internal pop_checkpoints(), exposed
 * to Vyne source for symmetry with checkpoint(). */
static inline VyneValue vmem_runtime_pop_n(VyneValue n_val) {
    if (n_val.type != V_INT64) {
        fprintf(stderr,
                "Runtime error: vmem.pop_n() expects an Int64 count\n");
        exit(1);
    }
    int64_t n = n_val.as.i64;
    if (n < 0) n = 0;
    vmem_runtime_pop_checkpoints((int)n);
    return vyne_null();
}

/* Rewind, but also report the number of bytes reclaimed. The regular
 * rewind() stays silent for hot paths; this variant is for
 * diagnostics and tests. */
static inline VyneValue vmem_runtime_rewind_freed(VyneValue handle) {
    if (handle.type != V_INT64) {
        fprintf(stderr,
                "Runtime error: vmem.rewind_freed() expects an Int64 handle\n");
        exit(1);
    }
    int64_t h = handle.as.i64;
    if (h < 0 || h >= g_vmem_top || !g_vmem_slots[h].active) {
        /* Delegate to rewind for the diagnostic and exit. */
        vmem_runtime_rewind(handle);
        return vyne_int(0);
    }
    size_t before = g_arena.total_allocated;
    size_t at_cp  = g_vmem_slots[h].cp.total_allocated;
    vmem_runtime_rewind(handle);
    return vyne_int((int64_t)(before - at_cp));
}

/* ===================================================================
 * DIAGNOSTICS
 * =================================================================== */

/* True if every checkpoint pushed has been popped or rewound. */
static inline VyneValue vmem_runtime_is_balanced(void) {
    return vyne_bool(g_vmem_top == 0);
}

/* Abort with a diagnostic if the checkpoint stack is not empty at the
 * point of call. Put this at the end of main, or after the region you
 * want to verify. Useful for catching a region whose close was skipped
 * by a control-flow path. */
static inline VyneValue vmem_runtime_assert_balanced(void) {
    if (g_vmem_top != 0) {
        fprintf(stderr,
                "Runtime error: vmem.assert_balanced() failed — "
                "%d checkpoint(s) still live\n", g_vmem_top);
        exit(1);
    }
    return vyne_null();
}

/* Validate a single handle without rewinding. Returns false for
 * out-of-range handles, already-rewound handles, or handles whose
 * stored checkpoint no longer matches a live block. */
static inline VyneValue vmem_runtime_verify_checkpoint(VyneValue handle) {
    if (handle.type != V_INT64) return vyne_bool(false);
    int64_t h = handle.as.i64;
    if (h < 0 || h >= g_vmem_top) return vyne_bool(false);
    if (!g_vmem_slots[h].active)   return vyne_bool(false);

    /* The checkpoint's stored block must still be reachable from the
     * current head by walking the `next` chain. If it isn't, a rewind
     * to this handle would corrupt the arena. */
    ArenaBlock* want = g_vmem_slots[h].cp.block;
    for (ArenaBlock* b = g_arena.head; b; b = b->next) {
        if (b == want) return vyne_bool(true);
    }
    /* A NULL checkpoint block means "the arena was empty when this
     * checkpoint was taken". That is still valid. */
    return vyne_bool(want == NULL && g_arena.head == NULL);
}

/* Print a compact state summary to stderr. Never allocates from the
 * arena — everything goes through fprintf. */
static inline VyneValue vmem_runtime_dump(void) {
    size_t blocks = 0;
    size_t cap    = 0;
    for (ArenaBlock* b = g_arena.head; b; b = b->next) {
        blocks++;
        cap += b->capacity;
    }
    size_t cur_used = 0;
    size_t cur_cap  = 0;
    if (g_arena.head && g_arena_cur) {
        cur_used = (size_t)(g_arena_cur - g_arena.head->data);
        cur_cap  = g_arena.head->capacity;
    }

    fprintf(stderr,
        "vmem dump\n"
        "  main:    live=%zu  peak=%zu  blocks=%zu  cap=%zu\n"
        "           head used=%zu / %zu\n"
        "  commit:  live=%zu  peak=%zu\n"
        "  ckpts:   depth=%d / %d\n",
        g_arena.total_allocated, g_arena.peak_allocated, blocks, cap,
        cur_used, cur_cap,
        g_commit_arena.total_allocated, g_commit_arena.peak_allocated,
        g_vmem_top, VYNE_MAX_CHECKPOINTS);
    return vyne_null();
}

/* Same data as dump(), but as a Map so Vyne code can read it.
 * The Map itself lives in the arena — calling stats() while inside a
 * region is fine, its storage is reclaimed at rewind like anything else. */
static inline VyneValue vmem_runtime_stats(void) {
    size_t blocks = 0;
    size_t cap    = 0;
    for (ArenaBlock* b = g_arena.head; b; b = b->next) {
        blocks++;
        cap += b->capacity;
    }
    size_t cur_used = 0;
    size_t cur_cap  = 0;
    if (g_arena.head && g_arena_cur) {
        cur_used = (size_t)(g_arena_cur - g_arena.head->data);
        cur_cap  = g_arena.head->capacity;
    }

    VyneValue m = vyne_map_create();
    vyne_map_set(m, vyne_string_static("live"),
                 vyne_int((int64_t)g_arena.total_allocated));
    vyne_map_set(m, vyne_string_static("peak"),
                 vyne_int((int64_t)g_arena.peak_allocated));
    vyne_map_set(m, vyne_string_static("commit_live"),
                 vyne_int((int64_t)g_commit_arena.total_allocated));
    vyne_map_set(m, vyne_string_static("commit_peak"),
                 vyne_int((int64_t)g_commit_arena.peak_allocated));
    vyne_map_set(m, vyne_string_static("blocks"),
                 vyne_int((int64_t)blocks));
    vyne_map_set(m, vyne_string_static("capacity"),
                 vyne_int((int64_t)cap));
    vyne_map_set(m, vyne_string_static("head_used"),
                 vyne_int((int64_t)cur_used));
    vyne_map_set(m, vyne_string_static("head_capacity"),
                 vyne_int((int64_t)cur_cap));
    vyne_map_set(m, vyne_string_static("checkpoint_depth"),
                 vyne_int((int64_t)g_vmem_top));
    vyne_map_set(m, vyne_string_static("checkpoint_capacity"),
                 vyne_int((int64_t)VYNE_MAX_CHECKPOINTS));
    return m;
}

/* ===================================================================
 * STREAM ARENA INSPECTION
 * -------------------------------------------------------------------
 * The @streamed pinned arena is independent of g_arena. These report
 * its live/peak bytes and whether the platform actually page-locked
 * the current blocks. All return Int64; stream_is_pinned() is 0/1.
 * =================================================================== */

static inline VyneValue vmem_runtime_stream_total_allocated(void) {
    return vyne_int((int64_t)vyne_stream_total_allocated());
}

static inline VyneValue vmem_runtime_stream_peak_allocated(void) {
    return vyne_int((int64_t)vyne_stream_peak_allocated());
}

static inline VyneValue vmem_runtime_stream_is_pinned(void) {
    return vyne_int((int64_t)vyne_stream_is_pinned());
}

static inline VyneValue vmem_runtime_stream_block_count(void) {
    return vyne_int((int64_t)vyne_stream_block_count());
}

#endif /* VYNE_VMEM_RT_H */

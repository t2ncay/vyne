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
    arena_free_all();
    g_vmem_top = 0;
    return vyne_null();
}

#endif /* VYNE_VMEM_RT_H */
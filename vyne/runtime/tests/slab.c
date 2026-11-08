// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tuncay Gafarli
//
// This file is part of the Vyne runtime library, distributed under the
// MIT License. See LICENSE-MIT at the repository root for the full text.
//
// @slab runtime: size-class rounding, zero-fill, LIFO reuse, and the
// region-rewind lifetime contract. Drives the allocator directly so it does
// not depend on codegen or the interpreter.
//
//   cc -std=c11 vyne/runtime/tests/slab.c -lm -o slab && ./slab

#include "../detail/arena.h"
#include "../modules/vyne_slab_runtime.h"
#include <assert.h>

int main(void) {
    ArenaCheckpoint cp = arena_checkpoint();

    // --- Float64: class rounding + zero-fill ------------------------------
    VyneSlab_f64 s = vyne_slab_f64_create(1024, 4);
    assert(s.nclasses == 8); // 8, 16, 32, 64, 128, 256, 512, 1024

    VyneArray_f64 a = vyne_slab_f64_alloc(&s, 100);
    assert(a.size == 100);
    assert(a.cap == 128); // next power-of-two class
    for (int i = 0; i < 100; ++i)
        assert(a.data[i] == 0.0);

    VyneArray_f64 b = vyne_slab_f64_alloc(&s, 300);
    assert(b.size == 300);
    assert(b.cap == 512);

    // --- LIFO reuse -------------------------------------------------------
    double* first_slot = a.data;
    a.data[0] = 3.5;
    vyne_slab_f64_free(&s, &a);
    assert(a.data == NULL && a.size == 0 && a.cap == 0);

    VyneArray_f64 c = vyne_slab_f64_alloc(&s, 100);
    assert(c.data == first_slot); // the just-freed slot is handed back
    assert(c.data[0] == 0.0);     // zero-filled, no stale 3.5

    // --- Int64 -----------------------------------------------------------
    VyneSlab_i64 si = vyne_slab_i64_create(256, 2);
    VyneArray_i64 d = vyne_slab_i64_alloc(&si, 10);
    assert(d.size == 10);
    assert(d.cap == 16);
    d.data[0] = 42;
    assert(d.data[0] == 42);
    vyne_slab_i64_free(&si, &d);
    assert(d.data == NULL);

    // --- Region rewind reclaims every class's backing store ---------------
    arena_rewind(cp);
    arena_free_all();
    return 0;
}

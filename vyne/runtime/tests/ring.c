// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tuncay Gafarli
//
// This file is part of the Vyne runtime library, distributed under the
// MIT License. See LICENSE-MIT at the repository root for the full text.
//
// @ring runtime: fixed slot count, rotation, zero-fill on reuse, and the
// aliasing that makes the bounded-history contract observable. Drives the
// allocator directly so it does not depend on codegen or the interpreter.
//
//   cc -std=c11 vyne/runtime/tests/ring.c -lm -o ring && ./ring

#include "../modules/vyne_ring_runtime.h"
#include <assert.h>

int main(void) {
    // --- Float64: 3 slots x 8 elements ------------------------------------
    VyneRing_f64 r = vyne_ring_f64_create(3, 8);

    VyneArray_f64 a = vyne_ring_f64_alloc(&r, 8);
    a.data[0] = 11.0;

    VyneArray_f64 b = vyne_ring_f64_alloc(&r, 5);
    b.data[0] = 22.0;
    assert(b.size == 5);
    assert(b.cap == 8); // the slot width, not the requested size

    VyneArray_f64 c = vyne_ring_f64_alloc(&r, 8);
    assert(c.size == 8);
    assert(c.data[0] == 0.0);

    // Fourth allocation wraps to slot 0 and overwrites a.
    VyneArray_f64 d = vyne_ring_f64_alloc(&r, 8);
    assert(d.data == a.data);
    assert(d.data[0] == 0.0);
    d.data[0] = 44.0;
    assert(a.data[0] == 44.0); // a aliases the rotated slot

    // --- Int64: 2 slots x 4 elements -------------------------------------
    VyneRing_i64 ri = vyne_ring_i64_create(2, 4);

    VyneArray_i64 x = vyne_ring_i64_alloc(&ri, 4);
    x.data[0] = 7;

    VyneArray_i64 y = vyne_ring_i64_alloc(&ri, 4);
    assert(y.data != x.data);
    assert(y.data[0] == 0);

    VyneArray_i64 z = vyne_ring_i64_alloc(&ri, 4); // wraps to slot 0
    assert(z.data == x.data);
    assert(z.data[0] == 0);
    z.data[0] = 9;
    assert(x.data[0] == 9);

    arena_free_all();
    return 0;
}

// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tuncay Gafarli
//
// This file is part of the Vyne runtime library, distributed under the
// MIT License. See LICENSE-MIT at the repository root for the full text.
//
// @streamed runtime: pinned-arena allocation, budget frames, alignment,
// nested frames, and reclaim on leave. Drives the allocator directly so it
// does not depend on codegen or the interpreter.
//
//   cc -std=c11 vyne/runtime/tests/streamed.c -lm -o streamed && ./streamed

#include "../modules/vyne_stream_runtime.h"
#include <assert.h>
#include <stdint.h>

static int aligned(const void* p) {
    return ((uintptr_t)p % VYNE_TYPED_ARRAY_ALIGN) == 0;
}

int main(void) {
    // --- One frame, Float64 + Int64 --------------------------------------
    vyne_stream_enter(1024);

    VyneArray_f64 a = vyne_stream_f64_alloc(16);
    assert(a.size == 16 && a.cap == 16);
    assert(aligned(a.data));
    for (int i = 0; i < 16; ++i)
        assert(a.data[i] == 0.0);
    a.data[0] = 3.5;
    assert(a.data[0] == 3.5);
    assert(vyne_stream_total_allocated() >= 16 * sizeof(double));
    assert(vyne_stream_block_count() >= 1);

    int pinned = vyne_stream_is_pinned();
    assert(pinned == 0 || pinned == 1); // provider-dependent

    VyneArray_i64 b = vyne_stream_i64_alloc(8);
    assert(b.size == 8 && b.cap == 8);
    assert(aligned(b.data));
    for (int i = 0; i < 8; ++i)
        assert(b.data[i] == 0);
    b.data[0] = 42;
    assert(b.data[0] == 42);
    assert(
        vyne_stream_total_allocated() >=
        16 * sizeof(double) + 8 * sizeof(int64_t)
    );

    // --- Nested frame ----------------------------------------------------
    size_t outer = vyne_stream_total_allocated();
    vyne_stream_enter(256);
    VyneArray_f64 c = vyne_stream_f64_alloc(4);
    assert(c.size == 4);
    assert(vyne_stream_total_allocated() > outer);
    vyne_stream_leave();
    assert(vyne_stream_total_allocated() == outer);

    // --- Reclaim both frames --------------------------------------------
    vyne_stream_leave();
    assert(vyne_stream_total_allocated() == 0);
    assert(vyne_stream_block_count() == 0);
    assert(vyne_stream_peak_allocated() >= 16 * sizeof(double));

    arena_free_all();
    return 0;
}

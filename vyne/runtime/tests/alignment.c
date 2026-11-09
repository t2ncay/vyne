// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tuncay Gafarli
//
// This file is part of the Vyne runtime library, distributed under the
// MIT License. See LICENSE-MIT at the repository root for the full text.
//
// Typed-array payloads must be aligned to VYNE_TYPED_ARRAY_ALIGN so the
// generated C can use aligned loads. Covers both creation and the
// reallocation path in vyne_array_*_push.
//
//   cc -std=c11 vyne/runtime/tests/alignment.c -lm -o alignment && ./alignment

#include "../vyne_runtime.h"
#include <assert.h>
#include <stdint.h>

static int aligned(const void* p) {
    return ((uintptr_t)p % VYNE_TYPED_ARRAY_ALIGN) == 0;
}

int main(void) {
    VyneArray_f64 a = vyne_array_f64_create(100);
    VyneArray_i64 b = vyne_array_i64_create(100);
    assert(aligned(a.data));
    assert(aligned(b.data));

    // Force several reallocations; each new payload keeps the alignment.
    VyneArray_f64 c = vyne_array_f64_create(4);
    for (int i = 0; i < 200; ++i) {
        vyne_array_f64_push(&c, (double)i);
        assert(aligned(c.data));
    }
    assert(c.size == 204); // the initial 4 zero slots + 200 pushes
    assert(c.data[0] == 0.0);
    assert(c.data[203] == 199.0);

    VyneArray_i64 d = vyne_array_i64_create(4);
    for (int i = 0; i < 100; ++i) {
        vyne_array_i64_push(&d, i);
        assert(aligned(d.data));
    }
    assert(d.size == 104); // the initial 4 zero slots + 100 pushes
    assert(d.data[103] == 99);

    // Boxed dispatch path: vyne_array_push/place_all also reallocate the
    // typed payload and must preserve the same alignment.
    VyneArray_f64 e = vyne_array_f64_create(4);
    VyneValue boxed_f64 = vyne_array_f64_to_value(&e);
    for (int i = 0; i < 200; ++i) {
        vyne_array_push(boxed_f64, vyne_float((double)i));
        assert(aligned(boxed_f64.as.arr_f64->data));
    }
    assert(boxed_f64.as.arr_f64->size == 204);

    VyneArray_i64 f = vyne_array_i64_create(4);
    VyneValue boxed_i64 = vyne_array_i64_to_value(&f);
    vyne_array_place_all(boxed_i64, vyne_int(7), 300);
    assert(aligned(boxed_i64.as.arr_i64->data));
    assert(boxed_i64.as.arr_i64->size == 300);
    assert(boxed_i64.as.arr_i64->data[0] == 7 &&
           boxed_i64.as.arr_i64->data[299] == 7);

    arena_free_all();
    return 0;
}

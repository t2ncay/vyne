// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tuncay Gafarli
//
// This file is part of the Vyne runtime library, distributed under the
// MIT License. See LICENSE-MIT at the repository root for the full text.
//
// Poison-on-rewind: a stale read into a rewound arena window must expose the
// poison pattern instead of a plausible leftover value. The test drives the
// arena directly so it does not depend on codegen or the interpreter.
//
//   cc -std=c11 vyne/runtime/tests/poison.c -o poison && ./poison

#include "../detail/arena.h"
#include <assert.h>

static int word_is(const uint8_t* p, uint64_t expected) {
    uint64_t v;
    memcpy(&v, p, sizeof v);
    return v == expected;
}

int main(void) {
    vyne_arena_set_poison(1);
    assert(vyne_arena_poison_enabled() == 1);

    // Warm up so the first checkpoint lives inside a real block; a checkpoint
    // taken before any allocation has cp.block == NULL and rewind frees every
    // block, which is not the case this test is about.
    (void)arena_alloc(8);

    // --- rewind within one block -----------------------------------------
    ArenaCheckpoint cp = arena_checkpoint();
    uint8_t* a = (uint8_t*)arena_alloc(64);
    uint8_t* b = (uint8_t*)arena_alloc(64);
    assert(a != NULL && b != NULL);
    memset(a, 0x11, 64);
    memset(b, 0x22, 64);

    arena_rewind(cp);

    for (int i = 0; i < 8; ++i) {
        assert(word_is(a + i * 8, VYNE_ARENA_POISON_WORD));
        assert(word_is(b + i * 8, VYNE_ARENA_POISON_WORD));
    }

    // --- reclaim poisons the released tail --------------------------------
    uint8_t* c = (uint8_t*)arena_alloc(32);
    assert(c != NULL);
    memset(c, 0x33, 32);
    assert(arena_try_reclaim(c, 32) == 1);
    for (int i = 0; i < 4; ++i)
        assert(word_is(c + i * 8, VYNE_ARENA_POISON_WORD));

    // --- disabled: rewind must not touch the released bytes ---------------
    vyne_arena_set_poison(0);
    assert(vyne_arena_poison_enabled() == 0);
    ArenaCheckpoint cp2 = arena_checkpoint();
    uint8_t* d = (uint8_t*)arena_alloc(64);
    assert(d != NULL);
    memset(d, 0x44, 64);
    (void)arena_alloc(64); // move the bump pointer past d
    arena_rewind(cp2);
    for (int i = 0; i < 64; ++i)
        assert(d[i] == 0x44);

    arena_free_all();
    return 0;
}

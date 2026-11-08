// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tuncay Gafarli
//
// This file is part of the Vyne runtime library, distributed under the
// MIT License. See LICENSE-MIT at the repository root for the full text.
//
// @persistent runtime: commit-arena routing, whole-program lifetime, nested
// frames, commit no-op, and survival across a main-arena rewind. Drives the
// runtime directly so it does not depend on codegen or the interpreter.
//
//   cc -std=c11 vyne/runtime/tests/persistent.c -lm -o persistent && ./persistent

#include "../modules/vmem.h"
#include <assert.h>
#include <string.h>

int main(void) {
    VyneValue a = vyne_string("temp"); // main arena
    ArenaCheckpoint cp = arena_checkpoint();
    size_t main_before = g_arena.total_allocated;

    // --- Outer persistent frame ------------------------------------------
    vyne_persistent_enter();
    VyneValue b = vyne_string("persist");
    assert(g_arena.total_allocated > 0);          // now the commit arena
    assert(g_commit_arena.total_allocated == 0);  // not published yet
    vyne_persistent_leave();

    assert(g_arena.total_allocated == main_before); // enclosing arena restored
    assert(g_commit_arena.total_allocated > 0);     // persistent survives
    assert(strcmp(b.as.str, "persist") == 0);

    // Rewind the main arena; the persistent value is untouched.
    arena_rewind(cp);
    assert(strcmp(b.as.str, "persist") == 0);

    // --- Nested persistent frames ----------------------------------------
    vyne_persistent_enter();
    VyneValue c = vyne_string("outer");
    vyne_persistent_enter();
    VyneValue d = vyne_string("inner");
    vyne_persistent_leave();
    assert(strcmp(c.as.str, "outer") == 0);
    assert(strcmp(d.as.str, "inner") == 0);
    vyne_persistent_leave();
    assert(strcmp(c.as.str, "outer") == 0);
    assert(strcmp(d.as.str, "inner") == 0);

    // --- commit() inside persistent is a no-op ---------------------------
    vyne_persistent_enter();
    VyneValue e = vyne_string("x");
    VyneValue f = vmem_runtime_commit(e);
    assert(f.as.str == e.as.str);
    vyne_persistent_leave();
    assert(strcmp(f.as.str, "x") == 0);

    // --- Persistent values survive a later main rewind -------------------
    ArenaCheckpoint cp2 = arena_checkpoint();
    (void)vyne_string("temp2");
    arena_rewind(cp2);
    assert(strcmp(b.as.str, "persist") == 0);
    assert(strcmp(c.as.str, "outer") == 0);
    assert(strcmp(d.as.str, "inner") == 0);

    (void)a;
    vmem_runtime_reset();
    return 0;
}

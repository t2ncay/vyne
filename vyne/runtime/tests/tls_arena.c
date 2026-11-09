// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tuncay Gafarli
//
// This file is part of the Vyne runtime library, distributed under the
// MIT License. See LICENSE-MIT at the repository root for the full text.
//
// Thread-local arena + shared commit-arena stress test.
//
// Every worker owns an independent region arena (g_arena and its bump
// pointers are thread-local), so local allocations must not interfere.
// Workers publish one typed array each through vmem_runtime_commit(),
// which serializes on the shared commit-arena spinlock. The main thread
// joins, checks every published copy byte-for-byte, and frees the shared
// arena once at process teardown.
//
//   cc -std=c11 -pthread vyne/runtime/tests/tls_arena.c -lm -o tls_arena
//   ./tls_arena
//
// On Windows the same test uses CreateThread; build with a MinGW gcc.

#include "../modules/vmem.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
typedef DWORD  VyneThreadRet;
typedef LPVOID VyneThreadArg;
#define VYNE_THREAD_FN(name) static DWORD WINAPI name(LPVOID arg)
static int run_threads(
    int count,
    DWORD (WINAPI* fn)(LPVOID),
    void** args
) {
    HANDLE handles[16];
    for (int i = 0; i < count; ++i)
        handles[i] = CreateThread(NULL, 0, fn, args[i], 0, NULL);
    for (int i = 0; i < count; ++i) {
        WaitForSingleObject(handles[i], INFINITE);
        CloseHandle(handles[i]);
    }
    return 0;
}
#else
#include <pthread.h>
typedef void* VyneThreadRet;
typedef void* VyneThreadArg;
#define VYNE_THREAD_FN(name) static void* name(void* arg)
static int run_threads(
    int count,
    void* (*fn)(void*),
    void** args
) {
    pthread_t handles[16];
    for (int i = 0; i < count; ++i)
        pthread_create(&handles[i], NULL, fn, args[i]);
    for (int i = 0; i < count; ++i)
        pthread_join(handles[i], NULL);
    return 0;
}
#endif

#define VYNE_TEST_THREADS 8
#define VYNE_TEST_ITERS   2000
#define VYNE_TEST_COMMIT_N 128

typedef struct {
    int id;
    int ok;
    VyneValue committed;
} Worker;

VYNE_THREAD_FN(worker_main) {
    Worker* w = (Worker*)arg;
    w->ok = 1;

    /* Local allocation churn on this thread's own bump arena. If g_arena
     * were still process-global, concurrent workers would corrupt each
     * other's bump pointers and the pattern check below would fail. */
    for (int iter = 0; iter < VYNE_TEST_ITERS; ++iter) {
        ArenaCheckpoint cp = arena_checkpoint();
        VyneArray_f64 a = vyne_array_f64_create(32);
        for (int j = 0; j < 32; ++j)
            a.data[j] = (double)(w->id * 100000 + iter * 100 + j);
        for (int j = 0; j < 32; ++j) {
            if (a.data[j] != (double)(w->id * 100000 + iter * 100 + j)) {
                w->ok = 0;
                break;
            }
        }
        arena_rewind(cp);
    }

    /* Publish one array to the shared commit arena. This is the only
     * cross-thread write in the test and must be lock-protected. */
    ArenaCheckpoint cp = arena_checkpoint();
    VyneArray_f64 out = vyne_array_f64_create(VYNE_TEST_COMMIT_N);
    for (int j = 0; j < VYNE_TEST_COMMIT_N; ++j)
        out.data[j] = (double)(w->id * 1000 + j);
    VyneValue boxed = vyne_array_f64_to_value(&out);
    w->committed = vmem_runtime_commit(boxed);
    arena_rewind(cp); /* the source copy dies; the commit copy does not */

    if (w->committed.type != V_F64_ARRAY || w->committed.as.arr_f64 == NULL) {
        w->ok = 0;
    } else {
        VyneArray_f64* c = w->committed.as.arr_f64;
        if (c->size != VYNE_TEST_COMMIT_N) {
            w->ok = 0;
        } else {
            for (int j = 0; j < VYNE_TEST_COMMIT_N; ++j) {
                if (c->data[j] != (double)(w->id * 1000 + j)) {
                    w->ok = 0;
                    break;
                }
            }
        }
    }

    /* Each worker owns its arena; this must not touch the commit arena. */
    vmem_runtime_thread_cleanup();
    return (VyneThreadRet)0;
}

int main(void) {
    Worker workers[VYNE_TEST_THREADS];
    void* args[VYNE_TEST_THREADS];
    memset(workers, 0, sizeof workers);

    for (int i = 0; i < VYNE_TEST_THREADS; ++i) {
        workers[i].id = i;
        args[i] = &workers[i];
    }

    run_threads(VYNE_TEST_THREADS, worker_main, args);

    size_t expect_bytes =
        (size_t)VYNE_TEST_THREADS * VYNE_TEST_COMMIT_N * sizeof(double);
    vyne_commit_lock_acquire();
    size_t commit_bytes = g_commit_arena.total_allocated;
    vyne_commit_lock_release();
    assert(commit_bytes >= expect_bytes);

    for (int i = 0; i < VYNE_TEST_THREADS; ++i) {
        assert(workers[i].ok);
        assert(workers[i].committed.type == V_F64_ARRAY);
        VyneArray_f64* c = workers[i].committed.as.arr_f64;
        assert(c != NULL);
        assert(c->size == VYNE_TEST_COMMIT_N);
        for (int j = 0; j < VYNE_TEST_COMMIT_N; ++j)
            assert(c->data[j] == (double)(workers[i].id * 1000 + j));
    }

    printf("tls_arena: %d threads, %d local iterations, %zu commit bytes\n",
           VYNE_TEST_THREADS, VYNE_TEST_ITERS, commit_bytes);

    arena_free_all();
    return 0;
}

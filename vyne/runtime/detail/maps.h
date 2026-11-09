// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tuncay Gafarli
//
// This file is part of the Vyne runtime library, distributed under the
// MIT License. See LICENSE-MIT at the repository root for the full text.

#pragma once
#include "values.h"
// ============================================================================
// MAP OPERATIONS
// ============================================================================

static inline uint32_t _vyne_hash_str(const char* s) {
    uint32_t h = 2166136261u;   // FNV-1a
    while (*s) {
        h ^= (uint8_t)*s++;
        h *= 16777619u;
    }
    return h;
}

// ----------------------------------------------------------------------------
// Intern table. Every map key is interned through this on the way in,
// so that:
//   - two identical string literals from different call sites collapse
//     to the same arena buffer, and
//   - map lookups can compare precomputed hashes before ever calling
//     strcmp.
// The table lives in malloc'd memory — NOT the arena — so that region
// rewind does not touch it. It is torn down by arena_free_all.
// ----------------------------------------------------------------------------

typedef struct {
    const char* canon;   // canonical arena pointer for this key
    uint32_t    hash;
    uint32_t    len;     // strlen(canon), precomputed
} _VyneInternEntry;

static _VyneInternEntry* _vyne_intern_table = NULL;
static size_t _vyne_intern_cap  = 0;
static size_t _vyne_intern_size = 0;

// Forward declaration. _vyne_intern_grow registers this function as the
// arena cleanup hook, but the function's own definition sits below —
// after _vyne_intern, which grow does not depend on. Ordering matters
// here because C has no implicit forward declaration for static functions.
static void _vyne_intern_shutdown(void);

static void _vyne_intern_grow(void) {
    // Register the cleanup hook on first table allocation. arena_free_all
    // will call it to clear the table before freeing the blocks that hold
    // our canonical key pointers. Register exactly once.
    if (g_arena_cleanup_hook == NULL) {
        g_arena_cleanup_hook = _vyne_intern_shutdown;
    }

    size_t new_cap = _vyne_intern_cap ? _vyne_intern_cap * 2 : 256;
    _VyneInternEntry* nt = (_VyneInternEntry*)calloc(new_cap, sizeof(*nt));
    if (!nt) { fprintf(stderr, "vyne: intern table OOM\n"); exit(1); }
    size_t mask = new_cap - 1;
    for (size_t i = 0; i < _vyne_intern_cap; ++i) {
        if (!_vyne_intern_table[i].canon) continue;
        uint32_t h = _vyne_intern_table[i].hash;
        size_t   j = h & mask;
        while (nt[j].canon) j = (j + 1) & mask;
        nt[j] = _vyne_intern_table[i];
    }
    free(_vyne_intern_table);
    _vyne_intern_table = nt;
    _vyne_intern_cap   = new_cap;
}

static inline const char* _vyne_intern(const char* s, uint32_t* out_hash) {
    if (!s) { *out_hash = 0; return NULL; }

    if (_vyne_intern_cap == 0) _vyne_intern_grow();
    if (_vyne_intern_size * 4 >= _vyne_intern_cap * 3) _vyne_intern_grow();

    uint32_t h    = _vyne_hash_str(s);
    size_t   mask = _vyne_intern_cap - 1;
    size_t   i    = h & mask;
    *out_hash = h;

    for (;;) {
        const char* k = _vyne_intern_table[i].canon;
        if (k == NULL) {
            // Miss — copy into the arena and record.
            size_t len = strlen(s);
            char*  buf = (char*)arena_alloc(len + 1);
            memcpy(buf, s, len + 1);
            _vyne_intern_table[i].canon = buf;
            _vyne_intern_table[i].hash  = h;
            _vyne_intern_table[i].len   = (uint32_t)len;
            _vyne_intern_size++;
            return buf;
        }
        if (_vyne_intern_table[i].hash == h &&
            memcmp(k, s, _vyne_intern_table[i].len + 1) == 0) {
            return k;
        }
        i = (i + 1) & mask;
    }
}

static void _vyne_intern_shutdown(void) {
    free(_vyne_intern_table);
    _vyne_intern_table = NULL;
    _vyne_intern_cap   = 0;
    _vyne_intern_size  = 0;
    g_arena_cleanup_hook = NULL;
}

static inline VyneValue vyne_map_create(void) {
    VyneValue val = { .type = V_MAP };
    VyneMap* m = (VyneMap*)arena_alloc(sizeof(VyneMap));
    m->capacity   = 8;
    m->size       = 0;
    m->tombstones = 0;
    m->entries    = (VyneMapEntry*)arena_alloc(sizeof(VyneMapEntry) * m->capacity);
    for (int i = 0; i < m->capacity; i++) {
        m->entries[i].key = NULL;
    }
    val.as.map = m;
    return val;
}

static inline int _vyne_map_find_hashed(VyneMap* m, const char* key_canon,
                                        uint32_t kh) {
    // Every caller already has the hash: vyne_map_get/set/has/delete intern
    // the key first, and _vyne_map_find hashes it once. Re-hashing the
    // canonical pointer here doubled the cost of every lookup probe.
    if (m->size == 0) return -1;
    uint32_t mask = (uint32_t)(m->capacity - 1);
    uint32_t i = kh & mask;

    for (int probes = 0; probes < m->capacity; ++probes) {
        const char* k = m->entries[i].key;
        if (k == NULL) return -1;
        if (k != VYNE_TOMBSTONE &&
            (k == key_canon || strcmp(k, key_canon) == 0)) {
            return (int)i;
        }
        i = (i + 1) & mask;
    }
    return -1;
}

static inline int _vyne_map_find(VyneMap* m, const char* key) {
    if (m->size == 0) return -1;
    uint32_t kh = _vyne_hash_str(key);
    return _vyne_map_find_hashed(m, key, kh);
}

static void _vyne_map_rehash(VyneMap* m, int new_cap) {
    VyneMapEntry* old      = m->entries;
    int           old_cap  = m->capacity;

    m->entries    = (VyneMapEntry*)arena_alloc(sizeof(VyneMapEntry) * new_cap);
    m->capacity   = new_cap;
    m->size       = 0;
    m->tombstones = 0;
    for (int i = 0; i < new_cap; i++) m->entries[i].key = NULL;

    uint32_t mask = (uint32_t)(new_cap - 1);
    for (int i = 0; i < old_cap; i++) {
        const char* k = old[i].key;
        if (k == NULL || k == VYNE_TOMBSTONE) continue;
        uint32_t j = _vyne_hash_str(k) & mask;
        while (m->entries[j].key != NULL) j = (j + 1) & mask;
        m->entries[j].key   = k;
        m->entries[j].value = old[i].value;
        m->size++;
    }
}

static inline void vyne_map_set(VyneValue map_val, VyneValue key, VyneValue val) {
    if (map_val.type != V_MAP || key.type != V_STRING) return;
    VyneMap* m = map_val.as.map;

    // Intern the key first. This both collapses duplicate literals and
    // hands us the hash for the probe below.
    uint32_t kh = 0;
    const char* key_canon = _vyne_intern(key.as.str, &kh);

    // Grow or compact if load factor is getting high.
    if ((m->size + m->tombstones) * 4 >= m->capacity * 3) {
        int new_cap = (m->size * 2 > m->capacity) ? m->capacity * 2 : m->capacity;
        _vyne_map_rehash(m, new_cap);
    }

    uint32_t mask = (uint32_t)(m->capacity - 1);
    uint32_t i = kh & mask;
    int first_tomb = -1;

    for (;;) {
        const char* k = m->entries[i].key;
        if (k == NULL) {
            if (first_tomb >= 0) i = (uint32_t)first_tomb;
            else m->size++;
            m->entries[i].key   = key_canon;
            m->entries[i].value = val;
            if (first_tomb >= 0) m->tombstones--;
            return;
        }
        if (k == VYNE_TOMBSTONE) {
            if (first_tomb < 0) first_tomb = (int)i;
        } else if (k == key_canon || strcmp(k, key_canon) == 0) {
            // Pointer compare first — intern guarantees unique pointers
            // per distinct key, so a pointer match is a full match.
            m->entries[i].value = val;
            return;
        }
        i = (i + 1) & mask;
    }
}

static inline VyneValue vyne_map_get(VyneValue map_val, VyneValue key) {
    if (map_val.type != V_MAP || key.type != V_STRING) return vyne_null();
    uint32_t kh = 0;
    const char* key_canon = _vyne_intern(key.as.str, &kh);
    VyneMap* m = map_val.as.map;
    int idx = _vyne_map_find_hashed(m, key_canon, kh);
    if (idx >= 0) return m->entries[idx].value;
    return vyne_null();
}

static inline bool vyne_map_has(VyneValue map_val, VyneValue key) {
    if (map_val.type != V_MAP || key.type != V_STRING) return false;
    uint32_t kh = 0;
    const char* key_canon = _vyne_intern(key.as.str, &kh);
    return _vyne_map_find_hashed(map_val.as.map, key_canon, kh) >= 0;
}

static inline void vyne_map_delete(VyneValue map_val, VyneValue key) {
    if (map_val.type != V_MAP || key.type != V_STRING) return;
    uint32_t kh = 0;
    const char* key_canon = _vyne_intern(key.as.str, &kh);
    VyneMap* m = map_val.as.map;
    int idx = _vyne_map_find_hashed(m, key_canon, kh);
    if (idx < 0) return;

    uint32_t mask = (uint32_t)(m->capacity - 1);
    uint32_t i = (uint32_t)idx;
    uint32_t j = (i + 1) & mask;

    // Backward-shift: pull any subsequent entry whose probe distance
    // would still be valid one slot earlier into the vacated slot. Now
    // the table never accumulates tombstones, and _vyne_map_find_hashed
    // stops on the first NULL.
    while (m->entries[j].key != NULL && m->entries[j].key != VYNE_TOMBSTONE) {
        uint32_t h = _vyne_hash_str(m->entries[j].key) & mask;
        uint32_t dist_i = (i - h) & mask;
        uint32_t dist_j = (j - h) & mask;
        if (dist_i < dist_j) {
            m->entries[i] = m->entries[j];
            i = j;
        }
        j = (j + 1) & mask;
    }
    m->entries[i].key = NULL;
    m->size--;
    // tombstones stays 0 forever under this scheme.
}

static inline void vyne_map_clear(VyneValue map_val) {
    if (map_val.type != V_MAP) return;
    VyneMap* m = map_val.as.map;
    for (int i = 0; i < m->capacity; i++) m->entries[i].key = NULL;
    m->size       = 0;
    m->tombstones = 0;
}

static inline VyneValue vyne_map_keys(VyneValue map_val) {
    if (map_val.type != V_MAP) return vyne_array_create(0);
    VyneMap* m = map_val.as.map;
    VyneValue res = vyne_array_create(m->size);
    VyneValue* elems = res.as.arr->elements;
    int n = 0;
    for (int i = 0; i < m->capacity && n < m->size; i++) {
        const char* k = m->entries[i].key;
        if (k == NULL || k == VYNE_TOMBSTONE) continue;
        elems[n++] = vyne_string(k);
    }
    return res;
}

static inline VyneValue vyne_map_values(VyneValue map_val) {
    if (map_val.type != V_MAP) return vyne_array_create(0);
    VyneMap* m = map_val.as.map;
    VyneValue res = vyne_array_create(m->size);
    VyneValue* elems = res.as.arr->elements;
    int n = 0;
    for (int i = 0; i < m->capacity && n < m->size; i++) {
        const char* k = m->entries[i].key;
        if (k == NULL || k == VYNE_TOMBSTONE) continue;
        elems[n++] = m->entries[i].value;
    }
    return res;
}

static inline VyneValue vyne_index_get(VyneValue base, VyneValue index) {
    if (base.type == V_ARRAY || base.type == V_F64_ARRAY ||
        base.type == V_F32_ARRAY || base.type == V_I64_ARRAY)
        return vyne_array_get(base, index);
    if (base.type == V_MAP) return vyne_map_get(base, index);
    if (base.type == V_STRING && index.type == V_INT64) {
        const char* s = base.as.str;
        int len = (int)strlen(s);
        int idx = (int)index.as.i64;
        if (idx < 0 || idx >= len) return vyne_null();
        return vyne_char_at(s, idx);
    }
    return vyne_null();
}

static inline void vyne_index_set(VyneValue base, VyneValue index, VyneValue val) {
    if (base.type == V_ARRAY || base.type == V_F64_ARRAY ||
        base.type == V_F32_ARRAY || base.type == V_I64_ARRAY) {
        vyne_array_set(base, index, val); return;
    }
    if (base.type == V_MAP) { vyne_map_set(base, index, val); return; }
}

static inline void vyne_dismiss_module(const char* name) {
    (void)name;
}


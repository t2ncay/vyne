#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <ctype.h>

#if defined(_MSC_VER)
    #define VYNE_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
    #define VYNE_NOINLINE __attribute__((noinline))
#else
    #define VYNE_NOINLINE
#endif

#define VYNE_ARENA_BLOCK_SIZE (8 * 1024 * 1024)
#define VYNE_MAX_METHODS 256
#define VYNE_MAX_FRAME_SIZE 1024

// ============================================================================
// ARENA ALLOCATOR
// ============================================================================

typedef struct ArenaBlock {
    uint8_t* data;
    size_t used;
    size_t capacity;
    struct ArenaBlock* next;
} ArenaBlock;

typedef struct {
    ArenaBlock* head;
    size_t total_allocated;
} VyneArena;

static VyneArena g_arena = { NULL, 0 };

static inline ArenaBlock* arena_new_block(size_t min_size) {
    size_t cap = min_size > VYNE_ARENA_BLOCK_SIZE ? min_size : VYNE_ARENA_BLOCK_SIZE;
    ArenaBlock* block = (ArenaBlock*)malloc(sizeof(ArenaBlock));
    block->data = (uint8_t*)malloc(cap);
    block->used = 0;
    block->capacity = cap;
    block->next = NULL;
    return block;
}

static inline void* arena_alloc(size_t size) {
    size = (size + 7) & ~(size_t)7;
    if (!g_arena.head || g_arena.head->used + size > g_arena.head->capacity) {
        ArenaBlock* block = arena_new_block(size);
        block->next = g_arena.head;
        g_arena.head = block;
    }
    void* ptr = g_arena.head->data + g_arena.head->used;
    g_arena.head->used += size;
    g_arena.total_allocated += size;
    return ptr;
}

static inline void arena_free_all(void) {
    ArenaBlock* block = g_arena.head;
    while (block) {
        ArenaBlock* next = block->next;
        free(block->data);
        free(block);
        block = next;
    }
    g_arena.head = NULL;
    g_arena.total_allocated = 0;
}

// ============================================================================
// VALUE TYPES
// ============================================================================

typedef struct VyneValue VyneValue;
struct VyneArray;
struct VyneStruct;

typedef VyneValue (*VyneMethodFn)(int argc, VyneValue* args);

typedef enum {
    V_NULL = 0,
    V_FLOAT64 = 1,
    V_INT64 = 2,
    V_STRING = 3,
    V_ARRAY = 4,
    V_BOOL = 5,
    V_STRUCT = 6,
    V_FUNCTION = 7,
    V_MODULE = 8,
    V_REFERENCE = 9,
    V_MAP = 10
} VyneType;

struct VyneValue {
    VyneType type;
    union {
        double f64;
        int64_t i64;
        char* str;
        struct VyneArray* arr;
        struct VyneStruct* strct;
        struct VyneFunction* fn;
        struct VyneMap* map;
        void* ptr;
    } as;
    int ref_count;
};

// ============================================================================
// ARRAY
// ============================================================================

typedef struct VyneArray {
    VyneValue* elements;
    int size;
    int capacity;
} VyneArray;

// ============================================================================
// MAP
// ============================================================================
typedef struct VyneMapEntry {
    const char* key;
    VyneValue value;
} VyneMapEntry;

typedef struct VyneMap {
    VyneMapEntry* entries;
    int size;
    int capacity;
} VyneMap;

// ============================================================================
// STRUCT
// ============================================================================

typedef struct VyneField {
    uint32_t id;
    const char* name;
    VyneValue value;
} VyneField;

typedef struct VyneStruct {
    const char* type_name;
    VyneField* fields;
    int field_count;
    struct VyneMethodEntry** methods;
    int method_count;
} VyneStruct;

static inline bool vyne_values_equal(VyneValue a, VyneValue b);
static inline VyneValue vyne_to_string(VyneValue v);

static inline bool vyne_values_equal(VyneValue a, VyneValue b);
static inline VyneValue vyne_to_string(VyneValue v);

static inline VyneValue vyne_array_get(VyneValue arr_val, VyneValue index_val);
static inline void vyne_array_set(VyneValue arr_val, VyneValue index_val, VyneValue rhs);
static inline void vyne_array_push(VyneValue arr_val, VyneValue val);
static inline VyneValue vyne_map_deepcopy(VyneValue mp);

// ============================================================================
// FUNCTION
// ============================================================================

typedef struct VyneFunction {
    int arity;
    int param_count;
    const char** param_names;
    VyneType* param_types;
    bool* is_reference;
    VyneValue* constants;
    uint8_t* bytecode;
    size_t bytecode_size;
    VyneMethodFn native_fn;
    bool is_native;
    const char* name;
    int frame_size;
    const char* expected_return_type;
} VyneFunction;

// ============================================================================
// METHOD TABLE
// ============================================================================

typedef struct VyneMethodEntry {
    const char* type_name;
    const char* method_name;
    VyneMethodFn fn;
} VyneMethodEntry;

static VyneMethodEntry g_method_table[VYNE_MAX_METHODS];
static int g_method_count = 0;

// ============================================================================
// VALUE CREATION
// ============================================================================

static inline VyneValue vyne_null(void) {
    VyneValue val = { .type = V_NULL, .ref_count = 0 };
    val.as.i64 = 0;
    return val;
}

static inline VyneValue vyne_int(int64_t v) {
    VyneValue val = { .type = V_INT64, .ref_count = 0 };
    val.as.i64 = v;
    return val;
}

static inline VyneValue vyne_bool(bool v) {
    VyneValue val = { .type = V_BOOL, .ref_count = 0 };
    val.as.i64 = v ? 1 : 0;
    return val;
}

static inline VyneValue vyne_float(double v) {
    VyneValue val = { .type = V_FLOAT64, .ref_count = 0 };
    val.as.f64 = v;
    return val;
}

static inline VyneValue vyne_string(const char* s) {
    VyneValue val = { .type = V_STRING, .ref_count = 0 };
    if (s) {
        size_t len = strlen(s) + 1;
        char* copy = (char*)arena_alloc(len);
        memcpy(copy, s, len);
        val.as.str = copy;
    } else {
        val.as.str = NULL;
    }
    return val;
}

static inline VyneValue vyne_string_own(char* s) {
    VyneValue val = { .type = V_STRING, .ref_count = 0 };
    val.as.str = s;
    return val;
}

static inline VyneValue vyne_array_create(int initial_size) {
    VyneValue val = { .type = V_ARRAY, .ref_count = 0 };
    VyneArray* arr = (VyneArray*)arena_alloc(sizeof(VyneArray));
    int cap = initial_size > 0 ? initial_size : 4;
    arr->elements = (VyneValue*)arena_alloc(sizeof(VyneValue) * cap);
    arr->size = initial_size;
    arr->capacity = cap;
    for (int i = 0; i < initial_size; i++)
        arr->elements[i] = vyne_null();
    val.as.arr = arr;
    return val;
}

static inline VyneValue vyne_struct_create(const char* type_name) {
    VyneValue val = { .type = V_STRUCT, .ref_count = 0 };
    VyneStruct* s = (VyneStruct*)arena_alloc(sizeof(VyneStruct));
    s->type_name = type_name;
    s->fields = NULL;
    s->field_count = 0;
    s->methods = NULL;
    s->method_count = 0;
    val.as.strct = s;
    return val;
}

// ============================================================================
// MAP OPERATIONS
// ============================================================================

static inline VyneValue vyne_map_create(void) {
    VyneValue val = { .type = V_MAP, .ref_count = 0 };
    VyneMap* m = (VyneMap*)arena_alloc(sizeof(VyneMap));
    m->capacity = 8;
    m->size = 0;
    m->entries = (VyneMapEntry*)arena_alloc(sizeof(VyneMapEntry) * m->capacity);
    val.as.map = m;
    return val;
}

static inline int _vyne_map_find(VyneMap* m, const char* key) {
    for (int i = 0; i < m->size; i++) {
        if (strcmp(m->entries[i].key, key) == 0) return i;
    }
    return -1;
}

static inline void vyne_map_set(VyneValue map_val, VyneValue key, VyneValue val) {
    if (map_val.type != V_MAP || key.type != V_STRING) return;
    VyneMap* m = map_val.as.map;
    int idx = _vyne_map_find(m, key.as.str);
    if (idx >= 0) { m->entries[idx].value = val; return; }

    if (m->size >= m->capacity) {
        int new_cap = m->capacity * 2;
        VyneMapEntry* new_e = (VyneMapEntry*)arena_alloc(sizeof(VyneMapEntry) * new_cap);
        memcpy(new_e, m->entries, sizeof(VyneMapEntry) * m->size);
        m->entries = new_e;
        m->capacity = new_cap;
    }
    m->entries[m->size].key = key.as.str;
    m->entries[m->size].value = val;
    m->size++;
}

static inline VyneValue vyne_map_get(VyneValue map_val, VyneValue key) {
    if (map_val.type != V_MAP || key.type != V_STRING) return vyne_null();
    VyneMap* m = map_val.as.map;
    int idx = _vyne_map_find(m, key.as.str);
    if (idx >= 0) return m->entries[idx].value;
    return vyne_null();
}

static inline bool vyne_map_has(VyneValue map_val, VyneValue key) {
    if (map_val.type != V_MAP || key.type != V_STRING) return false;
    VyneMap* m = map_val.as.map;
    return _vyne_map_find(m, key.as.str) >= 0;
}

static inline void vyne_map_delete(VyneValue map_val, VyneValue key) {
    if (map_val.type != V_MAP || key.type != V_STRING) return;
    VyneMap* m = map_val.as.map;
    int idx = _vyne_map_find(m, key.as.str);
    if (idx < 0) return;
    for (int i = idx; i < m->size - 1; i++) {
        m->entries[i] = m->entries[i + 1];
    }
    m->size--;
}

static inline void vyne_map_clear(VyneValue map_val) {
    if (map_val.type != V_MAP) return;
    map_val.as.map->size = 0;
}

static inline VyneValue vyne_map_keys(VyneValue map_val) {
    if (map_val.type != V_MAP) return vyne_array_create(0);
    VyneMap* m = map_val.as.map;
    VyneValue res = vyne_array_create(0);
    for (int i = 0; i < m->size; i++) {
        vyne_array_push(res, vyne_string(m->entries[i].key));
    }
    return res;
}

static inline VyneValue vyne_map_values(VyneValue map_val) {
    if (map_val.type != V_MAP) return vyne_array_create(0);
    VyneMap* m = map_val.as.map;
    VyneValue res = vyne_array_create(0);
    for (int i = 0; i < m->size; i++) {
        vyne_array_push(res, m->entries[i].value);
    }
    return res;
}

static inline VyneValue vyne_index_get(VyneValue base, VyneValue index) {
    if (base.type == V_ARRAY) return vyne_array_get(base, index);
    if (base.type == V_MAP) return vyne_map_get(base, index);
    if (base.type == V_STRING && index.type == V_INT64) {
        const char* s = base.as.str;
        int len = (int)strlen(s);
        int idx = (int)index.as.i64;
        if (idx < 0 || idx >= len) return vyne_null();
        char buf[2] = { s[idx], '\0' };
        return vyne_string(buf);
    }
    return vyne_null();
}

static inline void vyne_index_set(VyneValue base, VyneValue index, VyneValue val) {
    if (base.type == V_ARRAY) { vyne_array_set(base, index, val); return; }
    if (base.type == V_MAP)   { vyne_map_set(base, index, val);   return; }
}

static inline void vyne_dismiss_module(const char* name) {
    (void)name;
}

// ============================================================================
// STRING OPERATIONS
// ============================================================================

static inline VyneValue vyne_string_substr(VyneValue str, int64_t start, int64_t count) {
    if (str.type != V_STRING) return vyne_null();
    const char* s = str.as.str;
    int64_t len = (int64_t)strlen(s);
    if (start < 0) start = 0;
    if (start > len) start = len;
    if (count < 0) {
        // till end
        return vyne_string(s + start);
    }
    if (start + count > len) count = len - start;
    char* buf = (char*)arena_alloc(count + 1);
    memcpy(buf, s + start, count);
    buf[count] = '\0';
    return vyne_string(buf);
}

static inline VyneValue vyne_string_find(VyneValue str, VyneValue target) {
    if (str.type != V_STRING || target.type != V_STRING) return vyne_int(-1);
    const char* found = strstr(str.as.str, target.as.str);
    if (!found) return vyne_int(-1);
    return vyne_int((int64_t)(found - str.as.str));
}

static inline VyneValue vyne_string_uppercase(VyneValue str) {
    if (str.type != V_STRING) return vyne_null();
    size_t len = strlen(str.as.str);
    char* buf = (char*)arena_alloc(len + 1);
    for (size_t i = 0; i < len; i++) buf[i] = (char)toupper((unsigned char)str.as.str[i]);
    buf[len] = '\0';
    return vyne_string(buf);
}

static inline VyneValue vyne_string_lowercase(VyneValue str) {
    if (str.type != V_STRING) return vyne_null();
    size_t len = strlen(str.as.str);
    char* buf = (char*)arena_alloc(len + 1);
    for (size_t i = 0; i < len; i++) buf[i] = (char)tolower((unsigned char)str.as.str[i]);
    buf[len] = '\0';
    return vyne_string(buf);
}

static inline VyneValue vyne_string_trim(VyneValue str) {
    if (str.type != V_STRING) return vyne_null();
    const char* s = str.as.str;
    size_t len = strlen(s);
    size_t start = 0;
    while (start < len && isspace((unsigned char)s[start])) start++;
    size_t end = len;
    while (end > start && isspace((unsigned char)s[end-1])) end--;
    size_t new_len = end - start;
    char* buf = (char*)arena_alloc(new_len + 1);
    memcpy(buf, s + start, new_len);
    buf[new_len] = '\0';
    return vyne_string(buf);
}

static inline VyneValue vyne_string_replace(VyneValue str, VyneValue old_s, VyneValue new_s) {
    if (str.type != V_STRING || old_s.type != V_STRING || new_s.type != V_STRING) return vyne_null();
    const char* src = str.as.str;
    const char* o = old_s.as.str;
    const char* n = new_s.as.str;
    size_t olen = strlen(o);
    if (olen == 0) return str;
    size_t nlen = strlen(n);

    size_t count = 0;
    const char* p = src;
    while ((p = strstr(p, o)) != NULL) { count++; p += olen; }

    size_t src_len = strlen(src);
    size_t result_len = src_len + count * (nlen >= olen ? (nlen - olen) : 0);
    if (nlen < olen) result_len = src_len - count * (olen - nlen);

    char* buf = (char*)arena_alloc(result_len + 1);
    size_t pos = 0;
    p = src;
    while (1) {
        const char* found = strstr(p, o);
        if (!found) {
            size_t tail = strlen(p);
            memcpy(buf + pos, p, tail);
            pos += tail;
            break;
        }
        size_t head = found - p;
        memcpy(buf + pos, p, head); pos += head;
        memcpy(buf + pos, n, nlen); pos += nlen;
        p = found + olen;
    }
    buf[pos] = '\0';
    return vyne_string(buf);
}

// ============================================================================
// ARRAY OPERATIONS
// ============================================================================

static inline VyneValue vyne_array_get(VyneValue arr_val, VyneValue index_val) {
    if (arr_val.type != V_ARRAY || index_val.type != V_INT64) return vyne_null();
    VyneArray* arr = arr_val.as.arr;
    int idx = (int)index_val.as.i64;
    if (idx < 0 || idx >= arr->size) return vyne_null();
    return arr->elements[idx];
}

static inline void vyne_array_set(VyneValue arr_val, VyneValue index_val, VyneValue rhs) {
    if (arr_val.type != V_ARRAY || index_val.type != V_INT64) return;
    VyneArray* arr = arr_val.as.arr;
    int idx = (int)index_val.as.i64;
    if (idx >= 0 && idx < arr->size) {
        arr->elements[idx] = rhs;
    }
}

static inline void vyne_array_push(VyneValue arr_val, VyneValue val) {
    if (arr_val.type != V_ARRAY) return;
    VyneArray* arr = arr_val.as.arr;
    if (arr->size >= arr->capacity) {
        int new_cap = arr->capacity * 2;
        VyneValue* new_elems = (VyneValue*)arena_alloc(sizeof(VyneValue) * new_cap);
        memcpy(new_elems, arr->elements, sizeof(VyneValue) * arr->size);
        arr->elements = new_elems;
        arr->capacity = new_cap;
    }
    arr->elements[arr->size++] = val;
}

static inline VyneValue vyne_array_deepcopy(VyneValue arr) {
    if (arr.type != V_ARRAY) return arr;
    VyneValue result = vyne_array_create(0);
    VyneArray* src = arr.as.arr;
    for (int i = 0; i < src->size; i++) {
        VyneValue elem = src->elements[i];
        if (elem.type == V_ARRAY) elem = vyne_array_deepcopy(elem);
        else if (elem.type == V_MAP) elem = vyne_map_deepcopy(elem);
        vyne_array_push(result, elem);
    }
    return result;
}

static inline VyneValue vyne_map_deepcopy(VyneValue mp) {
    if (mp.type != V_MAP) return mp;
    VyneValue result = vyne_map_create();
    VyneMap* src = mp.as.map;
    for (int i = 0; i < src->size; i++) {
        VyneValue elem = src->entries[i].value;
        if (elem.type == V_ARRAY) elem = vyne_array_deepcopy(elem);
        else if (elem.type == V_MAP) elem = vyne_map_deepcopy(elem);
        vyne_map_set(result, vyne_string(src->entries[i].key), elem);
    }
    return result;
}

static inline VyneValue vyne_array_pop(VyneValue arr_val) {
    if (arr_val.type != V_ARRAY) return vyne_null();
    VyneArray* arr = arr_val.as.arr;
    if (arr->size == 0) return vyne_null();
    return arr->elements[--arr->size];
}

static inline VyneValue vyne_array_reverse(VyneValue arr_val) {
    if (arr_val.type != V_ARRAY) return arr_val;
    VyneArray* arr = arr_val.as.arr;
    if (arr->size <= 1) return arr_val;
    int start = 0, end = arr->size - 1;
    while (start < end) {
        VyneValue temp = arr->elements[start];
        arr->elements[start] = arr->elements[end];
        arr->elements[end] = temp;
        start++;
        end--;
    }
    return arr_val;
}

static inline bool vyne_array_contains(VyneValue arr_val, VyneValue target) {
    if (arr_val.type != V_ARRAY) return false;
    VyneArray* arr = arr_val.as.arr;
    for (int i = 0; i < arr->size; i++) {
        if (vyne_values_equal(arr->elements[i], target)) return true;
    }
    return false;
}

// ============================================================================
// EXTENDED ARRAY OPERATIONS
// ============================================================================

static inline VyneValue vyne_array_pop_front(VyneValue arr_val) {
    if (arr_val.type != V_ARRAY) return vyne_null();
    VyneArray* arr = arr_val.as.arr;
    if (arr->size == 0) return vyne_null();
    VyneValue front = arr->elements[0];
    for (int i = 1; i < arr->size; i++) arr->elements[i - 1] = arr->elements[i];
    arr->size--;
    return front;
}

static inline VyneValue vyne_array_back(VyneValue arr_val) {
    if (arr_val.type != V_ARRAY) return vyne_null();
    VyneArray* arr = arr_val.as.arr;
    if (arr->size == 0) return vyne_null();
    return arr->elements[arr->size - 1];
}

static inline bool vyne_array_delete(VyneValue arr_val, VyneValue target) {
    if (arr_val.type != V_ARRAY) return false;
    VyneArray* arr = arr_val.as.arr;
    for (int i = 0; i < arr->size; i++) {
        if (vyne_values_equal(arr->elements[i], target)) {
            for (int j = i + 1; j < arr->size; j++) arr->elements[j - 1] = arr->elements[j];
            arr->size--;
            return true;
        }
    }
    return false;
}

static inline VyneValue vyne_array_delete_at(VyneValue arr_val, int64_t idx) {
    if (arr_val.type != V_ARRAY) return vyne_null();
    VyneArray* arr = arr_val.as.arr;
    if (idx < 0 || idx >= arr->size) return vyne_null();
    VyneValue removed = arr->elements[idx];
    for (int i = (int)idx + 1; i < arr->size; i++) arr->elements[i - 1] = arr->elements[i];
    arr->size--;
    return removed;
}

static inline int _vyne_cmp_values(const void* a, const void* b) {
    VyneValue va = *(const VyneValue*)a;
    VyneValue vb = *(const VyneValue*)b;
    if ((va.type == V_INT64 || va.type == V_FLOAT64) &&
        (vb.type == V_INT64 || vb.type == V_FLOAT64)) {
        double da = (va.type == V_FLOAT64) ? va.as.f64 : (double)va.as.i64;
        double db = (vb.type == V_FLOAT64) ? vb.as.f64 : (double)vb.as.i64;
        if (da < db) return -1;
        if (da > db) return  1;
        return 0;
    }
    return 0;
}

static inline void vyne_array_sort(VyneValue arr_val) {
    if (arr_val.type != V_ARRAY) return;
    VyneArray* arr = arr_val.as.arr;
    if (arr->size <= 1) return;
    qsort(arr->elements, arr->size, sizeof(VyneValue), _vyne_cmp_values);
}

static inline void vyne_array_clear(VyneValue arr_val) {
    if (arr_val.type != V_ARRAY) return;
    arr_val.as.arr->size = 0;
}

static inline void vyne_array_place_all(VyneValue arr_val, VyneValue val, int64_t count) {
    if (arr_val.type != V_ARRAY || count < 0) return;
    VyneArray* arr = arr_val.as.arr;
    if (count > arr->capacity) {
        int new_cap = count < 4 ? 4 : (int)count;
        VyneValue* new_elems = (VyneValue*)arena_alloc(sizeof(VyneValue) * new_cap);
        arr->elements = new_elems;
        arr->capacity = new_cap;
    }
    for (int64_t i = 0; i < count; i++) arr->elements[i] = val;
    arr->size = (int)count;
}

// ============================================================================
// SLICE — base[lo..hi], inclusive on both ends
// ============================================================================

static inline VyneValue vyne_slice_get(VyneValue base, VyneValue lo_v, VyneValue hi_v) {
    bool lo_open = (lo_v.type == V_NULL);
    bool hi_open = (hi_v.type == V_NULL);

    int64_t lo = lo_open ? 0
                         : ((lo_v.type == V_INT64) ? lo_v.as.i64 : (int64_t)lo_v.as.f64);
    // hi defaults to a value that will always clamp to len-1 below.
    int64_t hi = hi_open ? INT64_MAX
                         : ((hi_v.type == V_INT64) ? hi_v.as.i64 : (int64_t)hi_v.as.f64);

    if (base.type == V_ARRAY) {
        VyneArray* a = base.as.arr;
        if (lo < 0) lo = 0;
        if (hi >= a->size) hi = a->size - 1;
        if (lo > hi) return vyne_array_create(0);
        int n = (int)(hi - lo + 1);
        VyneValue r = vyne_array_create(n);
        memcpy(r.as.arr->elements, a->elements + lo, sizeof(VyneValue) * n);
        return r;
    }
    if (base.type == V_STRING) {
        const char* s = base.as.str;
        int64_t len = (int64_t)strlen(s);
        if (lo < 0) lo = 0;
        if (hi >= len) hi = len - 1;
        if (lo > hi) return vyne_string("");
        int64_t n = hi - lo + 1;
        char* buf = (char*)arena_alloc((size_t)n + 1);
        memcpy(buf, s + lo, (size_t)n);
        buf[n] = '\0';
        return vyne_string(buf);
    }
    return vyne_null();
}

// Polymorphic dispatch for methods that exist on both arrays and maps
static inline VyneValue vyne_delete_any(VyneValue recv, VyneValue val) {
    if (recv.type == V_ARRAY) {
        return vyne_bool(vyne_array_delete(recv, val));
    }
    if (recv.type == V_MAP && val.type == V_STRING) {
        vyne_map_delete(recv, val);
        return vyne_bool(true);
    }
    return vyne_bool(false);
}

static inline void vyne_clear_any(VyneValue recv) {
    if (recv.type == V_ARRAY)      vyne_array_clear(recv);
    else if (recv.type == V_MAP)   vyne_map_clear(recv);
}

// ============================================================================
// RANGE
// ============================================================================

static inline VyneValue vyne_range_create(VyneValue start_val, VyneValue end_val) {
    if ((start_val.type == V_INT64 || start_val.type == V_FLOAT64) &&
        (end_val.type == V_INT64 || end_val.type == V_FLOAT64)) {
        if (start_val.type == V_INT64 && end_val.type == V_INT64) {
            int64_t start = start_val.as.i64;
            int64_t end = end_val.as.i64;
            if (start <= end) {
                VyneValue res = vyne_array_create(0);
                for (int64_t i = start; i <= end; ++i)
                    vyne_array_push(res, vyne_int(i));
                return res;
            }
        } else {
            double start = (start_val.type == V_FLOAT64) ? start_val.as.f64 : (double)start_val.as.i64;
            double end = (end_val.type == V_FLOAT64) ? end_val.as.f64 : (double)end_val.as.i64;
            if (start <= end) {
                VyneValue res = vyne_array_create(0);
                for (double i = start; i <= end; ++i)
                    vyne_array_push(res, vyne_float(i));
                return res;
            }
        }
    }
    return vyne_array_create(0);
}

// ============================================================================
// STRUCT OPERATIONS
// ============================================================================

static inline VyneValue vyne_struct_get(VyneValue s_val, uint32_t field_id) {
    if (s_val.type != V_STRUCT) return vyne_null();
    VyneStruct* s = s_val.as.strct;
    for (int i = 0; i < s->field_count; i++) {
        if (s->fields[i].id == field_id) return s->fields[i].value;
    }
    return vyne_null();
}

static inline void vyne_struct_set(VyneValue s_val, uint32_t field_id,
                                   const char* field_name, VyneValue val) {
    if (s_val.type != V_STRUCT) return;
    VyneStruct* s = s_val.as.strct;

    for (int i = 0; i < s->field_count; i++) {
        if (s->fields[i].id == field_id) {
            s->fields[i].value = val;
            return;
        }
    }

    VyneField* new_fields = (VyneField*)arena_alloc(sizeof(VyneField) * (s->field_count + 1));
    if (s->fields && s->field_count > 0) {
        memcpy(new_fields, s->fields, sizeof(VyneField) * s->field_count);
    }
    new_fields[s->field_count].id = field_id;
    new_fields[s->field_count].name = field_name;
    new_fields[s->field_count].value = val;
    s->fields = new_fields;
    s->field_count++;
}

static inline void vyne_register_method(const char* type, const char* method, VyneMethodFn fn) {
    if (g_method_count < VYNE_MAX_METHODS) {
        g_method_table[g_method_count].type_name = type;
        g_method_table[g_method_count].method_name = method;
        g_method_table[g_method_count].fn = fn;
        g_method_count++;
    }
}

static inline VyneValue vyne_struct_call(VyneValue self, const char* method, int argc, VyneValue* args) {
    if (self.type != V_STRUCT) return vyne_null();
    const char* type_name = self.as.strct->type_name;
    for (int i = 0; i < g_method_count; i++) {
        if (strcmp(g_method_table[i].type_name, type_name) == 0 &&
            strcmp(g_method_table[i].method_name, method) == 0) {
            return g_method_table[i].fn(argc, args);
        }
    }
    return vyne_null();
}

// ============================================================================
// VALUE UTILITIES
// ============================================================================

static inline bool vyne_is_truthy(VyneValue v) {
    if (v.type == V_NULL) return false;
    if (v.type == V_BOOL || v.type == V_INT64) return v.as.i64 != 0;
    if (v.type == V_FLOAT64) return v.as.f64 != 0.0;
    return true;
}

static inline void _vyne_print_internal(VyneValue v) {
    switch (v.type) {
        case V_INT64:   printf("%lld", v.as.i64); break;
        case V_BOOL:    printf("%s", v.as.i64 ? "true" : "false"); break;
        case V_NULL:    printf("null"); break;
        case V_STRING:  printf("%s", v.as.str ? v.as.str : "null"); break;
        case V_FLOAT64: {
            char buf[64];
            sprintf(buf, "%g", v.as.f64);
            if (strchr(buf, '.') == NULL && strchr(buf, 'e') == NULL)
                strcat(buf, ".0");
            printf("%s", buf);
            break;
        }
        case V_ARRAY: {
            printf("[");
            VyneArray* arr = v.as.arr;
            for (int i = 0; i < arr->size; i++) {
                _vyne_print_internal(arr->elements[i]);
                if (i < arr->size - 1) printf(", ");
            }
            printf("]");
            break;
        }
        case V_MAP: {
            VyneMap* m = v.as.map;
            printf("{");
            for (int i = 0; i < m->size; i++) {
                printf("\"%s\": ", m->entries[i].key);
                _vyne_print_internal(m->entries[i].value);
                if (i < m->size - 1) printf(", ");
            }
            printf("}");
            break;
        }
        case V_STRUCT: {
            VyneStruct* s = v.as.strct;
            printf("%s { ", s->type_name);
            for (int i = 0; i < s->field_count; i++) {
                printf("%s: ", s->fields[i].name);
                _vyne_print_internal(s->fields[i].value);
                if (i < s->field_count - 1) printf(", ");
            }
            printf(" }");
            break;
        }
        default: printf("<unknown>");
    }
}

static inline void vyne_out(VyneValue v) {
    _vyne_print_internal(v);
    printf("\n");
    fflush(stdout);
}

// ============================================================================
// TYPE CONVERSIONS
// ============================================================================

static inline VyneValue vyne_to_int(VyneValue v) {
    if (v.type == V_INT64) return v;
    if (v.type == V_FLOAT64) return vyne_int((int64_t)v.as.f64);
    if (v.type == V_STRING) return vyne_int(atoll(v.as.str));
    if (v.type == V_BOOL) return vyne_int(v.as.i64);
    return vyne_int(0);
}

static inline VyneValue vyne_to_float(VyneValue v) {
    if (v.type == V_FLOAT64) return v;
    if (v.type == V_INT64) return vyne_float((double)v.as.i64);
    if (v.type == V_STRING) return vyne_float(atof(v.as.str));
    return vyne_float(0.0);
}

static inline const char* vyne_get_type_name(VyneValue v) {
    switch (v.type) {
        case V_INT64:   return "Int64";
        case V_FLOAT64: return "Float64";
        case V_STRING:  return "String";
        case V_BOOL:    return "Boolean";
        case V_ARRAY:   return "Array";
        case V_MAP:     return "Map";
        case V_STRUCT:  return "Struct";
        case V_NULL:    return "Null";
        default:        return "Unknown";
    }
}

static inline int64_t vyne_get_sizeof(VyneValue v) {
    if (v.type == V_ARRAY) return (int64_t)v.as.arr->size;
    if (v.type == V_MAP)   return (int64_t)v.as.map->size;
    if (v.type == V_STRING) return (int64_t)strlen(v.as.str);
    if (v.type == V_STRUCT) return (int64_t)v.as.strct->field_count;
    return 8;
}

static inline void _vyne_format_float(char* buf, double val) {
    sprintf(buf, "%g", val);
    if (strchr(buf, '.') == NULL && strchr(buf, 'e') == NULL)
        strcat(buf, ".0");
}

static inline VyneValue vyne_to_string(VyneValue v) {
    char buf[256];
    switch (v.type) {
        case V_INT64:   sprintf(buf, "%lld", v.as.i64); break;
        case V_FLOAT64: _vyne_format_float(buf, v.as.f64); break;
        case V_BOOL:    strcpy(buf, v.as.i64 ? "true" : "false"); break;
        case V_NULL:    strcpy(buf, "null"); break;
        case V_STRING:  return v;
        case V_ARRAY: {
            VyneArray* arr = v.as.arr;
            size_t total = 2; // '[' + ']'
            for (int i = 0; i < arr->size; i++) {
                VyneValue elem = vyne_to_string(arr->elements[i]);
                total += strlen(elem.as.str);
                if (i > 0) total += 2; // ", "
            }
            char* tmp = (char*)arena_alloc(total + 1);
            size_t pos = 0;
            tmp[pos++] = '[';
            for (int i = 0; i < arr->size; i++) {
                if (i > 0) { tmp[pos++] = ','; tmp[pos++] = ' '; }
                VyneValue elem = vyne_to_string(arr->elements[i]);
                size_t slen = strlen(elem.as.str);
                memcpy(tmp + pos, elem.as.str, slen);
                pos += slen;
            }
            tmp[pos++] = ']';
            tmp[pos]   = '\0';
            return vyne_string(tmp);
        }
        case V_MAP: {
            VyneMap* m = v.as.map;
            size_t total = 3;
            for (int i = 0; i < m->size; i++) {
                total += strlen(m->entries[i].key) + 6;
                VyneValue vs = vyne_to_string(m->entries[i].value);
                total += strlen(vs.as.str) + 2;
            }
            char* tmp = (char*)arena_alloc(total);
            size_t pos = 0;
            tmp[pos++] = '{';
            for (int i = 0; i < m->size; i++) {
                if (i > 0) { tmp[pos++] = ','; tmp[pos++] = ' '; }
                tmp[pos++] = '"';
                size_t kl = strlen(m->entries[i].key);
                memcpy(tmp + pos, m->entries[i].key, kl);
                pos += kl;
                tmp[pos++] = '"';
                tmp[pos++] = ':';
                tmp[pos++] = ' ';
                VyneValue vs = vyne_to_string(m->entries[i].value);
                size_t vl = strlen(vs.as.str);
                memcpy(tmp + pos, vs.as.str, vl);
                pos += vl;
            }
            tmp[pos++] = '}';
            tmp[pos] = '\0';
            return vyne_string(tmp);
        }
        case V_STRUCT: {
            VyneStruct* s = v.as.strct;
            size_t total = strlen(s->type_name) + 4;
            for (int i = 0; i < s->field_count; i++) {
                total += strlen(s->fields[i].name) + 2;
                VyneValue fv = vyne_to_string(s->fields[i].value);
                total += strlen(fv.as.str) + 2;
            }
            char* tmp = (char*)arena_alloc(total);
            size_t pos = 0;
            memcpy(tmp + pos, s->type_name, strlen(s->type_name));
            pos += strlen(s->type_name);
            tmp[pos++]=' '; tmp[pos++]='{'; tmp[pos++]=' ';
            for (int i = 0; i < s->field_count; i++) {
                if (i) { tmp[pos++]=','; tmp[pos++]=' '; }
                size_t kl = strlen(s->fields[i].name);
                memcpy(tmp + pos, s->fields[i].name, kl); pos += kl;
                tmp[pos++]=':'; tmp[pos++]=' ';
                VyneValue fv = vyne_to_string(s->fields[i].value);
                size_t vl = strlen(fv.as.str);
                memcpy(tmp + pos, fv.as.str, vl); pos += vl;
            }
            tmp[pos++]=' '; tmp[pos++]='}'; tmp[pos]='\0';
            return vyne_string(tmp);
        }
        default: strcpy(buf, "[object]"); break;
    }
    return vyne_string(buf);
}

static inline bool vyne_values_equal(VyneValue a, VyneValue b) {
    if (a.type != b.type) {
        if ((a.type == V_INT64 || a.type == V_FLOAT64) &&
            (b.type == V_INT64 || b.type == V_FLOAT64)) {
            double av = (a.type == V_FLOAT64) ? a.as.f64 : (double)a.as.i64;
            double bv = (b.type == V_FLOAT64) ? b.as.f64 : (double)b.as.i64;
            return av == bv;
        }
        return false;
    }
    switch (a.type) {
        case V_NULL:    return true;
        case V_BOOL:
        case V_INT64:   return a.as.i64 == b.as.i64;
        case V_FLOAT64: return a.as.f64 == b.as.f64;
        case V_STRING:  return strcmp(a.as.str, b.as.str) == 0;
        case V_MAP: {
            VyneMap* am = a.as.map;
            VyneMap* bm = b.as.map;
            if (am == bm) return true;
            if (am->size != bm->size) return false;
            for (int i = 0; i < am->size; i++) {
                int idx = _vyne_map_find(bm, am->entries[i].key);
                if (idx < 0) return false;
                if (!vyne_values_equal(am->entries[i].value, bm->entries[idx].value)) return false;
            }
            return true;
        }
        default:        return false;
    }
}

// ============================================================================
// BINARY OPERATORS
// ============================================================================

enum {
    VBOP_ADD = 29, VBOP_SUB = 30, VBOP_MUL = 31, VBOP_DIV = 32,
    VBOP_MOD = 36, VBOP_POW = 37,
    VBOP_EQ = 43, VBOP_NEQ = 44,
    VBOP_GT = 45, VBOP_LT = 46, VBOP_GTE = 47, VBOP_LTE = 48,
    VBOP_AND = 49, VBOP_OR = 50,
    VBOP_FLOOR_DIV = 51
};

__declspec(noinline)
static VyneValue vyne_binop_slow(VyneValue left, VyneValue right, int op) {
    // String concatenation
    if (op == VBOP_ADD && (left.type == V_STRING || right.type == V_STRING)) {
        VyneValue ls = vyne_to_string(left);
        VyneValue rs = vyne_to_string(right);
        size_t llen = strlen(ls.as.str);
        size_t rlen = strlen(rs.as.str);
        char* res = (char*)arena_alloc(llen + rlen + 1);
        memcpy(res, ls.as.str, llen);
        memcpy(res + llen, rs.as.str, rlen);
        res[llen + rlen] = '\0';
        return vyne_string_own(res);
    }

    if (op == VBOP_ADD && left.type == V_ARRAY && right.type == V_ARRAY) {
        VyneArray* la = left.as.arr;
        VyneArray* ra = right.as.arr;
        VyneValue result = vyne_array_create(0);
        for (int i = 0; i < la->size; i++) vyne_array_push(result, la->elements[i]);
        for (int i = 0; i < ra->size; i++) vyne_array_push(result, ra->elements[i]);
        return result;
    }

    if (op == VBOP_AND) return vyne_bool(vyne_is_truthy(left) && vyne_is_truthy(right));
    if (op == VBOP_OR)  return vyne_bool(vyne_is_truthy(left) || vyne_is_truthy(right));

    // Float math
    bool is_float_math = (left.type == V_FLOAT64 || right.type == V_FLOAT64);
    if (is_float_math &&
        (left.type == V_FLOAT64 || left.type == V_INT64) &&
        (right.type == V_FLOAT64 || right.type == V_INT64)) {
        double l = (left.type == V_FLOAT64) ? left.as.f64 : (double)left.as.i64;
        double r = (right.type == V_FLOAT64) ? right.as.f64 : (double)right.as.i64;
        switch (op) {
            case VBOP_ADD: return vyne_float(l + r);
            case VBOP_SUB: return vyne_float(l - r);
            case VBOP_MUL: return vyne_float(l * r);
            case VBOP_DIV:
                if (r == 0.0) { fprintf(stderr, "Runtime error: Division by zero!\n"); exit(1); }
                return vyne_float(l / r);
            case VBOP_MOD:
                if (r == 0.0) { fprintf(stderr, "Runtime error: Modulo by zero!\n"); exit(1); }
                return vyne_float(fmod(l, r));
            case VBOP_POW: return vyne_float(pow(l, r));
            case VBOP_EQ:  return vyne_bool(l == r);
            case VBOP_NEQ: return vyne_bool(l != r);
            case VBOP_GT:  return vyne_bool(l > r);
            case VBOP_LT:  return vyne_bool(l < r);
            case VBOP_GTE: return vyne_bool(l >= r);
            case VBOP_LTE: return vyne_bool(l <= r);
        }
        return vyne_null();
    }

    if (op == VBOP_EQ)  return vyne_bool(vyne_values_equal(left, right));
    if (op == VBOP_NEQ) return vyne_bool(!vyne_values_equal(left, right));

    fprintf(stderr, "Runtime error: Invalid operation between %s and %s\n",
            vyne_get_type_name(left), vyne_get_type_name(right));
    exit(1);
}

static inline VyneValue vyne_binop(VyneValue left, VyneValue right, int op) {
    if (__builtin_expect(left.type == V_INT64 && right.type == V_INT64, 1)) {
        int64_t l = left.as.i64;
        int64_t r = right.as.i64;
        switch (op) {
            case VBOP_ADD: return vyne_int(l + r);
            case VBOP_SUB: return vyne_int(l - r);
            case VBOP_MUL: return vyne_int(l * r);
            case VBOP_DIV:
                if (r == 0) { fprintf(stderr, "Runtime error: Division by zero!\n"); exit(1); }
                return vyne_int(l / r);
            case VBOP_FLOOR_DIV:
                if (r == 0) { fprintf(stderr, "Runtime error: Division by zero!\n"); exit(1); }
                return vyne_int(l / r);
            case VBOP_MOD:
                if (r == 0) { fprintf(stderr, "Runtime error: Modulo by zero!\n"); exit(1); }
                return vyne_int(l % r);
            case VBOP_POW: return vyne_float(pow((double)l, (double)r));
            case VBOP_EQ:  return vyne_bool(l == r);
            case VBOP_NEQ: return vyne_bool(l != r);
            case VBOP_GT:  return vyne_bool(l > r);
            case VBOP_LT:  return vyne_bool(l < r);
            case VBOP_GTE: return vyne_bool(l >= r);
            case VBOP_LTE: return vyne_bool(l <= r);
            case VBOP_AND: return vyne_bool((l != 0) && (r != 0));
            case VBOP_OR:  return vyne_bool((l != 0) || (r != 0));
            // Any other op with two int64 operands: not a valid combination.
            default: break;
        }
    }

    return vyne_binop_slow(left, right, op);
}

// ============================================================================
// IN OPERATOR
// ============================================================================

static inline VyneValue vyne_in_operator(VyneValue left, VyneValue right, int isNot) {
    bool result = false;

    if (right.type == V_ARRAY) {
        VyneArray* arr = right.as.arr;
        for (int i = 0; i < arr->size; i++) {
            if (vyne_values_equal(left, arr->elements[i])) {
                result = true;
                break;
            }
        }
    } else if (right.type == V_STRUCT) {
        VyneStruct* s = right.as.strct;
        if (left.type == V_STRING) {
            for (int i = 0; i < s->field_count; i++) {
                if (strcmp(s->fields[i].name, left.as.str) == 0) {
                    result = true;
                    break;
                }
            }
        } else {
            fprintf(stderr, "Runtime Error: Map keys must be strings\n");
            return vyne_bool(0);
        }
    } else if (right.type == V_STRING) {
        if (left.type != V_STRING) {
            fprintf(stderr, "Runtime Error: String membership requires string left operand\n");
            return vyne_bool(0);
        }
        result = strstr(right.as.str, left.as.str) != NULL;
    } else if (right.type == V_MAP) {
        if (left.type != V_STRING) {
            fprintf(stderr, "Runtime Error: Map keys must be strings\n");
            return vyne_bool(0);
        }
        result = vyne_map_has(right, left);
    } else {
        fprintf(stderr, "Runtime Error: 'in' operator requires array, map, or string on right side\n");
        return vyne_bool(0);
    }

    if (isNot) result = !result;
    return vyne_bool(result);
}

// ============================================================================
// UNARY OPERATORS
// ============================================================================

static inline VyneValue vyne_unary(VyneValue val, int op) {
    switch (op) {
        case 44: // '!' 
            return vyne_bool(!vyne_is_truthy(val));
        case 30: // '-'
            if (val.type == V_INT64) return vyne_int(-val.as.i64);
            if (val.type == V_FLOAT64) return vyne_float(-val.as.f64);
            return vyne_null();
        default:
            return val;
    }
}

// ============================================================================
// MODULE DEPLOYMENT
// ============================================================================

static inline void vyne_deploy_module(const char* name) {
    // Module deployment hook
    // Called from transpiled code when 'deploy module;' is encountered
}

// ============================================================================
// INTERPOLATED STRING
// ============================================================================

static inline VyneValue vyne_interpolate(const char* parts[], int part_count, VyneValue* values) {
    size_t total_len = 1;
    for (int i = 0; i < part_count; i++) {
        if (i % 2 == 0) {
            total_len += strlen(parts[i]);
        } else {
            VyneValue str = vyne_to_string(values[i / 2]);
            total_len += strlen(str.as.str);
        }
    }

    char* result = (char*)arena_alloc(total_len);
    size_t pos = 0;
    for (int i = 0; i < part_count; i++) {
        if (i % 2 == 0) {
            const char* s = parts[i];
            size_t len = strlen(s);
            memcpy(result + pos, s, len);
            pos += len;
        } else {
            VyneValue str = vyne_to_string(values[i / 2]);
            size_t len = strlen(str.as.str);
            memcpy(result + pos, str.as.str, len);
            pos += len;
        }
    }
    result[pos] = '\0';
    return vyne_string(result);
}

// ============================================================================
// EXCEPTION HANDLING (try / catch / finally / throw)
// ----------------------------------------------------------------------------
// A small stack of jmp_buf frames. Each `try` block pushes one; a `throw`
// pops the innermost frame and longjmp's into its setjmp point.
// ============================================================================

#include <setjmp.h>

#define VYNE_MAX_EXC_FRAMES 64

typedef struct {
    jmp_buf buf;
} VyneExcFrame;

static VyneExcFrame g_exc_stack[VYNE_MAX_EXC_FRAMES];
static int g_exc_top = 0;
static VyneValue g_exc_value;

static inline int vyne_try_push(void) {
    if (g_exc_top >= VYNE_MAX_EXC_FRAMES) {
        fprintf(stderr, "Runtime error: try/catch nested too deep (>%d)\n",
                VYNE_MAX_EXC_FRAMES);
        exit(1);
    }
    return g_exc_top++;
}

static inline void vyne_try_pop(void) {
    if (g_exc_top > 0) g_exc_top--;
}

static inline void vyne_throw(VyneValue val) {
    if (g_exc_top == 0) {
        fprintf(stderr, "Uncaught exception: ");
        _vyne_print_internal(val);
        fprintf(stderr, "\n");
        exit(1);
    }
    g_exc_value = val;
    g_exc_top--;
    longjmp(g_exc_stack[g_exc_top].buf, 1);
}
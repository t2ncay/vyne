#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define VYNE_ARENA_BLOCK_SIZE (8 * 1024 * 1024)
#define VYNE_MAX_METHODS 256

typedef struct ArenaBlock {
    uint8_t *    data;
    size_t       used;
    size_t       capacity;
    struct ArenaBlock* next;
} ArenaBlock;

typedef struct {
    ArenaBlock*     head;
    size_t          total_allocated;      
} VyneArena;

static VyneArena g_arena = {NULL, 0};

static inline ArenaBlock* arena_new_block(size_t min_size) {
    size_t cap = min_size > VYNE_ARENA_BLOCK_SIZE ? min_size : VYNE_ARENA_BLOCK_SIZE;
    ArenaBlock* block = (ArenaBlock*)malloc(sizeof(ArenaBlock));
    block->data     = (uint8_t*)malloc(cap);
    block->used     = 0;
    block->capacity = cap;
    block->next     = NULL;
    return block;
}

static inline void* arena_alloc(size_t size) {
    size = (size + 7) & ~(size_t)7;

    if (!g_arena.head || g_arena.head->used + size > g_arena.head->capacity) {
        ArenaBlock* block = arena_new_block(size);
        block->next  = g_arena.head;
        g_arena.head = block;
    }

    void* ptr = g_arena.head->data + g_arena.head->used;
    g_arena.head->used      += size;
    g_arena.total_allocated += size;
    return ptr;
}

static inline void arena_free_all(void) {
    size_t total = g_arena.total_allocated;
    int blocks = 0;
    ArenaBlock* block = g_arena.head;
    while (block) {
        ArenaBlock* next = block->next;
        blocks++;
        free(block->data);
        free(block);
        block = next;
    }
    g_arena.head = NULL;
    g_arena.total_allocated = 0;
}

typedef struct VyneValue VyneValue;
struct VyneArray;
struct VyneStruct;

typedef VyneValue (*VyneMethodFn)(int argc, VyneValue* args);

typedef enum {
    V_NULL, V_FLOAT64, V_INT64, V_STRING, V_ARRAY, V_BOOL, V_STRUCT
} VyneType;

struct VyneValue {
    VyneType type;
    union {
        double f64;
        int64_t i64;
        char* str;
        struct VyneArray* arr;
        struct VyneStruct* strct;
    } as;
};

typedef struct VyneArray {
    VyneValue* elements;
    int size;
    int capacity;
} VyneArray;

typedef struct VyneField {
    uint32_t id;
    const char* name;
    VyneValue value;
} VyneField;

typedef struct VyneStruct {
    const char* type_name;
    VyneField* fields;
    int field_count;
} VyneStruct;

typedef struct {
    const char* type_name;
    const char* method_name;
    VyneMethodFn fn;
} VyneMethodEntry;

static VyneMethodEntry g_method_table[VYNE_MAX_METHODS];
static int g_method_count = 0;

static inline VyneValue vyne_binop(VyneValue left, VyneValue right, int op);
static inline bool vyne_is_truthy(VyneValue v);
static inline VyneValue vyne_int(int64_t v);
static inline VyneValue vyne_bool(bool v);
static inline VyneValue vyne_null();
static inline VyneValue vyne_string(const   char* s);

static inline VyneValue vyne_null() {
    VyneValue val;
    val.type = V_NULL;
    val.as.i64 = 0;
    return val;
}

static inline VyneValue vyne_int(int64_t v) {
    VyneValue val;
    val.type = V_INT64;
    val.as.i64 = v;
    return val;
}

static inline VyneValue vyne_bool(bool v) {
    VyneValue val;
    val.type = V_BOOL;
    val.as.i64 = v ? 1 : 0;
    return val;
}

static inline VyneValue vyne_float(double v) {
    VyneValue val;
    val.type = V_FLOAT64;
    val.as.f64 = v;
    return val;
}

static inline VyneValue vyne_string(const char* s) {
    VyneValue val;
    val.type = V_STRING;
    size_t len = strlen(s) + 1;
    char* copy = (char*)arena_alloc(len);
    memcpy(copy, s, len);
    val.as.str = copy;
    return val;
}

static inline VyneValue vyne_array_create(int initial_size) {
    VyneValue val;
    val.type = V_ARRAY;

    VyneArray* arr  = (VyneArray*)arena_alloc(sizeof(VyneArray));
    int cap         = initial_size > 0 ? initial_size : 4;
    arr->elements   = (VyneValue*)arena_alloc(sizeof(VyneValue) * cap);
    arr->size       = initial_size;
    arr->capacity   = cap;

    for (int i = 0; i < initial_size; i++)
        arr->elements[i] = vyne_null();

    val.as.arr = arr;
    return val;
}

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

static inline VyneValue vyne_array_pop(VyneValue arr_val) {
    VyneArray* arr = arr_val.as.arr;
    if (arr->size == 0) return vyne_null();
    return arr->elements[--arr->size];
}

static inline VyneValue vyne_array_reverse(VyneValue arr_val) {
    if (arr_val.type != V_ARRAY) return vyne_null();
    
    VyneArray* arr = arr_val.as.arr;
    if (arr->size <= 1) return arr_val;

    int start = 0;
    int end = arr->size - 1;
    VyneValue temp;

    while (start < end) {
        temp = arr->elements[start];
        arr->elements[start] = arr->elements[end];
        arr->elements[end] = temp;
        
        start++;
        end--;
    }

    return arr_val;
}

static inline bool vyne_array_contains(VyneValue arr_val, VyneValue target) {
    VyneArray* arr = arr_val.as.arr;
    for (int i = 0; i < arr->size; i++) {
        if (vyne_binop(arr->elements[i], target, 45).as.i64) return true;
    }
    return false;
};

// interfaces

static inline VyneValue vyne_struct_get(VyneValue s_val, uint32_t field_id) {
    if (s_val.type != V_STRUCT) return vyne_null();
    VyneStruct* s = s_val.as.strct;
    for (int i = 0; i < s->field_count; i++) {
        if (s->fields[i].id == field_id) return s->fields[i].value;
    }
    return vyne_null();
}

static inline void vyne_struct_set(VyneValue s_val, uint32_t field_id, VyneValue val) {
    if (s_val.type != V_STRUCT) return;
    VyneStruct* s = s_val.as.strct;
    for (int i = 0; i < s->field_count; i++) {
        if (s->fields[i].id == field_id) {
            s->fields[i].value = val;
            return;
        }
    }
}

static inline void vyne_register_method(const char* type, const char* method, VyneMethodFn fn) {
    if (g_method_count < VYNE_MAX_METHODS) {
        VyneMethodEntry entry;
        entry.type_name = type;
        entry.method_name = method;
        entry.fn = fn;
        g_method_table[g_method_count++] = entry; 
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

static inline bool vyne_is_truthy(VyneValue v) {
    if (v.type == V_NULL) return false;
    if (v.type == V_BOOL || v.type == V_INT64) return v.as.i64 != 0;
    if (v.type == V_FLOAT64) return v.as.f64 != 0.0;
    return true;
};

// built-in calls
static inline void _vyne_print_internal(VyneValue v) {
    switch(v.type) {
        case V_INT64:   printf("%lld", v.as.i64); break;
        case V_BOOL:    printf("%s", v.as.i64 ? "true" : "false"); break;
        case V_NULL:    printf("null"); break;
        case V_STRING:  printf("%s", v.as.str); break;
        
        case V_FLOAT64: {
            char buf[64];
            sprintf(buf, "%g", v.as.f64);
            if (strchr(buf, '.') == NULL && strchr(buf, 'e') == NULL) {
                strcat(buf, ".0");
            }
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
    switch(v.type) {
        case V_INT64:   return "Int64";
        case V_FLOAT64: return "Float64";
        case V_STRING:  return "String";
        case V_BOOL:    return "Boolean";
        case V_ARRAY:   return "Array";
        case V_STRUCT:  return "Struct";
        default:        return "Null";
    }
}

static inline int64_t vyne_get_sizeof(VyneValue v) {
    if (v.type == V_ARRAY) return (int64_t)v.as.arr->size;
    if (v.type == V_STRING) return (int64_t)strlen(v.as.str);
    if (v.type == V_STRUCT) return (int64_t)v.as.strct->field_count;
    return (int64_t)sizeof(VyneValue);
}

static inline void _vyne_format_float(char* buf, double val) {
    sprintf(buf, "%g", val);
    if (strchr(buf, '.') == NULL && strchr(buf, 'e') == NULL) {
        strcat(buf, ".0");
    }
}

static inline VyneValue vyne_to_string(VyneValue v) {
    char buf[256];
    switch(v.type) {
        case V_INT64:   sprintf(buf, "%lld", v.as.i64); break;
        case V_FLOAT64: _vyne_format_float(buf, v.as.f64); break;
        case V_BOOL:    strcpy(buf, v.as.i64 ? "true" : "false"); break;
        case V_NULL:    strcpy(buf, "null"); break;
        case V_STRING:  return v;
        case V_ARRAY: {
            VyneArray* arr = v.as.arr;
            size_t cap = 64;
            char* tmp = (char*)malloc(cap);
            size_t pos = 0;
            tmp[pos++] = '[';
            for (int i = 0; i < arr->size; i++) {
                if (i > 0) {
                    if (pos + 2 >= cap) { cap *= 2; tmp = (char*)realloc(tmp, cap); }
                    tmp[pos++] = ','; tmp[pos++] = ' ';
                }
                VyneValue elem = vyne_to_string(arr->elements[i]);  // recursive
                const char* s = (elem.type == V_STRING) ? elem.as.str : "null";
                size_t slen = strlen(s);
                while (pos + slen + 2 >= cap) { cap *= 2; tmp = (char*)realloc(tmp, cap); }
                memcpy(tmp + pos, s, slen);
                pos += slen;
            }
            tmp[pos++] = ']';
            tmp[pos]   = '\0';
            VyneValue result = vyne_string(tmp);
            free(tmp);
            return result;
        }
        default: strcpy(buf, "[object]"); break;
    }
    return vyne_string(buf);
}

static inline VyneValue vyne_binop(VyneValue left, VyneValue right, int op) {
    if (op == 28 && (left.type == V_STRING || right.type == V_STRING)) {
        char buf_l[512], buf_r[512];
        
        if (left.type == V_STRING) strcpy(buf_l, left.as.str);
        else if (left.type == V_INT64) sprintf(buf_l, "%lld", left.as.i64);
        else if (left.type == V_FLOAT64) sprintf(buf_l, "%g", left.as.f64);
        else strcpy(buf_l, "null");

        if (right.type == V_STRING) strcpy(buf_r, right.as.str);
        else if (right.type == V_INT64) sprintf(buf_r, "%lld", right.as.i64);
        else if (right.type == V_FLOAT64) sprintf(buf_r, "%g", right.as.f64);
        else strcpy(buf_r, "null");

        char* res = (char*)malloc(strlen(buf_l) + strlen(buf_r) + 1);
        strcpy(res, buf_l);
        strcat(res, buf_r);
        VyneValue v = vyne_string(res);
        free(res);
        return v;
    }
    
    if (left.type == V_INT64 && right.type == V_INT64) {
        switch(op) {
            case 28: return vyne_int(left.as.i64 + right.as.i64);
            case 29: return vyne_int(left.as.i64 - right.as.i64);
            case 30: return vyne_int(left.as.i64 * right.as.i64);
            case 32: return (right.as.i64 == 0) ? vyne_null() : vyne_float((double)left.as.i64 / right.as.i64);
            case 42: return vyne_bool(left.as.i64 == right.as.i64);
            case 44: return vyne_bool(left.as.i64 > right.as.i64);
            case 45: return vyne_bool(left.as.i64 < right.as.i64);
        }
    }

    if ((left.type == V_FLOAT64 || left.type == V_INT64) && 
        (right.type == V_FLOAT64 || right.type == V_INT64)) {
        double l = (left.type == V_FLOAT64) ? left.as.f64 : (double)left.as.i64;
        double r = (right.type == V_FLOAT64) ? right.as.f64 : (double)right.as.i64;
        switch(op) {
            case 28: return vyne_float(l + r);
            case 29: return vyne_float(l - r);
            case 31: return vyne_float(l * r);
            case 32: return vyne_float(l / r);
            case 45: return vyne_bool(l == r);
            case 47: return vyne_bool(l > r);
            case 48: return vyne_bool(l < r);
        }
    }

    return vyne_null(); 
}
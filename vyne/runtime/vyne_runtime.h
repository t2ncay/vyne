#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    V_NULL, V_FLOAT64, V_INT64, V_STRING, V_ARRAY, V_BOOL
} VyneType;

struct VyneArray;

typedef struct {
    VyneType type;
    union {
        double f64;
        int64_t i64;
        char* str;
        struct VyneArray* arr;
    } as;
} VyneValue;

typedef struct VyneArray {
    VyneValue* elements;
    int size;
    int capacity;
} VyneArray;

// 2. Fundamental funksiyaları (null kimi) ən yuxarıya çəkirik
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

static inline VyneValue vyne_string(char* s) {
    VyneValue val;
    val.type = V_STRING;
    val.as.str = strdup(s);
    return val;
}

static inline VyneValue vyne_array_create(int initial_size) {
    VyneValue val;
    val.type = V_ARRAY;
    VyneArray* arr = (VyneArray*)malloc(sizeof(VyneArray));
    arr->size = initial_size;
    arr->capacity = initial_size > 0 ? initial_size : 4;
    arr->elements = (VyneValue*)malloc(sizeof(VyneValue) * arr->capacity);
    
    for(int i = 0; i < initial_size; i++) {
        arr->elements[i] = vyne_null();
    }
    
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

static inline bool vyne_is_truthy(VyneValue v) {
    if (v.type == V_NULL) return false;
    if (v.type == V_BOOL || v.type == V_INT64) return v.as.i64 != 0;
    if (v.type == V_FLOAT64) return v.as.f64 != 0.0;
    return true;
}

static inline void vyne_out(VyneValue v) {
    switch(v.type) {
        case V_INT64:   printf("%lld\n", v.as.i64); break;
        case V_FLOAT64: printf("%g\n", v.as.f64); break;
        case V_STRING:  printf("%s\n", v.as.str); break;
        case V_BOOL:    printf("%s\n", v.as.i64 ? "true" : "false"); break;
        case V_NULL:    printf("null\n"); break;
        default:        printf("[object]\n");
    }
    fflush(stdout);
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
        free(res); // vyne_string uses strdup
        return v;
    }

    if (left.type == V_INT64 && right.type == V_INT64) {
        switch(op) {
            case 28: return vyne_int(left.as.i64 + right.as.i64);
            case 29: return vyne_int(left.as.i64 - right.as.i64);
            case 31: return vyne_int(left.as.i64 * right.as.i64);
            case 32: return (right.as.i64 == 0) ? vyne_null() : vyne_float((double)left.as.i64 / right.as.i64);
            case 45: return vyne_bool(left.as.i64 == right.as.i64);
            case 47: return vyne_bool(left.as.i64 > right.as.i64);
            case 48: return vyne_bool(left.as.i64 < right.as.i64);
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
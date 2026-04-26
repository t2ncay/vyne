#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

// Vyne Type Enum
typedef enum {
    V_NULL, V_FLOAT64, V_INT64, V_STRING, V_ARRAY, V_BOOL
} VyneType;

typedef struct {
    VyneType type;
    union {
        double f64;
        int64_t i64;
        char* str;
        struct VyneArray* arr;
    } as;
} VyneValue;

static inline VyneValue vyne_int(int64_t v) {
    VyneValue val;
    val.type = V_INT64;
    val.as.i64 = v;
    return val;
}

static inline VyneValue vyne_float(double v) {
    VyneValue val;
    val.type = V_FLOAT64;
    val.as.f64 = v;
    return val;
}

static inline VyneValue vyne_bool(bool v) {
    VyneValue val;
    val.type = V_BOOL;
    val.as.i64 = v ? 1 : 0;
    return val;
}

static inline VyneValue vyne_string(char* s) {
    VyneValue val;
    val.type = V_STRING;
    val.as.str = strdup(s);
    return val;
}

static inline VyneValue vyne_null() {
    VyneValue val;
    val.type = V_NULL;
    val.as.i64 = 0;
    return val;
}

// --- Logic ---

static inline bool vyne_is_truthy(VyneValue v) {
    if (v.type == V_NULL) return false;
    if (v.type == V_BOOL) return v.as.i64 != 0;
    if (v.type == V_INT64) return v.as.i64 != 0;
    if (v.type == V_FLOAT64) return v.as.f64 != 0.0;
    return true; // Strings and Arrays are truthy
}

// --- Built-ins ---

static inline void vyne_out(VyneValue v) {
    switch(v.type) {
        case V_INT64:   printf("%lld\n", v.as.i64); break; // %lld Windows üçün daha etibarlıdır
        case V_FLOAT64: printf("%g\n", v.as.f64); break;
        case V_STRING:  printf("%s\n", v.as.str); break;
        case V_BOOL:    printf("%s\n", v.as.i64 ? "true" : "false"); break;
        case V_NULL:    printf("null\n"); break;
        default:        printf("[object]\n");
    }
    fflush(stdout);
}
static inline VyneValue vyne_binop(VyneValue left, VyneValue right, int op) {
    if (left.type == V_INT64 && right.type == V_INT64) {
        switch(op) {
            case 30: // Add 
                return vyne_int(left.as.i64 + right.as.i64);
            case 31: // Substract
                return vyne_int(left.as.i64 - right.as.i64);
            case 32: // Multiply
                return vyne_int(left.as.i64 * right.as.i64);
            case 33: // Division
                if (right.as.i64 == 0) return vyne_null(); // Sıfıra bölünmə
                return vyne_float((double)left.as.i64 / right.as.i64);
            case 36: // Modulo
                return vyne_int(left.as.i64 % right.as.i64);
            case 45: // Double_Equals (==)
                return vyne_bool(left.as.i64 == right.as.i64);
            case 47: // Greater (>)
                return vyne_bool(left.as.i64 > right.as.i64);
            case 48: // Smaller (<)
                return vyne_bool(left.as.i64 < right.as.i64);
        }
    }
    
    if ((left.type == V_FLOAT64 || left.type == V_INT64) && 
        (right.type == V_FLOAT64 || right.type == V_INT64)) {
        
        double l = (left.type == V_FLOAT64) ? left.as.f64 : (double)left.as.i64;
        double r = (right.type == V_FLOAT64) ? right.as.f64 : (double)right.as.i64;

        switch(op) {
            case 30: return vyne_float(l + r);
            case 31: return vyne_float(l - r);
            case 32: return vyne_float(l * r);
            case 33: return vyne_float(l / r);
            case 45: return vyne_bool(l == r);
            case 47: return vyne_bool(l > r);
            case 48: return vyne_bool(l < r);
        }
    }

    return vyne_null(); 
}
// runtime/esp32/detail/mcu_operators.h
//
// Boxed-value operator support for the ESP32 runtime.
//
// The compiler lowers dynamic arithmetic and comparisons to
// `vyne_binop(left, right, op)` and property/method sizes to
// `vyne_get_sizeof(v)`. Those live in runtime/detail/operators.h and
// conversions.h on the desktop target; the MCU type header
// (mcu_types.h) is intentionally minimal, so this header supplies the
// subset an embedded sketch needs.
//
// `V_FLOAT32` is referenced by the compiler's unbox idiom but is absent
// from the MCU VyneType enum. Float32 is not a first-class MCU value
// type today; aliasing the tag to an unused integer lets the idiom
// compile and keeps the comparison false for values this runtime
// produces.
#pragma once
#include "mcu_types.h"
#include <math.h>
#include <stdio.h>

#ifndef V_FLOAT32
#define V_FLOAT32 13
#endif

// Must match runtime/detail/operators.h and the emitter's opcode values.
enum {
    VBOP_ADD = 29,
    VBOP_SUB = 30,
    VBOP_MUL = 31,
    VBOP_DIV = 32,
    VBOP_MOD = 36,
    VBOP_POW = 37,
    VBOP_EQ = 43,
    VBOP_NEQ = 44,
    VBOP_GT = 45,
    VBOP_LT = 46,
    VBOP_GTE = 47,
    VBOP_LTE = 48,
    VBOP_AND = 49,
    VBOP_OR = 50,
    VBOP_FLOOR_DIV = 51,
    VBOP_BITWISE_NOT = 52,
    VBOP_BITWISE_AND = 53,
    VBOP_BITWISE_OR = 54,
    VBOP_BITWISE_XOR = 55,
    VBOP_BITWISE_SLL = 56,
    VBOP_BITWISE_SRL = 57
};

static inline bool _vyne_is_numeric(VyneValue v) {
    return v.type == V_INT64 || v.type == V_FLOAT64 ||
           v.type == V_FLOAT32 || v.type == V_BOOL;
}

static inline bool _vyne_is_integral(VyneValue v) {
    return v.type == V_INT64 || v.type == V_BOOL;
}

static inline int64_t _vyne_as_i64(VyneValue v) {
    if (v.type == V_FLOAT64 || v.type == V_FLOAT32)
        return (int64_t)v.as.f64;
    return v.as.i64;
}

// Size helper used by `.size()` on any container. Maps are not part of
// the MCU type surface yet; strings are byte length.
static inline int64_t vyne_get_sizeof(VyneValue v) {
    if (v.type == V_ARRAY)
        return v.as.arr->size;
    if (v.type == V_F64_ARRAY)
        return ((VyneArray_f64*)v.as.arr)->size;
    if (v.type == V_I64_ARRAY)
        return ((VyneArray_i64*)v.as.arr)->size;
    if (v.type == V_STRING)
        return (int64_t)strlen(v.as.str);
    return 8;
}

// Dynamic index read. The compiler emits this for `xs[i]` when the
// container type is not statically known. Arrays of all three kinds are
// handled; string indexing returns a one-character string.
static inline VyneValue vyne_index_get(VyneValue base, VyneValue index) {
    if (base.type == V_ARRAY || base.type == V_F64_ARRAY ||
        base.type == V_I64_ARRAY)
        return vyne_array_get(base, index);
    if (base.type == V_STRING && index.type == V_INT64) {
        const char* s = base.as.str;
        int len = (int)strlen(s);
        int idx = (int)index.as.i64;
        if (idx < 0 || idx >= len)
            return vyne_null();
        char buf[2] = {s[idx], '\0'};
        return vyne_string(buf);
    }
    return vyne_null();
}

// Conversion used by the `string(x)` builtin. Values are formatted into
// a fixed stack buffer and copied into the arena by vyne_string().
static inline VyneValue vyne_to_string(VyneValue v) {
    char buf[64];
    switch (v.type) {
    case V_INT64:
        snprintf(buf, sizeof(buf), "%lld", (long long)v.as.i64);
        return vyne_string(buf);
    case V_FLOAT64:
        snprintf(buf, sizeof(buf), "%g", v.as.f64);
        return vyne_string(buf);
    case V_BOOL:
        return vyne_string(v.as.i64 ? "true" : "false");
    case V_STRING:
        return v;
    case V_NULL:
        return vyne_string("null");
    default:
        return vyne_string("<value>");
    }
}

// Binary arithmetic / comparison dispatcher. Division and modulo by zero
// return 0 rather than halting: on a board, a dead loop is harder to
// diagnose than a documented zero result, and the compiler's contract
// checker is the right place to reject it statically.
static inline VyneValue vyne_binop(VyneValue left, VyneValue right, int op) {
    // String equality and concatenation.
    if (left.type == V_STRING && right.type == V_STRING) {
        if (op == VBOP_EQ)
            return vyne_bool(strcmp(left.as.str, right.as.str) == 0);
        if (op == VBOP_NEQ)
            return vyne_bool(strcmp(left.as.str, right.as.str) != 0);
        if (op == VBOP_ADD) {
            size_t ln = strlen(left.as.str);
            size_t rn = strlen(right.as.str);
            char* buf = (char*)arena_alloc(ln + rn + 1);
            memcpy(buf, left.as.str, ln);
            memcpy(buf + ln, right.as.str, rn);
            buf[ln + rn] = '\0';
            VyneValue out;
            out.type = V_STRING;
            out.as.str = buf;
            return out;
        }
        return vyne_null();
    }

    // Logical connectives use truthiness for every type.
    if (op == VBOP_AND)
        return vyne_bool(vyne_is_truthy(left) && vyne_is_truthy(right));
    if (op == VBOP_OR)
        return vyne_bool(vyne_is_truthy(left) || vyne_is_truthy(right));

    // Preserve integer division and bitwise operations when both sides
    // are integral.
    if (_vyne_is_integral(left) && _vyne_is_integral(right)) {
        int64_t l = left.as.i64;
        int64_t r = right.as.i64;
        switch (op) {
        case VBOP_ADD:
            return vyne_int(l + r);
        case VBOP_SUB:
            return vyne_int(l - r);
        case VBOP_MUL:
            return vyne_int(l * r);
        case VBOP_DIV:
        case VBOP_FLOOR_DIV:
            return vyne_int(r == 0 ? 0 : l / r);
        case VBOP_MOD:
            return vyne_int(r == 0 ? 0 : l % r);
        case VBOP_POW:
            return vyne_float(pow((double)l, (double)r));
        case VBOP_EQ:
            return vyne_bool(l == r);
        case VBOP_NEQ:
            return vyne_bool(l != r);
        case VBOP_GT:
            return vyne_bool(l > r);
        case VBOP_LT:
            return vyne_bool(l < r);
        case VBOP_GTE:
            return vyne_bool(l >= r);
        case VBOP_LTE:
            return vyne_bool(l <= r);
        case VBOP_BITWISE_AND:
            return vyne_int(l & r);
        case VBOP_BITWISE_OR:
            return vyne_int(l | r);
        case VBOP_BITWISE_XOR:
            return vyne_int(l ^ r);
        case VBOP_BITWISE_SLL:
            return vyne_int(l << r);
        case VBOP_BITWISE_SRL:
            return vyne_int(l >> r);
        default:
            break;
        }
    }

    // Mixed or floating-point arithmetic.
    if (_vyne_is_numeric(left) && _vyne_is_numeric(right)) {
        double l = _vyne_as_f64(left);
        double r = _vyne_as_f64(right);
        switch (op) {
        case VBOP_ADD:
            return vyne_float(l + r);
        case VBOP_SUB:
            return vyne_float(l - r);
        case VBOP_MUL:
            return vyne_float(l * r);
        case VBOP_DIV:
        case VBOP_FLOOR_DIV:
            return vyne_float(r == 0.0 ? 0.0 : l / r);
        case VBOP_MOD:
            return vyne_float(r == 0.0 ? 0.0 : fmod(l, r));
        case VBOP_POW:
            return vyne_float(pow(l, r));
        case VBOP_EQ:
            return vyne_bool(l == r);
        case VBOP_NEQ:
            return vyne_bool(l != r);
        case VBOP_GT:
            return vyne_bool(l > r);
        case VBOP_LT:
            return vyne_bool(l < r);
        case VBOP_GTE:
            return vyne_bool(l >= r);
        case VBOP_LTE:
            return vyne_bool(l <= r);
        default:
            break;
        }
    }

    return vyne_null();
}

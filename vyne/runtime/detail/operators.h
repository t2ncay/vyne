// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tuncay Gafarli
//
// This file is part of the Vyne runtime library, distributed under the
// MIT License. See LICENSE-MIT at the repository root for the full text.

#pragma once
#include "equality.h"
#include <assert.h>
#include <winnt.h>
#include <stdio.h>
// BINARY OPERATORS
// ============================================================================

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

static inline VyneValue _vyne_float_arith(double x, int both_f32) {
    return both_f32 ? vyne_float32((float)x) : vyne_float(x);
}

static VyneValue vyne_binop_slow(VyneValue left, VyneValue right, int op) {
    // String concatenation
    if (op == VBOP_ADD && (left.type == V_STRING || right.type == V_STRING)) {
        const char* ls;
        const char* rs;
        size_t llen, rlen;
        
        if (left.type == V_STRING) {
            ls = left.as.str;
            llen = strlen(ls);
        } else {
            VyneValue s = vyne_to_string(left);
            ls = s.as.str;
            llen = strlen(ls);
        }

        if (right.type == V_STRING) {
            rs = right.as.str;
            rlen = strlen(rs);
        } else {
            VyneValue s = vyne_to_string(right);
            rs = s.as.str;
            rlen = strlen(rs);
        }

        char* res = (char*)arena_alloc(llen + rlen + 1);
        memcpy(res, ls, llen);
        memcpy(res + llen, rs, rlen);
        res[llen + rlen] = '\0';
        return vyne_string_own(res);
    }

    if (op == VBOP_ADD && _vyne_array_size(left) > 0 && _vyne_array_size(right) > 0
        && (left.type == V_ARRAY || left.type == V_F64_ARRAY || left.type == V_F32_ARRAY || left.type == V_I64_ARRAY)
        && (right.type == V_ARRAY || right.type == V_F64_ARRAY || right.type == V_F32_ARRAY || right.type == V_I64_ARRAY)) {
        VyneValue result = vyne_array_create(0);
        int64_t ln = _vyne_array_size(left);
        for (int64_t i = 0; i < ln; ++i) vyne_array_push(result, _vyne_array_elem_at(left, i));
        int64_t rn = _vyne_array_size(right);
        for (int64_t i = 0; i < rn; ++i) vyne_array_push(result, _vyne_array_elem_at(right, i));
        return result;
    }

    if (op == VBOP_AND) return vyne_bool(vyne_is_truthy(left) && vyne_is_truthy(right));
    if (op == VBOP_OR)  return vyne_bool(vyne_is_truthy(left) || vyne_is_truthy(right));

    // Float math (Float32 stays Float32 only when both operands are Float32;
    // any mix with Float64 or Int64 promotes to Float64).
    bool is_float_math = (left.type == V_FLOAT64 || left.type == V_FLOAT32 ||
                          right.type == V_FLOAT64 || right.type == V_FLOAT32);
    bool lnumeric = (left.type == V_FLOAT64 || left.type == V_FLOAT32 ||
                     left.type == V_INT64);
    bool rnumeric = (right.type == V_FLOAT64 || right.type == V_FLOAT32 ||
                     right.type == V_INT64);
    if (is_float_math && lnumeric && rnumeric) {
        double l = (left.type == V_INT64) ? (double)left.as.i64 : left.as.f64;
        double r = (right.type == V_INT64) ? (double)right.as.i64 : right.as.f64;
        int both_f32 = (left.type == V_FLOAT32 && right.type == V_FLOAT32);
        switch (op) {
            case VBOP_ADD: return _vyne_float_arith(l + r, both_f32);
            case VBOP_SUB: return _vyne_float_arith(l - r, both_f32);
            case VBOP_MUL: return _vyne_float_arith(l * r, both_f32);
            case VBOP_DIV:
                if (r == 0.0) { fprintf(stderr, "Runtime error: Division by zero!\n"); exit(1); }
                return _vyne_float_arith(l / r, both_f32);
            case VBOP_MOD:
                if (r == 0.0) { fprintf(stderr, "Runtime error: Modulo by zero!\n"); exit(1); }
                return _vyne_float_arith(fmod(l, r), both_f32);
            case VBOP_POW: return _vyne_float_arith(pow(l, r), both_f32);
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
    if (VYNE_LIKELY(left.type == V_INT64 && right.type == V_INT64)) {
        int64_t l = left.as.i64;
        int64_t r = right.as.i64;
        switch (op) {
            case VBOP_ADD: return vyne_int(l + r);
            case VBOP_SUB: return vyne_int(l - r);
            case VBOP_MUL: return vyne_int(l * r);
            case VBOP_BITWISE_AND: return vyne_int(l & r);
            case VBOP_BITWISE_OR: return vyne_int(l | r);
            case VBOP_BITWISE_XOR: return vyne_int(l ^ r);
            case VBOP_BITWISE_SLL : return vyne_int(l << r);
            case VBOP_BITWISE_SRL: return vyne_int(l >> r);
            case VBOP_FLOOR_DIV:
            case VBOP_DIV:
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

    if (VYNE_LIKELY(left.type == V_FLOAT64 && right.type == V_FLOAT64)) {
        double l = left.as.f64;
        double r = right.as.f64;
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
            case VBOP_AND: return vyne_bool((l != 0.0) && (r != 0.0));
            case VBOP_OR:  return vyne_bool((l != 0.0) || (r != 0.0));
            case VBOP_BITWISE_AND:
            case VBOP_BITWISE_XOR:
            case VBOP_BITWISE_OR:
            case VBOP_BITWISE_SLL:
            case VBOP_BITWISE_SRL: {
                printf("Behavior of this is yet to be implemented. FILE : %s, LINE : %d", __FILE__,__LINE__);
                assert(false);
            }
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

    if (right.type == V_ARRAY || right.type == V_F64_ARRAY || right.type == V_I64_ARRAY) {
        int64_t n = _vyne_array_size(right);
        for (int64_t i = 0; i < n; ++i) {
            if (vyne_values_equal(left, _vyne_array_elem_at(right, i))) {
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
        case VBOP_NEQ: // '!' 
            return vyne_bool(!vyne_is_truthy(val));
        case VBOP_SUB: // '-'
            if (val.type == V_INT64) return vyne_int(-val.as.i64);
            if (val.type == V_FLOAT64) return vyne_float(-val.as.f64);
            if (val.type == V_FLOAT32) return vyne_float32((float)(-val.as.f64));
            return vyne_null();
        case VBOP_BITWISE_NOT:
            return vyne_int(~val.as.i64);
        default:
            return val;
    }
}


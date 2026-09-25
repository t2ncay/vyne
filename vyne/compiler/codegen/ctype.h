#pragma once
#include <string>
#include <vector>

#include "../types.h"

// ============================================================================
// CType — static type → native C type mapping for the transpiler.
//
// M0 (this header): the mapping tables, box/unbox helpers, and mangling
// suffixes. No behavioral change to emitted C on its own; consumers opt in
// milestone by milestone.
//
// The core rule (issue #79 §3.1):
//   A value is emitted as its native C type iff the emitter can statically
//   prove its type at every use site AND it never crosses a dynamic boundary
//   unboxed. Otherwise boxed as VyneValue (today's behavior).
// ============================================================================

struct CType {
    enum class Kind {
        Unknown,    // dynamic → stay boxed as VyneValue
        Int64,
        Float64,
        Bool,
        Str,        // M1: still boxed (char* + length is a later milestone)
        Array,      // element type in args[0]
        Map,
        Struct,
        Module,
        Function
    };

    Kind kind = Kind::Unknown;
    std::vector<CType> args;   // element type for Array<T>; field types for Struct
    std::string mangledName;   // mangled C name for monomorphized structs/interfaces
    std::string nativeName;    // "int64_t", "double", "bool", "VyneValue", ...

    bool isBoxed() const {
        return kind != Kind::Int64 && kind != Kind::Float64 && kind != Kind::Bool;
    }

    bool isPrimitive() const {
        return kind == Kind::Int64 || kind == Kind::Float64 || kind == Kind::Bool;
    }

    // The C type name for an *unboxed* value of this static type,
    // or "VyneValue" when boxed.
    std::string cTypeName() const {
        switch (kind) {
            case Kind::Int64:   return "int64_t";
            case Kind::Float64: return "double";
            case Kind::Bool:    return "bool";
            default:            return "VyneValue";
        }
    }

    // Wrap a native-typed C expression into a VyneValue expression.
    // Only valid when isPrimitive().
    std::string box(const std::string& expr) const {
        switch (kind) {
            case Kind::Int64:   return "vyne_int(" + expr + ")";
            case Kind::Float64: return "vyne_float(" + expr + ")";
            case Kind::Bool:    return "vyne_bool(" + expr + ")";
            default:            return expr;
        }
    }

    // Read the native member out of a (boxed) VyneValue expression.
    // Only valid when isPrimitive().
    std::string unbox(const std::string& expr) const {
        switch (kind) {
            case Kind::Int64:   return expr + ".as.i64";
            case Kind::Float64: return expr + ".as.f64";
            case Kind::Bool:    return expr + ".as.i64"; // bools live in .as.i64
            default:            return expr;
        }
    }

    // Mangled suffix for monomorphized names, e.g. fn_max_of__i64_f64.
    std::string mangleSuffix() const {
        std::string s = mangleBase();
        for (const auto& a : args) s += "__" + a.mangleSuffix();
        return s;
    }

    std::string mangleBase() const {
        switch (kind) {
            case Kind::Int64:   return "i64";
            case Kind::Float64: return "f64";
            case Kind::Bool:    return "b";
            case Kind::Str:     return "str";
            case Kind::Array:   return mangledName.empty() ? "arr" : mangledName;
            case Kind::Map:     return "map";
            case Kind::Struct:  return mangledName.empty() ? "struct" : mangledName;
            case Kind::Module:  return "mod";
            case Kind::Function: return "fn";
            default:            return "unk";
        }
    }

    VType toVType() const {
        switch (kind) {
            case Kind::Int64:   return VType::Int64;
            case Kind::Float64: return VType::Float64;
            case Kind::Bool:    return VType::Bool;
            case Kind::Str:     return VType::String;
            case Kind::Array:   return VType::Array;
            case Kind::Map:     return VType::Map;
            case Kind::Struct:  return VType::Struct;
            case Kind::Module:  return VType::Module;
            case Kind::Function: return VType::Function;
            default:            return VType::Unknown;
        }
    }

    static CType fromVType(VType t) {
        switch (t) {
            case VType::Int64:   return CType{Kind::Int64};
            case VType::Float64: return CType{Kind::Float64};
            case VType::Bool:    return CType{Kind::Bool};
            case VType::String:  return CType{Kind::Str};
            case VType::Array:   return CType{Kind::Array};
            case VType::Map:     return CType{Kind::Map};
            case VType::Struct:  return CType{Kind::Struct};
            case VType::Module:  return CType{Kind::Module};
            case VType::Function:return CType{Kind::Function};
            default:             return CType{Kind::Unknown};
        }
    }

    static CType fromKind(Kind k) { CType c; c.kind = k; return c; }
};
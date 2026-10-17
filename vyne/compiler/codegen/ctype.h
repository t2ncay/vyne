// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Tuncay Gafarli
//
// This file is part of the Vyne compiler.
//
// Vyne is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License as published by the
// Free Software Foundation, version 3.
//
// Vyne is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public
// License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with Vyne. If not, see <https://www.gnu.org/licenses/>.

#pragma once
#include <cstdint>
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
        Unknown, // dynamic → stay boxed as VyneValue
        Int64,
        Float64,
        Bool,
        Str,         // M1: still boxed (char* + length is a later milestone)
        Array,       // element type in args[0]
        RawArrayPtr, // NEW: borrowed T* passed by the native ABI. Element type
                     // in args[0].
        Map,
        Struct,
        Module,
        Function
    };

    Kind kind = Kind::Unknown;
    std::vector<CType>
        args; // element type for Array<T>; field types for Struct
    std::vector<int64_t> shape; // row-major dims for scratch arrays
    std::string
        mangledName; // mangled C name for monomorphized structs/interfaces
    std::string nativeName; // "int64_t", "double", "bool", "VyneValue", ...

    // Slice 3: true iff this CType describes a value whose C-level
    // representation is the native C struct typedef emitted by
    // InterfaceNode::compile (e.g. `vyne_Point x;`). False for the
    // other Struct-kind case the codebase already uses: a boxed
    // VyneValue whose semantic type is a struct, produced by
    // native_dispatch.cpp for struct-returning native-ABI calls. Both
    // carry Kind::Struct and a mangledName; only this flag distinguishes
    // them, so boxAny can tell whether `.field` access is valid C.
    bool nativeCStruct = false;

    // Per-field descriptor for struct-typed native-variant parameters.
    // Populated by InterfaceNode::compile from the interface member list,
    // in declaration order. Consumed by tryEmitNativeCall (caller side)
    // and emitNativeFunctionBody (callee side). Both sides MUST see the
    // same ordering, which is guaranteed because both read from the
    // emitter's interfaceStructLayout map.
    //
    // `type` is one of:
    //   - Kind::Int64 / Kind::Float64          → scalar field
    //   - Kind::Array with args[0] = Int64/Float64 → typed-array field
    // Anything else is rejected at emission time by the callee-side
    // signature builder, which throws rather than emit broken C.

    CType() = default;
    CType(Kind k)
        : kind(k) {}
    CType(Kind k, std::vector<CType> a, std::vector<int64_t> s = {})
        : kind(k)
        , args(std::move(a))
        , shape(std::move(s)) {}

    // scratch shaped methods
    bool hasShape() const {
        return !shape.empty();
    }

    int64_t numElements() const {
        if (shape.empty())
            return -1;
        int64_t n = 1;
        for (int64_t d : shape)
            n *= d;
        return n;
    }

    bool sameShape(const CType& o) const {
        return shape == o.shape;
    }

    std::vector<int64_t> strides() const {
        std::vector<int64_t> s(shape.size());
        int64_t acc = 1;
        for (int i = (int)shape.size() - 1; i >= 0; --i) {
            s[i] = acc;
            acc *= shape[i];
        }
        return s;
    }

    bool isBoxed() const {
        return kind != Kind::Int64 && kind != Kind::Float64 &&
               kind != Kind::Bool;
    }

    bool isPrimitive() const {
        return kind == Kind::Int64 || kind == Kind::Float64 ||
               kind == Kind::Bool;
    }

    // The C type name for an *unboxed* value of this static type,
    // or "VyneValue" when boxed.
    std::string cTypeName() const {
        switch (kind) {
        case Kind::Int64:
            return "int64_t";
        case Kind::Float64:
            return "double";
        case Kind::Bool:
            return "bool";
        case Kind::RawArrayPtr: {
            if (args.empty())
                return "void*";
            return args[0].cTypeName() + "*";
        }
        // Slice 3e: an Array CType with a primitive or C-eligible-
        // struct element renders as the monomorphized container
        // name. The struct's typedef tag is stored in nativeName
        // by every construction site.
        case Kind::Array: {
            if (args.size() == 1) {
                const CType& el = args[0];
                if (el.kind == Kind::Float64)
                    return "VyneArray_f64";
                if (el.kind == Kind::Int64)
                    return "VyneArray_i64";
                if (el.kind == Kind::Struct && el.nativeCStruct &&
                    !el.nativeName.empty()) {
                    return "vyne_Array_" + el.nativeName;
                }
            }
            return "VyneValue";
        }
        // Slice 3a: a native C struct renders as its emitted
        // typedef tag when the CType carries one.
        case Kind::Struct:
            if (nativeCStruct && !nativeName.empty())
                return nativeName;
            return "VyneValue";
        default:
            return "VyneValue";
        }
    }

    // Wrap a native-typed C expression into a VyneValue expression.
    // Only valid when isPrimitive().
    std::string box(const std::string& expr) const {
        switch (kind) {
        case Kind::Int64:
            return "vyne_int(" + expr + ")";
        case Kind::Float64:
            return "vyne_float(" + expr + ")";
        case Kind::Bool:
            return "vyne_bool(" + expr + ")";
        default:
            return expr;
        }
    }

    // Read the native member out of a (boxed) VyneValue expression.
    // Only valid when isPrimitive().
    std::string unbox(const std::string& expr) const {
        switch (kind) {
        case Kind::Int64:
            return expr + ".as.i64";
        case Kind::Float64:
            return expr + ".as.f64";
        case Kind::Bool:
            return expr + ".as.i64"; // bools live in .as.i64
        default:
            return expr;
        }
    }

    // Mangled suffix for monomorphized names, e.g. fn_max_of__i64_f64.
    std::string mangleSuffix() const {
        std::string s = mangleBase();
        for (const auto& a : args)
            s += "__" + a.mangleSuffix();
        return s;
    }

    std::string mangleBase() const {
        switch (kind) {
        case Kind::Int64:
            return "i64";
        case Kind::Float64:
            return "f64";
        case Kind::Bool:
            return "b";
        case Kind::Str:
            return "str";
        case Kind::Array:
            return mangledName.empty() ? "arr" : mangledName;
        case Kind::Map:
            return "map";
        case Kind::Struct:
            return mangledName.empty() ? "struct" : mangledName;
        case Kind::Module:
            return "mod";
        case Kind::Function:
            return "fn";
        default:
            return "unk";
        }
    }

    VType toVType() const {
        switch (kind) {
        case Kind::Int64:
            return VType::Int64;
        case Kind::Float64:
            return VType::Float64;
        case Kind::Bool:
            return VType::Bool;
        case Kind::Str:
            return VType::String;
        case Kind::Array:
            return VType::Array;
        case Kind::Map:
            return VType::Map;
        case Kind::Struct:
            return VType::Struct;
        case Kind::Module:
            return VType::Module;
        case Kind::Function:
            return VType::Function;
        default:
            return VType::Unknown;
        }
    }

    static CType fromVType(VType t) {
        switch (t) {
        case VType::Int64:
            return CType{Kind::Int64};
        case VType::Float64:
            return CType{Kind::Float64};
        case VType::Bool:
            return CType{Kind::Bool};
        case VType::String:
            return CType{Kind::Str};
        case VType::Array:
            return CType{Kind::Array};
        case VType::Map:
            return CType{Kind::Map};
        case VType::Struct:
            return CType{Kind::Struct};
        case VType::Module:
            return CType{Kind::Module};
        case VType::Function:
            return CType{Kind::Function};
        default:
            return CType{Kind::Unknown};
        }
    }

    static CType fromKind(Kind k) {
        CType c;
        c.kind = k;
        return c;
    }

    // Map a Vyne element type to its C scalar name. Used by scratch arrays
    // and typed-array fallbacks. Unknown maps to the boxed name.
    static std::string elemCName(VType t) {
        switch (t) {
        case VType::Int64:
            return "int64_t";
        case VType::Float64:
            return "double";
        case VType::Bool:
            return "bool";
        default:
            return "VyneValue";
        }
    }

    // Map a Vyne element type to the typed-array container struct name.
    static std::string arrayContainerName(VType elem) {
        switch (elem) {
        case VType::Int64:
            return "VyneArray_i64";
        case VType::Float64:
            return "VyneArray_f64";
        default:
            return "VyneValue";
        }
    }
};

// ============================================================================
// StructFieldDesc — layout descriptor for struct-typed native-variant params
// ----------------------------------------------------------------------------
// A struct-typed parameter in a native variant expands into one C parameter
// per field. This descriptor is what the caller-side dispatcher
// (tryEmitNativeCall) and the callee-side signature builder
// (emitNativeFunctionBody) both consult to agree on that expansion.
//
// It lives in the emitter's `interfaceStructLayout` map, keyed by every
// spelling of the interface name under which the interface was registered.
// It is NOT a member of CType: a CType describes a value's representation,
// whereas this describes how an interface's members are laid out for the
// flat ABI. Keeping them separate avoids both the incomplete-type cycle
// (CType would need to hold a CType by value) and a redundant second copy
// of the field list.
//
// Allowed field types:
//   - Kind::Int64               → `int64_t` C parameter
//   - Kind::Float64             → `double` C parameter
//   - Kind::Array with args[0] = Int64 or Float64
//                               → `VyneArray_i64*` / `VyneArray_f64*`
// Any other field type disqualifies the whole interface from native
// dispatch; InterfaceNode::compile refuses to register a layout in that
// case, and the function is emitted in boxed form only.
// ============================================================================
struct StructFieldDesc {
    uint32_t id;      // StringPool::intern(name) — stable across the process
    std::string name; // for diagnostics and C parameter naming
    CType type;       // scalar kind, or Array with element kind
};

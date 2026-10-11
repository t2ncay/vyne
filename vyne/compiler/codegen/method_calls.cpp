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

#include "detail/codegen_helpers.h"
#include <optional>

// Method calls, including specialized collection and native methods.
// Keep expressions that emit statements in evaluation order; see README.md.
//
// ============================================================
// Dispatch chain
// ------------------------------------------------------------
// MethodCallNode::getCExpr is a linear chain of `tryEmit*` handlers.
// Each returns std::nullopt when its category does not match, so the
// caller falls through to the next. The order below mirrors the
// original single-function version and is load-bearing:
//
//   1. native module            (vmath.sqrt, vcore.now, ...)
//   2. interface constructor    (a.b.Ctor(...))
//   3. group call               (vlin.add(a, b), vml.dense(...))
//   4. byte_at                  (str.byte_at(i))
//   5. typed-array fast path    (push, size, length on Array<T>)
//   6. boxed receiver bind      (single alloc for everything below)
//   7. built-in array methods   (pop, sort, delete, clear, ...)
//   8. string methods           (substr, find, uppercase, ...)
//   9. struct field list        (.fields)
//  10. map methods              (has, keys, values, set)
//  11. user-defined struct      (last resort)
//
// The receiver's C expression is evaluated exactly once, before step 4,
// and passed to every subsequent handler. Argument evaluation inside a
// handler follows source order, after the receiver.
// ============================================================

namespace {

// ----------------------------------------------------------------
// Small shared helpers
// ----------------------------------------------------------------

// Receiver's dotted path ("vmath", "vlinalg.Types", or a bare name).
// Empty for receivers that aren't a Variable or MemberAccess.
std::string receiverPath(const ASTNode* receiver) {
    if (!receiver)
        return "";
    if (receiver->type() == NodeType::VARIABLE) {
        return static_cast<const VariableNode*>(receiver)->getOriginalName();
    }
    if (receiver->type() == NodeType::MEMBER_ACCESS) {
        return static_cast<const MemberAccessNode*>(receiver)->getFullPath();
    }
    return "";
}

// True when the receiver names a variable the emitter has registered
// as a user-defined struct/interface. Used so a struct method named
// `size`/`length` wins over the built-in `vyne_get_sizeof` fallback.
bool receiverNamesStruct(C_Emitter& e, const ASTNode* receiver) {
    if (!receiver || receiver->type() != NodeType::VARIABLE)
        return false;

    auto* var = static_cast<const VariableNode*>(receiver);
    std::string recvName = var->getOriginalName();
    std::string prefix = e.getActiveFunctionPrefix();
    std::string lookupKey =
        prefix.empty() ? ("v_" + recvName) : ("v_" + prefix + "_" + recvName);

    const std::string* typeName = e.lookupLocalStructType(lookupKey);
    if (!typeName)
        typeName = e.lookupGlobalStructType("v_" + recvName);
    if (!typeName)
        return false;

    return *typeName != "String" && *typeName != "Int64" &&
           *typeName != "Float64" && *typeName != "Bool" &&
           *typeName != "Array" && *typeName != "Map" && *typeName != "Null";
}

// ----------------------------------------------------------------
// 1. Native module dispatch
// ----------------------------------------------------------------
std::optional<std::string> tryEmitNativeModuleMethod(
    C_Emitter& e,
    const MethodCallNode& node,
    const std::string& recvPath
) {
    if (node.getReceiver()->type() != NodeType::VARIABLE)
        return std::nullopt;

    const NativeMapEntry* entry = e.findNative(recvPath, node.getMethodName());
    if (!entry)
        return std::nullopt;

    // Zero-arg getter: `vmath.pi()` -> "vmath_pi()".
    if (entry->isProperty)
        return std::string(entry->cName);

    const auto& args = node.getArguments();
    std::string resTemp = e.newTemp("n_ret");

    // Variadic: (argc, argv).
    if (entry->usesArgv) {
        int n = (int)args.size();
        std::string argArr = e.newTemp("n_args");
        e.emit(
            "VyneValue* " + argArr +
            " = (VyneValue*)arena_alloc(sizeof(VyneValue) * " +
            std::to_string(n > 0 ? n : 1) + ");"
        );
        for (int i = 0; i < n; ++i) {
            e.emit(
                argArr + "[" + std::to_string(i) +
                "] = " + e.boxAny(args[i]->getCExpr(e)) + ";"
            );
        }
        e.emit(
            "VyneValue " + resTemp + " = " + entry->cName + "(" +
            std::to_string(n) + ", " + argArr + ");"
        );
        return resTemp;
    }

    // M5: unboxed _f64 variant when every arg is a provable double.
    if (entry->nativeF64 && !args.empty()) {
        bool allF64 = true;
        for (const auto& a : args) {
            std::string raw = a->getCExpr(e);
            const CType* ct = e.exprNativeType(raw);
            if (!ct || ct->kind != CType::Kind::Float64) {
                allF64 = false;
                break;
            }
        }
        if (allF64) {
            std::string argStr;
            for (size_t i = 0; i < args.size(); ++i) {
                if (i > 0)
                    argStr += ", ";
                argStr += args[i]->getCExpr(e);
            }
            e.emit(
                "double " + resTemp + " = " + entry->nativeF64 + "(" + argStr +
                ");"
            );
            e.declareNativeTemp(resTemp, CType::fromKind(CType::Kind::Float64));
            return resTemp;
        }
    }

    // Fixed arity.
    std::string argStr;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0)
            argStr += ", ";
        argStr += e.boxAny(args[i]->getCExpr(e));
    }
    e.emit("VyneValue " + resTemp + " = " + entry->cName + "(" + argStr + ");");
    return resTemp;
}

// ----------------------------------------------------------------
// 2. Interface constructor via a dotted path
// ----------------------------------------------------------------
std::optional<std::string> tryEmitInterfaceConstructor(
    C_Emitter& e,
    const MethodCallNode& node,
    const std::string& recvPath
) {
    if (recvPath.empty())
        return std::nullopt;

    // Full path, then progressively shorter suffixes, then bare name.
    std::vector<std::string> candidates;
    candidates.push_back(recvPath);
    {
        std::string tmp = recvPath;
        size_t dot;
        while ((dot = tmp.find('.')) != std::string::npos) {
            tmp = tmp.substr(dot + 1);
            candidates.push_back(tmp);
        }
    }
    candidates.push_back("");

    const std::string& methodName = node.getMethodName();
    const auto& args = node.getArguments();

    for (const auto& base : candidates) {
        std::string dotted =
            base.empty() ? methodName : (base + "." + methodName);
        std::string mangled =
            base.empty() ? methodName : (base + "_" + methodName);
        std::replace(mangled.begin(), mangled.end(), '.', '_');

        if (!e.isInterface(dotted) && !e.isInterface(mangled))
            continue;

        // The name the caller wrote is not necessarily the name the
        // interface was emitted under. `vchem.Graph` and `Graph` both
        // reference the same interface, whose C constructor might be
        // `struct_vchem_Types_Graph`. Ask the emitter for the suffix it
        // recorded, and fall back to the local mangle only when nothing
        // is registered (in which case the call was against an interface
        // declared under exactly this spelling).
        const std::string* ctorSuffixPtr = e.getInterfaceCtor(dotted);
        if (!ctorSuffixPtr)
            ctorSuffixPtr = e.getInterfaceCtor(mangled);
        std::string ctorSuffix = ctorSuffixPtr ? *ctorSuffixPtr : mangled;

        const std::vector<std::string>* defaults =
            e.getInterfaceDefaults(dotted);
        if (!defaults)
            defaults = e.getInterfaceDefaults(mangled);
        if (!defaults)
            defaults = e.getInterfaceDefaults(methodName);
        std::vector<std::string> argStrs;
        argStrs.reserve(args.size());
        for (const auto& a : args)
            argStrs.push_back(e.boxAny(a->getCExpr(e)));
        if (defaults) {
            for (size_t i = argStrs.size(); i < defaults->size(); ++i) {
                argStrs.push_back((*defaults)[i]);
            }
        }

        std::string directArgs;
        for (size_t i = 0; i < argStrs.size(); ++i) {
            if (i > 0)
                directArgs += ", ";
            directArgs += argStrs[i];
        }

        std::string resTemp = e.newTemp("g_iface");
        e.emit(
            "VyneValue " + resTemp + " = struct_" + ctorSuffix + "(" +
            directArgs + ");"
        );
        return resTemp;
    }
    return std::nullopt;
}

// ----------------------------------------------------------------
// 3. Group call
// ----------------------------------------------------------------
std::optional<std::string> tryEmitGroupMethod(
    C_Emitter& e,
    const MethodCallNode& node,
    const std::string& recvPath
) {
    if (recvPath.empty() || !e.isGroup(recvPath))
        return std::nullopt;

    const std::string& methodName = node.getMethodName();
    const auto& args = node.getArguments();

    if (auto blas = tryEmitBlasCall(e, recvPath, methodName, args)) {
        return *blas;
    }

    // Argument expressions are evaluated once and reused by the native
    // dispatcher and the boxed fallback.
    std::vector<ASTNode*> orderedArgs;
    orderedArgs.reserve(args.size());
    for (const auto& a : args)
        orderedArgs.push_back(a.get());

    std::vector<std::string> argCExprs;
    std::vector<VType> argVTypes;
    std::vector<const CType*> argCTypes;
    argCExprs.reserve(orderedArgs.size());
    argVTypes.reserve(orderedArgs.size());
    argCTypes.reserve(orderedArgs.size());

    for (auto* a : orderedArgs) {
        std::string s = a->getCExpr(e);
        const CType* fullCt = e.lookupType(s);
        const CType* primCt = e.exprNativeType(s);
        VType t = primCt ? primCt->toVType() : a->getStaticType();
        argCExprs.push_back(std::move(s));
        argVTypes.push_back(t);
        argCTypes.push_back(fullCt);
    }

    std::string gMethodName = recvPath + "_" + methodName;
    std::replace(gMethodName.begin(), gMethodName.end(), '.', '_');

    if (auto native = tryEmitNativeCall(
            e,
            gMethodName,
            methodName,
            orderedArgs,
            argCExprs,
            argVTypes,
            argCTypes
        )) {
        return *native;
    }

    // Boxed fallback.
    int argSize = (int)orderedArgs.size();
    std::string argArr = e.newTemp("g_args");
    if (argSize > 0) {
        e.emit(
            "VyneValue* " + argArr +
            " = (VyneValue*)arena_alloc(sizeof(VyneValue) * " +
            std::to_string(argSize) + ");"
        );
        for (int i = 0; i < argSize; ++i) {
            e.emit(
                argArr + "[" + std::to_string(i) +
                "] = " + e.boxAny(argCExprs[i]) + ";"
            );
        }
    } else {
        e.emit("VyneValue* " + argArr + " = NULL;");
    }

    std::string resTemp = e.newTemp("g_ret");
    e.emit(
        "VyneValue " + resTemp + " = fn_" + gMethodName + "(" +
        std::to_string(argSize) + ", " + argArr + ");"
    );
    return resTemp;
}

// ----------------------------------------------------------------
// 4. str.byte_at(i) -> Int64
// ----------------------------------------------------------------
std::optional<std::string> tryEmitByteAt(
    C_Emitter& e,
    const MethodCallNode& node,
    const std::string& recvRaw
) {
    if (node.getMethodName() != "byte_at")
        return std::nullopt;

    const auto& args = node.getArguments();
    if (args.size() != 1) {
        throw std::runtime_error(
            "Compile Error: byte_at() requires exactly 1 argument (line " +
            std::to_string(node.lineNumber) + ")"
        );
    }

    // Bind the receiver once so the tag check and the pointer read see
    // the same value, even when the receiver is a compound expression.
    std::string recvTmp = e.newTemp("brecv");
    e.emit("VyneValue " + recvTmp + " = " + e.boxAny(recvRaw) + ";");

    std::string rawIdx = args[0]->getCExpr(e);
    std::string idx = coerceToNative(e, args[0].get(), rawIdx, VType::Int64);

    std::string temp = e.newTemp("byte");
    e.emit(
        "int64_t " + temp + " = (" + recvTmp +
        ".type == V_STRING) "
        "? (int64_t)(unsigned char)(" +
        recvTmp + ".as.str[" + idx +
        "]) "
        ": 0;"
    );
    e.declareNativeTemp(temp, CType::fromKind(CType::Kind::Int64));
    return temp;
}

// ----------------------------------------------------------------
// 5. Typed-array fast path: push, size, length
//
// Fires only when the receiver's CType is Array<Float64> or Array<Int64>.
// A bare `Array`, String, Map, or user-defined struct falls through to
// the boxed handlers below.
//
// `push` handles both branches here — the boxed fallback uses its own
// temp name and binds a fresh boxed receiver, matching the original
// code's ordering (the shared `recv` bind has not run yet).
// ----------------------------------------------------------------
std::optional<std::string> tryEmitTypedArrayMethod(
    C_Emitter& e,
    const MethodCallNode& node,
    const std::string& recvRaw
) {
    const std::string& methodName = node.getMethodName();
    const auto& args = node.getArguments();
    const CType* rt = e.lookupType(recvRaw);

    if (methodName == "push") {
        if (rt && rt->kind == CType::Kind::Array && !rt->args.empty()) {
            VType elem = rt->args[0].toVType();
            std::string pushFn = (elem == VType::Float64)
                                     ? "vyne_array_f64_push"
                                     : "vyne_array_i64_push";
            for (const auto& argNode : args) {
                std::string rawVal = argNode->getCExpr(e);
                std::string val =
                    coerceToNative(e, argNode.get(), rawVal, elem);
                e.emit(pushFn + "(&" + recvRaw + ", " + val + ");");
            }
            return recvRaw;
        }

        // Boxed push fallback — its own bind, matching the original order.
        std::string recvBoxed = e.newTemp("m_recv");
        e.emit("VyneValue " + recvBoxed + " = " + e.boxAny(recvRaw) + ";");
        for (const auto& argNode : args) {
            e.emit(
                "vyne_array_push(" + recvBoxed + ", " +
                e.boxAny(argNode->getCExpr(e)) + ");"
            );
        }
        return recvBoxed;
    }

    if ((methodName == "size" || methodName == "length") && rt &&
        rt->kind == CType::Kind::Array && !rt->args.empty()) {
        std::string temp = e.newTemp("len");
        e.emit("VyneValue " + temp + " = vyne_int(" + recvRaw + ".size);");
        return temp;
    }

    return std::nullopt;
}

// ----------------------------------------------------------------
// 6. Built-in container methods on a boxed receiver
//
// pop, reverse, size/length fallback, pop_front, back, delete_at,
// sort, place_all, delete, clear. The size/length fallback is gated
// on `!receiverIsStruct` so a user-defined `size()` method wins.
//
// `delete` and `clear` are polymorphic across arrays and maps via
// vyne_delete_any / vyne_clear_any.
// ----------------------------------------------------------------
std::optional<std::string> tryEmitBuiltinArrayMethod(
    C_Emitter& e,
    const MethodCallNode& node,
    const std::string& recv,
    bool receiverIsStruct
) {
    if (receiverIsStruct)
        return std::nullopt;

    const std::string& methodName = node.getMethodName();
    const auto& args = node.getArguments();

    if (methodName == "pop") {
        std::string temp = e.newTemp("pop");
        e.emit("VyneValue " + temp + " = vyne_array_pop(" + recv + ");");
        return temp;
    }
    if (methodName == "reverse") {
        e.emit("vyne_array_reverse(" + recv + ");");
        return recv;
    }
    if (!receiverIsStruct && (methodName == "length" || methodName == "size")) {
        std::string temp = e.newTemp("len");
        e.emit(
            "VyneValue " + temp + " = vyne_int(vyne_get_sizeof(" + recv + "));"
        );
        return temp;
    }
    if (methodName == "pop_front") {
        std::string temp = e.newTemp("pf");
        e.emit("VyneValue " + temp + " = vyne_array_pop_front(" + recv + ");");
        return temp;
    }
    if (methodName == "back") {
        std::string temp = e.newTemp("bk");
        e.emit("VyneValue " + temp + " = vyne_array_back(" + recv + ");");
        return temp;
    }
    if (methodName == "delete_at") {
        std::string idx = e.boxAny(args[0]->getCExpr(e));
        std::string temp = e.newTemp("delat");
        e.emit(
            "VyneValue " + temp + " = vyne_array_delete_at(" + recv + ", (" +
            idx + ").as.i64);"
        );
        return temp;
    }
    if (methodName == "sort") {
        e.emit("vyne_array_sort(" + recv + ");");
        return recv;
    }
    if (methodName == "place_all") {
        std::string v = e.boxAny(args[0]->getCExpr(e));
        std::string c = e.boxAny(args[1]->getCExpr(e));
        e.emit(
            "vyne_array_place_all(" + recv + ", " + v + ", (" + c + ").as.i64);"
        );
        return recv;
    }
    if (methodName == "delete") {
        std::string k = e.boxAny(args[0]->getCExpr(e));
        std::string temp = e.newTemp("del");
        e.emit(
            "VyneValue " + temp + " = vyne_delete_any(" + recv + ", " + k + ");"
        );
        return temp;
    }
    if (methodName == "clear") {
        e.emit("vyne_clear_any(" + recv + ");");
        return recv;
    }
    return std::nullopt;
}

// ----------------------------------------------------------------
// 7. String methods
// ----------------------------------------------------------------
std::optional<std::string> tryEmitStringMethod(
    C_Emitter& e,
    const MethodCallNode& node,
    const std::string& recv
) {
    const std::string& methodName = node.getMethodName();
    const auto& args = node.getArguments();

    if (methodName == "substr") {
        if (args.empty()) {
            throw std::runtime_error(
                "Compile Error: substr() requires at least 1 argument (line " +
                std::to_string(node.lineNumber) + ")"
            );
        }
        std::string s = e.boxAny(args[0]->getCExpr(e));
        std::string c = (args.size() >= 2) ? e.boxAny(args[1]->getCExpr(e))
                                           : "vyne_int(-1)";
        std::string temp = e.newTemp("substr");
        e.emit(
            "VyneValue " + temp + " = vyne_string_substr(" + recv + ", (" + s +
            ").as.i64, (" + c + ").as.i64);"
        );
        return temp;
    }
    if (methodName == "find") {
        if (args.empty()) {
            throw std::runtime_error(
                "Compile Error: find() requires 1 argument (line " +
                std::to_string(node.lineNumber) + ")"
            );
        }
        std::string target = e.boxAny(args[0]->getCExpr(e));
        std::string temp = e.newTemp("find");
        e.emit(
            "VyneValue " + temp + " = vyne_string_find(" + recv + ", " +
            target + ");"
        );
        return temp;
    }
    if (methodName == "uppercase") {
        std::string temp = e.newTemp("up");
        e.emit("VyneValue " + temp + " = vyne_string_uppercase(" + recv + ");");
        return temp;
    }
    if (methodName == "lowercase") {
        std::string temp = e.newTemp("lo");
        e.emit("VyneValue " + temp + " = vyne_string_lowercase(" + recv + ");");
        return temp;
    }
    if (methodName == "trim") {
        std::string temp = e.newTemp("tr");
        e.emit("VyneValue " + temp + " = vyne_string_trim(" + recv + ");");
        return temp;
    }
    if (methodName == "replace") {
        if (args.size() < 2) {
            throw std::runtime_error(
                "Compile Error: replace() requires 2 arguments (line " +
                std::to_string(node.lineNumber) + ")"
            );
        }
        std::string o = e.boxAny(args[0]->getCExpr(e));
        std::string n = e.boxAny(args[1]->getCExpr(e));
        std::string temp = e.newTemp("rep");
        e.emit(
            "VyneValue " + temp + " = vyne_string_replace(" + recv + ", " + o +
            ", " + n + ");"
        );
        return temp;
    }
    return std::nullopt;
}

// ----------------------------------------------------------------
// 8. Struct field list: obj.fields -> Array<String>
// ----------------------------------------------------------------
std::optional<std::string> tryEmitStructFieldList(
    C_Emitter& e,
    const MethodCallNode& node,
    const std::string& recv
) {
    if (node.getMethodName() != "fields")
        return std::nullopt;

    std::string temp = e.newTemp("flds");
    e.emit("VyneValue " + temp + " = vyne_array_create(0);");
    e.emitBlockOpen("if (" + recv + ".type == V_STRUCT) {");
    e.emit("VyneStruct* __s = " + recv + ".as.strct;");
    e.emitBlockOpen("for (int __i = 0; __i < __s->field_count; __i++) {");
    e.emit(
        "vyne_array_push(" + temp + ", vyne_string(__s->fields[__i].name));"
    );
    e.emitBlockClose();
    e.emitBlockClose();
    return temp;
}

// ----------------------------------------------------------------
// 9. Map methods
// ----------------------------------------------------------------
std::optional<std::string> tryEmitMapMethod(
    C_Emitter& e,
    const MethodCallNode& node,
    const std::string& recv,
    bool receiverIsStruct
) {
    // A user-defined struct's own methods win over built-ins that share
    // a name. Without this guard, `Matrix.get(r, c)` would be
    // intercepted here, emit `vyne_map_get(struct, ...)`, and the
    // runtime type check would silently return null instead of
    // dispatching to the user's method. Same reasoning as the `size`
    // and `length` gate in tryEmitBuiltinArrayMethod.
    if (receiverIsStruct)
        return std::nullopt;

    const std::string& methodName = node.getMethodName();
    const auto& args = node.getArguments();

    if (methodName == "has") {
        if (args.size() != 1) {
            throw std::runtime_error(
                "Compile Error: has() requires exactly 1 argument (line " +
                std::to_string(node.lineNumber) + ")"
            );
        }
        std::string arg = e.boxAny(args[0]->getCExpr(e));
        std::string temp = e.newTemp("has");
        e.emit(
            "VyneValue " + temp + " = vyne_bool(vyne_map_has(" + recv + ", " +
            arg + "));"
        );
        return temp;
    }

    // m.get(k) -> value or null. Same semantics as m[k] but chains and
    // reads explicitly. Null when the key is absent, when the receiver
    // is not a map, or when k is not a string — all three are the
    // "something didn't resolve" case for a lookup.
    if (methodName == "get") {
        if (args.size() != 1) {
            throw std::runtime_error(
                "Compile Error: get() requires exactly 1 argument (line " +
                std::to_string(node.lineNumber) + ")"
            );
        }
        std::string key = e.boxAny(args[0]->getCExpr(e));
        std::string temp = e.newTemp("mget");
        e.emit(
            "VyneValue " + temp + " = vyne_map_get(" + recv + ", " + key + ");"
        );
        return temp;
    }

    // m.get_or(k, default) -> value if the key is present, default
    // otherwise. Uses `has` rather than a null-value probe, so a map
    // that legitimately stores null under k returns null, not default.
    // Two probes into the same table; correctness first, micro-optimise
    // later if a profile ever asks for it.
    if (methodName == "get_or") {
        if (args.size() != 2) {
            throw std::runtime_error(
                "Compile Error: get_or() requires exactly 2 arguments (line " +
                std::to_string(node.lineNumber) + ")"
            );
        }
        std::string key = e.boxAny(args[0]->getCExpr(e));
        std::string dflt = e.boxAny(args[1]->getCExpr(e));
        std::string temp = e.newTemp("mor");
        e.emit(
            "VyneValue " + temp + " = vyne_map_has(" + recv + ", " + key +
            ") "
            "? vyne_map_get(" +
            recv + ", " + key +
            ") "
            ": " +
            dflt + ";"
        );
        return temp;
    }

    // m.is_empty() -> Bool. Inline type check, not `vyne_get_sizeof`,
    // because the fallback for non-map types returns 8 and would make
    // `is_empty` on a non-map return a plausible-looking false.
    if (methodName == "is_empty") {
        if (!args.empty()) {
            throw std::runtime_error(
                "Compile Error: is_empty() takes no arguments (line " +
                std::to_string(node.lineNumber) + ")"
            );
        }
        std::string temp = e.newTemp("mempty");
        e.emit(
            "VyneValue " + temp + " = vyne_bool(" + recv +
            ".type == V_MAP && " + recv + ".as.map->size == 0);"
        );
        return temp;
    }

    if (methodName == "keys") {
        if (!args.empty()) {
            throw std::runtime_error(
                "Compile Error: keys() takes no arguments (line " +
                std::to_string(node.lineNumber) + ")"
            );
        }
        std::string temp = e.newTemp("keys");
        e.emit("VyneValue " + temp + " = vyne_map_keys(" + recv + ");");
        return temp;
    }

    if (methodName == "values") {
        if (!args.empty()) {
            throw std::runtime_error(
                "Compile Error: values() takes no arguments (line " +
                std::to_string(node.lineNumber) + ")"
            );
        }
        std::string temp = e.newTemp("vals");
        e.emit("VyneValue " + temp + " = vyne_map_values(" + recv + ");");
        return temp;
    }

    if (methodName == "set") {
        if (args.size() != 2) {
            throw std::runtime_error(
                "Compile Error: set() requires exactly 2 arguments (line " +
                std::to_string(node.lineNumber) + ")"
            );
        }
        std::string k = e.boxAny(args[0]->getCExpr(e));
        std::string v = e.boxAny(args[1]->getCExpr(e));
        e.emit("vyne_map_set(" + recv + ", " + k + ", " + v + ");");
        return v;
    }

    return std::nullopt;
}

// ----------------------------------------------------------------
// 10. User-defined struct method (last resort)
// ----------------------------------------------------------------
std::optional<std::string> tryEmitStructMethod(
    C_Emitter& e,
    const MethodCallNode& node,
    const std::string& recv
) {
    const std::string& methodName = node.getMethodName();
    const auto& args = node.getArguments();

    std::string temp = e.newTemp("mret");
    int argSize = (int)args.size();
    std::string argArr = e.newTemp("m_args");

    e.emit("VyneValue " + temp + " = vyne_null();");
    e.emitBlockOpen("if (" + recv + ".type == V_STRUCT) {");
    e.emit(
        "VyneValue* " + argArr +
        " = (VyneValue*)arena_alloc(sizeof(VyneValue) * " +
        std::to_string(argSize + 1) + ");"
    );
    e.emit(argArr + "[0] = " + recv + ";");
    for (int i = 0; i < argSize; ++i) {
        e.emit(
            argArr + "[" + std::to_string(i + 1) +
            "] = " + e.boxAny(args[i]->getCExpr(e)) + ";"
        );
    }
    e.emit(
        temp + " = vyne_struct_call(" + recv + ", \"" + methodName + "\", " +
        std::to_string(argSize + 1) + ", " + argArr + ");"
    );
    e.emitBlockClose();
    return temp;
}

} // namespace

// ============================================================
// ENTRY POINT
// ============================================================

std::string MethodCallNode::getCExpr(C_Emitter& e) const {
    std::string recvPath = receiverPath(receiver.get());

    if (auto r = tryEmitNativeModuleMethod(e, *this, recvPath))
        return *r;
    if (auto r = tryEmitInterfaceConstructor(e, *this, recvPath))
        return *r;
    if (auto r = tryEmitGroupMethod(e, *this, recvPath))
        return *r;

    // The tail: receiver is evaluated exactly once, before every handler
    // below. Argument evaluation inside each handler follows source order.
    std::string recvRaw = receiver->getCExpr(e);

    if (auto r = tryEmitByteAt(e, *this, recvRaw))
        return *r;
    if (auto r = tryEmitTypedArrayMethod(e, *this, recvRaw))
        return *r;

    // From here on the receiver must be boxed. Bind once, share with
    // every subsequent handler.
    std::string recv = e.newTemp("m_recv");
    e.emit("VyneValue " + recv + " = " + e.boxAny(recvRaw) + ";");

    bool receiverIsStruct = receiverNamesStruct(e, receiver.get());

    if (auto r = tryEmitBuiltinArrayMethod(e, *this, recv, receiverIsStruct))
        return *r;
    if (auto r = tryEmitStringMethod(e, *this, recv))
        return *r;
    if (auto r = tryEmitStructFieldList(e, *this, recv))
        return *r;
    if (auto r = tryEmitMapMethod(e, *this, recv, receiverIsStruct))
        return *r;
    if (auto r = tryEmitStructMethod(e, *this, recv))
        return *r;

    return "vyne_null()";
}

void MethodCallNode::compile(C_Emitter& e) const {
    getCExpr(e);
}

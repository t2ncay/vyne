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

// ============================================================================
// Native-ABI dispatch
// ----------------------------------------------------------------------------
// The one place that decides whether a call lowers to its native variant
// (bare `double*` parameters, no boxing, no arena traffic per argument)
// or to the boxed `(int argc, VyneValue* argv)` ABI.
//
// Both FunctionCallNode and MethodCallNode's group branch funnel through
// here. The FunctionCallNode's older inline copy was correct; this
// extraction is behavior-preserving for it and new capability for the
// group branch.
//
// Callers capture their arguments once before invoking this, and reuse
// the same captures if the helper returns nullopt and they fall through
// to the boxed path. No argument is evaluated twice.
// ============================================================================

static std::string nodeInterfaceName(C_Emitter& e, const ASTNode* node) {
    if (!node || node->type() != NodeType::VARIABLE) return "";
    auto* var = static_cast<const VariableNode*>(node);
    return e.lookupStructInterfaceName(var->getOriginalName());
}

std::optional<std::string> tryEmitNativeCall(
    C_Emitter& e,
    const std::string& mangledCallee,
    const std::string& lookupName,
    const std::vector<ASTNode*>& orderedArgs,
    const std::vector<std::string>& argCExprs,
    const std::vector<VType>& argVTypes,
    const std::vector<const CType*>& argCTypes)
{
    const std::string* nativeName = e.lookupNativeVariant(mangledCallee);
    if (!nativeName) return std::nullopt;

    const std::vector<CType>* paramTypes = e.getFunctionParamTypes(lookupName);
    const CType* retCt = e.getFunctionReturnType(lookupName);
    if (!paramTypes || !retCt) return std::nullopt;
    if (paramTypes->size() != orderedArgs.size()) return std::nullopt;

    bool retIsNative = retCt->isPrimitive() ||
                       (retCt->kind == CType::Kind::Array &&
                        !retCt->args.empty());
    if (retCt->kind == CType::Kind::Struct && !retCt->mangledName.empty()) {
        const auto* layout = e.getInterfaceStructLayout(retCt->mangledName);
        if (layout && !layout->empty()) retIsNative = true;
    }
    if (!retIsNative) return std::nullopt;

    // Every argument must match the declared parameter type.
    // Int64 → Float64 widening is permitted (matches the interpreter's
    // convertIfNeeded). Array element types must match exactly.
    std::vector<const std::vector<StructFieldDesc>*> structLayouts(
        orderedArgs.size(), nullptr);

    for (size_t i = 0; i < orderedArgs.size(); ++i) {
        const CType& want = (*paramTypes)[i];

        if (want.kind == CType::Kind::Array && !want.args.empty()) {
            const CType* have = argCTypes[i];
            if (!have || have->kind != CType::Kind::Array ||
                have->args.empty() ||
                have->args[0].toVType() != want.args[0].toVType()) {
                return std::nullopt;
            }
        } else if (want.kind == CType::Kind::Struct) {
            // The argument must be a struct whose static interface
            // type is tracked by the emitter. `nodeInterfaceName` handles
            // the AST-node → original-name unwrapping; the emitter's
            // lookupStructInterfaceName handles name → interface.
            const CType* have = argCTypes[i];
            if (!have || have->kind != CType::Kind::Struct) {
                return std::nullopt;
            }
            // Slice 3b: a native C struct arg is a `vyne_*` value at
            // the C level, not a VyneValue. The field-extraction path
            // below emits `vyne_struct_get` on it, which is a C error.
            // Reject so the caller falls back to the boxed ABI, which
            // re-boxes via boxAny at the call site.
            //
            // Later slices will pass the flat fields directly when the
            // callee's declared interface matches the arg's. That path
            // is not needed for 3b's Point-only scope.
            if (have->nativeCStruct) return std::nullopt;

            const std::string ifaceName =
                nodeInterfaceName(e, orderedArgs[i]);
            if (ifaceName.empty()) return std::nullopt;

            const auto* layout = e.getInterfaceStructLayout(ifaceName);
            if (!layout) return std::nullopt;
            structLayouts[i] = layout;
        } else {
            VType wantVT = want.toVType();
            bool ok = (argVTypes[i] == wantVT) ||
                      (wantVT == VType::Float64 &&
                       (argVTypes[i] == VType::Int64 ||
                        argVTypes[i] == VType::Float32)) ||
                      (wantVT == VType::Float32 &&
                       (argVTypes[i] == VType::Int64 ||
                        argVTypes[i] == VType::Float64));
            if (!ok) return std::nullopt;
        }
    }

    // Build the direct call. Array params pass `.data` (the raw T*);
    // scalars go through coerceToNative which handles the C cast.
    //
    // Struct params expand into one argument per field. Each field
    // extraction is emitted as a statement against `e` first (so that
    // field access doesn't get duplicated inside the C call expression),
    // then the resulting temp is used in the argument list. This
    // preserves evaluation order (source order == argument order) and
    // guarantees each field is read exactly once from the source struct.
    //
    // Array fields are handled differently than in the report: instead
    // of emitting `(double*, int64_t)` — which loses the container's
    // capacity — we materialize the field's boxed VyneValue into a
    // local `VyneArray_f64` / `VyneArray_i64` temp and pass its address.
    // That preserves size, cap, and the shared backing-buffer semantics.
    std::string argList;
    for (size_t i = 0; i < orderedArgs.size(); ++i) {
        if (i > 0) argList += ", ";
        const CType& want = (*paramTypes)[i];

        if (want.kind == CType::Kind::Array && !want.args.empty()) {
            argList += argCExprs[i] + ".data";
        } else if (want.kind == CType::Kind::Struct) {
            // structLayouts[i] was set during validation. It cannot be
            // null here — validation guarantees it.
            const auto* layout = structLayouts[i];
            if (!layout) return std::nullopt;  // defensive

            const std::string& structExpr = argCExprs[i];
            bool first = true;
            for (const auto& fd : *layout) {
                if (!first) argList += ", ";
                first = false;

                // Emit a temp holding the field's boxed value. The
                // receiver is already boxed (structs are always
                // VyneValue in the emitter's tables), so this is a
                // single struct_get.
                std::string fieldBoxed = e.newTemp("sf");
                e.emit("VyneValue " + fieldBoxed +
                       " = vyne_struct_get(" + structExpr +
                       ", " + std::to_string(fd.id) + ");");

                if (fd.type.kind == CType::Kind::Int64) {
                    // Unbox into a native int64_t temp so the ABI sees a
                    // register-sized scalar, not a union read.
                    std::string fv = e.newTemp("sfi");
                    e.emit("int64_t " + fv + " = (" + fieldBoxed +
                           ".type == V_INT64) ? " + fieldBoxed +
                           ".as.i64 : (int64_t)" + fieldBoxed + ".as.f64;");
                    argList += fv;
                } else if (fd.type.kind == CType::Kind::Float64) {
                    std::string fv = e.newTemp("sff");
                    e.emit("double " + fv + " = (" + fieldBoxed +
                           ".type == V_FLOAT64 || " + fieldBoxed +
                           ".type == V_FLOAT32) ? " + fieldBoxed +
                           ".as.f64 : (double)" + fieldBoxed + ".as.i64;");
                    argList += fv;
                } else if (fd.type.kind == CType::Kind::Float32) {
                    std::string fv = e.newTemp("sff32");
                    e.emit("float " + fv + " = (" + fieldBoxed +
                           ".type == V_FLOAT64 || " + fieldBoxed +
                           ".type == V_FLOAT32) ? (float)" + fieldBoxed +
                           ".as.f64 : (float)" + fieldBoxed + ".as.i64;");
                    argList += fv;
                } else if (fd.type.kind == CType::Kind::Array &&
                           !fd.type.args.empty()) {
                    // Materialize the container by value, then pass its
                    // address. `vyne_value_to_array_f64` is O(1) when
                    // the boxed value is already a V_F64_ARRAY (shares
                    // the backing buffer), O(N) otherwise. Either way
                    // the callee gets a valid pointer and the right size.
                    VType felem = fd.type.args[0].toVType();
                    bool isF64 = (felem == VType::Float64);
                    bool isF32 = (felem == VType::Float32);
                    std::string cName = isF64 ? "VyneArray_f64"
                                      : isF32 ? "VyneArray_f32"
                                              : "VyneArray_i64";
                    std::string conv  = isF64 ? "vyne_value_to_array_f64"
                                      : isF32 ? "vyne_value_to_array_f32"
                                              : "vyne_value_to_array_i64";
                    std::string av = e.newTemp("sfa");
                    e.emit(cName + " " + av + " = " + conv + "(" +
                           fieldBoxed + ");");
                    argList += "&" + av;
                } else {
                    // Validation-time rejection should have caught this.
                    // Belt-and-braces: refuse rather than emit garbage.
                    return std::nullopt;
                }
            }
        } else {
            argList += coerceToNative(e, orderedArgs[i], argCExprs[i],
                                      want.toVType());
        }
    }

    if (retCt->kind == CType::Kind::Struct && !retCt->mangledName.empty()) {
        const auto* layout = e.getInterfaceStructLayout(retCt->mangledName);
        if (!layout || layout->empty()) return std::nullopt;

        std::vector<std::string> outVars;
        outVars.reserve(layout->size());
        for (const auto& fd : *layout) {
            std::string ov = e.newTemp("out_" + fd.name);
            outVars.push_back(ov);
            if (fd.type.kind == CType::Kind::Int64) {
                e.emit("int64_t " + ov + " = 0;");
            } else if (fd.type.kind == CType::Kind::Float64) {
                e.emit("double " + ov + " = 0.0;");
            } else if (fd.type.kind == CType::Kind::Array &&
                       !fd.type.args.empty()) {
                VType felem = fd.type.args[0].toVType();
                bool isF64 = (felem == VType::Float64);
                bool isF32 = (felem == VType::Float32);
                e.emit(std::string(isF64 ? "VyneArray_f64 "
                                  : isF32 ? "VyneArray_f32 "
                                          : "VyneArray_i64 ") + ov + ";");
            }
        }

        std::string fullArgs;
        for (const auto& ov : outVars) {
            if (!fullArgs.empty()) fullArgs += ", ";
            fullArgs += "&" + ov;
        }
        if (!argList.empty()) {
            if (!fullArgs.empty()) fullArgs += ", ";
            fullArgs += argList;
        }

        e.emit("fn_" + *nativeName + "(" + fullArgs + ");");

        std::string ctorName = "struct_" + retCt->mangledName;
        std::replace(ctorName.begin(), ctorName.end(), '.', '_');
        std::string ctorArgs;
        for (size_t i = 0; i < layout->size(); ++i) {
            if (i > 0) ctorArgs += ", ";
            const auto& fd = (*layout)[i];
            const std::string& ov = outVars[i];
            if (fd.type.kind == CType::Kind::Int64) {
                ctorArgs += "vyne_int(" + ov + ")";
            } else if (fd.type.kind == CType::Kind::Float64) {
                ctorArgs += "vyne_float(" + ov + ")";
            } else if (fd.type.kind == CType::Kind::Array &&
                       !fd.type.args.empty()) {
                VType felem = fd.type.args[0].toVType();
                bool isF64 = (felem == VType::Float64);
                bool isF32 = (felem == VType::Float32);
                std::string wrap = isF64 ? "vyne_array_f64_to_value"
                                   : isF32 ? "vyne_array_f32_to_value"
                                           : "vyne_array_i64_to_value";
                ctorArgs += wrap + "(&" + ov + ")";
            }
        }

        std::string nret = e.newTemp("nret");
        e.emit("VyneValue " + nret + " = " + ctorName +
               "(" + ctorArgs + ");");
        e.declareNativeTemp(nret, *retCt);
        return nret;
    }

    std::string retCName;
    if (retCt->kind == CType::Kind::Array && !retCt->args.empty()) {
        VType elem = retCt->args[0].toVType();
        retCName = (elem == VType::Float64) ? "VyneArray_f64"
                 : (elem == VType::Float32) ? "VyneArray_f32"
                                            : "VyneArray_i64";
    } else {
        retCName = retCt->cTypeName();
    }

    std::string nret = e.newTemp("nret");
    e.emit(retCName + " " + nret + " = fn_" + *nativeName +
           "(" + argList + ");");
    e.declareNativeTemp(nret, *retCt);
    return nret;
}
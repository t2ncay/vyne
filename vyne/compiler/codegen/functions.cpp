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

// Function declarations and body emission, including parameter and return
// handling. Keep expressions that emit statements in evaluation order; see
// README.md.
// ============================================================
// FUNCTIONS
// ============================================================

static void emitFunctionBody(
    C_Emitter& e,
    const std::vector<Parameter>& parameters,
    const std::vector<std::shared_ptr<ASTNode>>& body,
    const std::string& mangledName
) {
    // Parameters
    for (size_t i = 0; i < parameters.size(); ++i) {
        std::string paramSanitized = parameters[i].name;
        std::replace(paramSanitized.begin(), paramSanitized.end(), '.', '_');
        std::string paramName = "v_" + mangledName + "_" + paramSanitized;

        std::string arg = "args[" + std::to_string(i) + "]";
        std::string guard = "(arg_count > " + std::to_string(i) + ")";

        // M1 (issue #79): unbox known-primitive params into native locals at
        // function entry. The ABI stays `(int, VyneValue*)` — callers still
        // box, and any dynamic value (e.g. a Float64 where Int64 is declared)
        // is coerced here exactly like the old ForNode bound conversion.
        // Non-reference params only; reference params go through the
        // interpreter path and stay boxed.
        CType pt = CType::fromVType(parameters[i].type);
        if (pt.isPrimitive() && !parameters[i].isReference) {
            std::string init = "0";
            switch (pt.kind) {
            case CType::Kind::Int64:
                init = "(" + guard + ") ? ((" + arg + ".type == V_INT64) ? " +
                       arg + ".as.i64 : (int64_t)" + arg + ".as.f64) : 0";
                break;
            case CType::Kind::Float64:
                init = "(" + guard + ") ? ((" + arg + ".type == V_FLOAT64) ? " +
                       arg + ".as.f64 : (double)" + arg + ".as.i64) : 0.0";
                break;
            case CType::Kind::Bool:
                init = "(" + guard + ") ? (" + arg + ".as.i64 != 0) : false";
                break;
            default:
                break;
            }
            e.declareLocal(paramName, pt);
            e.emit(pt.cTypeName() + " " + paramName + " = " + init + ";");
        } else {
            e.registerDeclaration(paramName);
            e.emit(
                "VyneValue " + paramName + " = " + guard + " ? " + arg +
                " : vyne_null();"
            );

            if (parameters[i].type == VType::Struct &&
                !parameters[i].typePath.empty()) {
                e.setLocalStructType(paramName, parameters[i].typePath);
            }
        }
    }

    // Return-value slot (shared by defer and try/catch paths).
    std::string retVar = "__ret_" + mangledName;
    std::string retFlag = "__returning_" + mangledName;
    std::string cleanupLabel = "__cleanup_" + mangledName;

    e.emit("VyneValue " + retVar + " = vyne_null();");
    e.emit("int " + retFlag + " = 0;");

    e.setReturnVars(retVar, retFlag);

    // Top-level defers (unchanged collection logic)
    std::vector<DeferNode*> defers;
    for (const auto& s : body) {
        if (s && s->type() == NodeType::DEFER) {
            auto* d = static_cast<DeferNode*>(s.get());
            d->markCollected();
            defers.push_back(d);
        }
    }

    if (!defers.empty()) {
        e.pushDeferContext(cleanupLabel, retVar);
    }

    for (const auto& stmt : body)
        if (stmt)
            stmt->compile(e);

    if (!defers.empty()) {
        e.emit("goto " + cleanupLabel + ";");

        e.dedent();
        e.emit(cleanupLabel + ":");
        e.indent();

        for (auto it = defers.rbegin(); it != defers.rend(); ++it) {
            (*it)->getBody()->compile(e);
        }

        e.emit("return " + retVar + ";");
        e.popDeferContext();
    } else {
        e.emit("return " + retVar + ";");
    }

    e.clearReturnVars();
}

static void emitNativeFunctionBody(
    C_Emitter& e,
    const std::vector<Parameter>& parameters,
    const std::vector<std::shared_ptr<ASTNode>>& body,
    const std::string& nativeName,
    const CType& returnType
) {
    e.pushFunctionContext();
    e.enterFunction(nativeName); // prefix used for local resolution
    e.setNativeReturnType(returnType);
    e.setInNativeVariant(true);

    // Struct return: build out-param list. The native function becomes
    // `void fn_...(...)` with one out-param per struct field prepended
    // to the parameter list.
    std::vector<std::string> structOutParams;
    const std::vector<StructFieldDesc>* structReturnLayout = nullptr;
    if (returnType.kind == CType::Kind::Struct &&
        !returnType.mangledName.empty()) {
        structReturnLayout = e.getInterfaceStructLayout(returnType.mangledName);
        if (!structReturnLayout || structReturnLayout->empty()) {
            throw std::runtime_error(
                "Native variant returns Struct '" + returnType.mangledName +
                "' but no field layout is registered for it."
            );
        }
        for (const auto& fd : *structReturnLayout) {
            structOutParams.push_back("__ret_" + fd.name);
        }
    }
    e.setStructReturnOutParams(structOutParams);

    std::string retCName;
    if (returnType.kind == CType::Kind::Struct) {
        retCName = "void";
    } else if (
        returnType.kind == CType::Kind::Array && !returnType.args.empty()
    ) {
        VType elem = returnType.args[0].toVType();
        retCName = (elem == VType::Float64) ? "VyneArray_f64" : "VyneArray_i64";
    } else {
        retCName = returnType.cTypeName();
    }

    // Flat parameter list. Struct-typed parameters expand into one C
    // parameter per field, in the same order the caller-side dispatcher
    // iterates. `structFieldParamNames` captures the per-parameter field
    // names so the entry-time reconstruction below can refer to them.
    std::vector<std::vector<std::string>> structFieldParamNames(
        parameters.size()
    );

    std::string paramList;
    if (structReturnLayout) {
        for (size_t i = 0; i < structOutParams.size(); ++i) {
            if (!paramList.empty())
                paramList += ", ";
            const auto& fd = (*structReturnLayout)[i];
            const std::string& op = structOutParams[i];
            if (fd.type.kind == CType::Kind::Int64) {
                paramList += "int64_t* " + op;
            } else if (fd.type.kind == CType::Kind::Float64) {
                paramList += "double* " + op;
            } else if (
                fd.type.kind == CType::Kind::Array && !fd.type.args.empty()
            ) {
                bool isF64 = (fd.type.args[0].toVType() == VType::Float64);
                paramList +=
                    std::string(isF64 ? "VyneArray_f64* " : "VyneArray_i64* ") +
                    op;
            }
        }
    }
    for (size_t i = 0; i < parameters.size(); ++i) {
        if (!paramList.empty())
            paramList += ", ";
        std::string pName = "v_" + nativeName + "_" + parameters[i].name;

        if (parameters[i].type == VType::Array &&
            parameters[i].arrayElemType != VType::Unknown) {
            VType elem = parameters[i].arrayElemType;
            std::string elemC = (elem == VType::Float64) ? "double" : "int64_t";
            paramList += elemC + "* __restrict__ " + pName;
        } else if (parameters[i].type == VType::Struct) {
            const std::string& ifaceName = parameters[i].typePath;
            const auto* layout = e.getInterfaceStructLayout(ifaceName);
            if (!layout) {
                // Registration gate should have prevented this. Guard
                // anyway: a native variant without a resolvable layout
                // cannot be emitted, and silently falling back to a
                // boxed signature here would break the ABI contract
                // with the caller, which emitted a flat call.
                throw std::runtime_error(
                    "Native variant of '" + nativeName +
                    "' has a Struct "
                    "parameter '" +
                    parameters[i].name + "' whose interface '" + ifaceName +
                    "' has no registered field layout. "
                    "This is a compiler bug — report it with the Vyne source."
                );
            }
            bool first = true;
            for (const auto& fd : *layout) {
                if (!first)
                    paramList += ", ";
                first = false;
                std::string fpName = pName + "_" + fd.name;
                structFieldParamNames[i].push_back(fpName);

                if (fd.type.kind == CType::Kind::Int64) {
                    paramList += "int64_t " + fpName;
                } else if (fd.type.kind == CType::Kind::Float64) {
                    paramList += "double " + fpName;
                } else if (
                    fd.type.kind == CType::Kind::Array && !fd.type.args.empty()
                ) {
                    bool isF64 = (fd.type.args[0].toVType() == VType::Float64);
                    paramList +=
                        std::string(
                            isF64 ? "VyneArray_f64* " : "VyneArray_i64* "
                        ) +
                        fpName;
                } else {
                    throw std::runtime_error(
                        "Unsupported struct field type in native variant of '" +
                        nativeName + "': field '" + fd.name + "'"
                    );
                }
            }
        } else {
            CType pt = CType::fromVType(parameters[i].type);
            paramList += pt.cTypeName() + " " + pName;
        }
    }

    e.emitGlobalDecl(retCName + " fn_" + nativeName + "(" + paramList + ");");

    e.emit("// native variant of " + nativeName);
    e.emitBlockOpen(retCName + " fn_" + nativeName + "(" + paramList + ") {");

    for (size_t i = 0; i < parameters.size(); ++i) {
        std::string pName = "v_" + nativeName + "_" + parameters[i].name;
        CType pt = CType::fromVType(parameters[i].type);

        if (parameters[i].type == VType::Array &&
            parameters[i].arrayElemType != VType::Unknown) {
            CType raw;
            raw.kind = CType::Kind::RawArrayPtr;
            raw.args.push_back(CType::fromVType(parameters[i].arrayElemType));
            e.declareLocal(pName, raw);
        } else if (parameters[i].type == VType::Struct) {
            // Reconstruct a boxed VyneValue struct from the flat
            // parameters so the body (which uses vyne_struct_get and
            // vyne_struct_set) compiles unchanged. This is the cost
            // that Design B (native C struct type) eliminates.
            //
            // The result is declared at region depth 0: function
            // parameters conceptually outlive any region opened in the
            // body, so a subsequent `x = arg` inside a region must
            // NOT trip the escape checker just because the parameter
            // was reconstructed here.
            const std::string& ifaceName = parameters[i].typePath;
            const auto* layout = e.getInterfaceStructLayout(ifaceName);
            std::string sVar = pName + "_boxed";
            e.emit(
                "VyneValue " + sVar + " = vyne_struct_create(\"" + ifaceName +
                "\");"
            );
            for (size_t f = 0; f < layout->size(); ++f) {
                const auto& fd = (*layout)[f];
                const std::string& fpName = structFieldParamNames[i][f];
                if (fd.type.kind == CType::Kind::Int64) {
                    e.emit(
                        "vyne_struct_set(" + sVar + ", " +
                        std::to_string(fd.id) + ", \"" + fd.name +
                        "\", vyne_int(" + fpName + "));"
                    );
                } else if (fd.type.kind == CType::Kind::Float64) {
                    e.emit(
                        "vyne_struct_set(" + sVar + ", " +
                        std::to_string(fd.id) + ", \"" + fd.name +
                        "\", vyne_float(" + fpName + "));"
                    );
                } else if (
                    fd.type.kind == CType::Kind::Array && !fd.type.args.empty()
                ) {
                    // fpName is a pointer to a VyneArray_f64 / _i64 on
                    // the caller's stack. Wrap it into a boxed VyneValue
                    // that aliases the same backing buffer.
                    bool isF64 = (fd.type.args[0].toVType() == VType::Float64);
                    std::string wrap = isF64 ? "vyne_array_f64_to_value"
                                             : "vyne_array_i64_to_value";
                    std::string bv = pName + "_f" + std::to_string(f);
                    e.emit(
                        "VyneValue " + bv + " = " + wrap + "(" + fpName + ");"
                    );
                    e.emit(
                        "vyne_struct_set(" + sVar + ", " +
                        std::to_string(fd.id) + ", \"" + fd.name + "\", " + bv +
                        ");"
                    );
                }
            }
            // Declare the boxed struct at depth 0 — parameters are
            // conceptually outside any region in the body.
            e.declareLocal(pName, CType::fromKind(CType::Kind::Struct), 0);
            e.emit("VyneValue " + pName + " = " + sVar + ";");
            // Register the struct's interface type so subsequent member
            // accesses on `pName` resolve via the unboxing fast paths
            // in MemberAccessNode::getCExpr.
            e.setLocalStructType(pName, ifaceName);
        } else {
            e.declareLocal(pName, pt);
        }
    }

    for (const auto& stmt : body)
        if (stmt)
            stmt->compile(e);

    switch (returnType.kind) {
    case CType::Kind::Float64:
        e.emit("return 0.0;");
        break;
    case CType::Kind::Int64:
        e.emit("return 0;");
        break;
    case CType::Kind::Bool:
        e.emit("return false;");
        break;
    case CType::Kind::Struct:
        e.emit("return;");
        break;
    case CType::Kind::Array:
        if (!returnType.args.empty() &&
            returnType.args[0].toVType() == VType::Float64) {
            e.emit("return vyne_array_f64_create(0);");
        } else if (
            !returnType.args.empty() &&
            returnType.args[0].toVType() == VType::Int64
        ) {
            e.emit("return vyne_array_i64_create(0);");
        } else {
            e.emit("VyneArray_f64 __empty = {NULL, 0, 0}; return __empty;");
        }
        break;
    default:
        e.emit("return 0;");
        break;
    }

    e.emitBlockClose();
    e.emit("");
    e.exitFunction();
    e.setInNativeVariant(false);
    e.popFunctionContext();
    e.clearNativeReturnType();
}

void FunctionNode::compile(C_Emitter& e) const {
    std::string mangledName = originalName;
    std::replace(mangledName.begin(), mangledName.end(), '.', '_');

    if (!targetModule.empty()) {
        mangledName = targetModule + "_" + mangledName;
    }

    // Record this function under its module and set the active module
    // while its body is being emitted. Saved so nested compilation
    // (a group inside a group, an interface method) restores cleanly.
    std::string savedModule = e.getActiveModule();
    if (!targetModule.empty()) {
        e.registerModuleFunction(targetModule, originalName);
        e.setActiveModule(targetModule);
    }

    // Register this function's return type so the region escape check
    // can see through calls to it. This fires for EVERY function that
    // ever gets compiled — top-level, group member, aliased import —
    // which makes the check robust against whichever container the
    // function happens to live in. Without it, an interface method or
    // any function that skips GroupNode::compile's FUNCTION branch
    // leaves no entry in functionReturnTypes, and `x = fn(...)` inside
    // a region trips VNE-070 even when fn returns a primitive.
    {
        CType retCt = CType::fromVType(returnType);
        if (returnType == VType::Array &&
            getReturnArrayElemType() != VType::Unknown) {
            retCt.args.push_back(CType::fromVType(getReturnArrayElemType()));
        }
        if (returnType == VType::Struct && !getReturnTypePath().empty()) {
            retCt.mangledName = getReturnTypePath();
        }
        std::string orig = originalName;
        std::string mangled = orig;
        std::replace(mangled.begin(), mangled.end(), '.', '_');

        e.registerFunctionReturnType(orig, retCt);
        e.registerFunctionReturnType(mangled, retCt);

        // Also under the module / group prefix if there is one.
        if (!targetModule.empty()) {
            e.registerFunctionReturnType(targetModule + "_" + mangled, retCt);
            e.registerFunctionReturnType(targetModule + "." + orig, retCt);
        }
    }

    e.emitGlobalDecl(
        "VyneValue fn_" + mangledName + "(int arg_count, VyneValue* args);"
    );
    e.pushFunctionContext();
    e.enterFunction(mangledName);

    e.emit("// fn: " + originalName);
    e.emitBlockOpen(
        "VyneValue fn_" + mangledName + "(int arg_count, VyneValue* args) {"
    );

    // Publish the source-level return type so ReturnNode::compile can
    // resolve `return [];` when the declared type is `Array<T>`.
    {
        CType retCt = CType::fromVType(returnType);
        if (returnType == VType::Array &&
            getReturnArrayElemType() != VType::Unknown) {
            retCt.args.push_back(CType::fromVType(getReturnArrayElemType()));
        }
        e.setCurrentFunctionReturnType(retCt);
    }

    emitFunctionBody(e, parameters, body, mangledName);

    e.clearCurrentFunctionReturnType();

    e.emitBlockClose();
    e.emit("");

    e.exitFunction();
    e.popFunctionContext();
    e.setActiveModule(savedModule);

    {
        std::string nativeMangled = originalName;
        std::replace(nativeMangled.begin(), nativeMangled.end(), '.', '_');
        if (!targetModule.empty())
            nativeMangled = targetModule + "_" + nativeMangled;

        if (const std::string* nv = e.lookupNativeVariant(nativeMangled)) {
            CType retCt = CType::fromVType(returnType);
            if (returnType == VType::Array &&
                getReturnArrayElemType() != VType::Unknown) {
                retCt.args.push_back(
                    CType::fromVType(getReturnArrayElemType())
                );
            }
            if (returnType == VType::Struct && !getReturnTypePath().empty()) {
                retCt.mangledName = getReturnTypePath();
            }
            emitNativeFunctionBody(e, parameters, body, *nv, retCt);
        }
    }
}

void FunctionNode::compileAs(
    C_Emitter& e,
    const std::string& mangledName
) const {
    std::string name = mangledName;
    std::replace(name.begin(), name.end(), '.', '_');

    // Register under the two names we know for sure. The dotted
    // alias-form (`vlin.cross_entropy`) is registered by the caller
    // (ImportNode::compile), because only the caller knows the alias.
    {
        CType retCt = CType::fromVType(returnType);
        if (returnType == VType::Array &&
            getReturnArrayElemType() != VType::Unknown) {
            retCt.args.push_back(CType::fromVType(getReturnArrayElemType()));
        }
        e.registerFunctionReturnType(name, retCt);
        e.registerFunctionReturnType(originalName, retCt);
    }

    e.emitGlobalDecl(
        "VyneValue fn_" + name + "(int arg_count, VyneValue* args);"
    );
    e.pushFunctionContext();
    e.enterFunction(name);
    e.emit("// fn (aliased): " + mangledName);
    e.emitBlockOpen(
        "VyneValue fn_" + name + "(int arg_count, VyneValue* args) {"
    );

    {
        CType retCt = CType::fromVType(returnType);
        if (returnType == VType::Array &&
            getReturnArrayElemType() != VType::Unknown) {
            retCt.args.push_back(CType::fromVType(getReturnArrayElemType()));
        }
        e.setCurrentFunctionReturnType(retCt);
    }

    emitFunctionBody(e, parameters, body, name);

    e.clearCurrentFunctionReturnType();

    e.emitBlockClose();
    e.emit("");

    e.exitFunction();
    e.popFunctionContext();
}

std::string FunctionNode::getCExpr(C_Emitter& e) const {
    compile(e);
    std::string name = originalName;
    std::replace(name.begin(), name.end(), '.', '_');
    if (!targetModule.empty()) {
        name = targetModule + "_" + name;
    }
    return "fn_" + name;
}

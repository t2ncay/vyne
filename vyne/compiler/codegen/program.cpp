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

// Program and block emission, plus ternary expressions.
// Keep expressions that emit statements in evaluation order; see README.md.
// ============================================================
// PROGRAM / BLOCK
// ============================================================

void ProgramNode::compile(C_Emitter& e) const {
    // --- Slice 3g: fixed-point C-eligibility -------------------------
    // An interface is C-eligible iff every field is a primitive scalar,
    // a String, a Map, or an Array<X> whose element X is itself a
    // C-eligible struct. Eligibility is a fixed point: `Molecule` may
    // depend on `Atom`, `Atom` may depend on `Point`, and the source
    // may declare them in any order.
    //
    // This runs before any statement is lowered. InterfaceNode::compile
    // then consults the emitter's set instead of recomputing, so the
    // rule is enforced in one place and once per program.
    //
    // Ordering note: emitting a typedef that references another
    // interface's container still requires source order — see the
    // diagnostic in InterfaceNode::compile. The fixed-point pass makes
    // eligibility order-independent, not emission order-independent.
    {
        bool changed = true;
        while (changed) {
            changed = false;
            for (const auto& stmt : statements) {
                if (!stmt || stmt->type() != NodeType::INTERFACE)
                    continue;
                auto* iface = static_cast<const InterfaceNode*>(stmt.get());

                std::string name = iface->getInterfaceName();
                std::string modName = iface->getModuleName();
                std::string fullName =
                    modName.empty() ? name : (modName + "." + name);

                // isCEligible already does dot-suffix probing, so a hit
                // on either spelling short-circuits the re-check.
                if (e.isCEligible(fullName))
                    continue;

                if (interfaceIsCEligible(e, *iface)) {
                    e.markCEligible(fullName);
                    if (fullName != name)
                        e.markCEligible(name);
                    changed = true;
                }
            }
        }
    }
    // --- end Slice 3g -------------------------------------------------

    for (const auto& stmt : statements) {
        if (stmt && stmt->type() == NodeType::INTERFACE) {
            auto* iface = static_cast<const InterfaceNode*>(stmt.get());
            registerInterfaceLayoutsFromMembers(
                e,
                iface->getInterfaceName(),
                iface->getModuleName(),
                iface->getMembers()
            );
        }
    }

    // Pre-register every function and interface under its module, before
    // any call site is lowered. Without this, a call to a same-module
    // function that is defined *later* in the file resolves to the bare
    // name, because FunctionNode::compile is the only place that
    // registers a function and it runs in source order. `_parse_value`
    // inside `parse` is the case that exposed it: `_skip_ws` is defined
    // above `parse` and resolves correctly, `_parse_value` is defined
    // below and does not.
    for (const auto& stmt : statements) {
        if (!stmt)
            continue;
        if (stmt->type() == NodeType::FUNCTION) {
            auto* fn = static_cast<const FunctionNode*>(stmt.get());
            if (!fn->getTargetModule().empty()) {
                e.registerModuleFunction(
                    fn->getTargetModule(),
                    fn->getOriginalName()
                );
            }
        } else if (stmt->type() == NodeType::INTERFACE) {
            auto* iface = static_cast<const InterfaceNode*>(stmt.get());
            if (!iface->getModuleName().empty()) {
                e.registerModuleInterface(
                    iface->getModuleName(),
                    iface->getInterfaceName()
                );
            }
        }
    }

    for (const auto& stmt : statements) {
        if (stmt && stmt->type() == NodeType::FUNCTION) {
            auto* fn = static_cast<FunctionNode*>(stmt.get());

            std::vector<std::string> paramNames;
            std::vector<CType> paramTypes;
            bool allNativeCallable = true;

            for (const auto& p : fn->getParameters()) {
                paramNames.push_back(p.name);
                CType ct = CType::fromVType(p.type);

                // Carry the element type for Array<Float64> / Array<Int64> so
                // the native dispatcher can match the argument's typed-array
                // CType.
                if (p.type == VType::Array &&
                    p.arrayElemType != VType::Unknown) {
                    ct.args.push_back(CType::fromVType(p.arrayElemType));
                }
                paramTypes.push_back(ct);

                bool paramOK =
                    ct.isPrimitive() ||
                    (ct.kind == CType::Kind::Array && !ct.args.empty());

                // Struct-typed parameter: only native-callable if the
                // interface has a registered field layout. Registration
                // is order-dependent — ProgramNode::compile processes
                // statements in source order, and ImportNode::compile
                // compiles the imported AST before the importing file's
                // statements, so the interface is always registered before
                // any function that mentions it in a parameter.
                //
                // If the layout is missing, allNativeCallable drops to
                // false and the function is emitted only in its boxed
                // form. That is the safe default: the caller side can
                // always fall back to the boxed call path.
                if (!paramOK && p.type == VType::Struct) {
                    if (e.getInterfaceStructLayout(p.typePath)) {
                        paramOK = true;
                    }
                }

                if (!paramOK || p.isReference)
                    allNativeCallable = false;
            }

            CType retCt = CType::fromVType(fn->getReturnType());
            if (fn->getReturnType() == VType::Array &&
                fn->getReturnArrayElemType() != VType::Unknown) {
                retCt.args.push_back(
                    CType::fromVType(fn->getReturnArrayElemType())
                );
            }
            bool retStructHasLayout = false;
            if (fn->getReturnType() == VType::Struct &&
                !fn->getReturnTypePath().empty()) {
                retCt.mangledName = fn->getReturnTypePath();
                const auto* layout =
                    e.getInterfaceStructLayout(retCt.mangledName);
                retStructHasLayout = (layout != nullptr && !layout->empty());
            }
            bool retNative =
                retCt.isPrimitive() ||
                (retCt.kind == CType::Kind::Array && !retCt.args.empty()) ||
                retStructHasLayout;

            // Conservative: reject functions with top-level defer or try/catch.
            bool hasDeferOrTry = false;
            for (const auto& s : fn->getBody()) {
                if (s && (s->type() == NodeType::DEFER ||
                          s->type() == NodeType::TRY_CATCH)) {
                    hasDeferOrTry = true;
                    break;
                }
            }

            e.registerFunctionSignature(
                fn->getOriginalName(),
                std::move(paramNames)
            );
            e.registerFunctionParamTypes(
                fn->getOriginalName(),
                std::move(paramTypes)
            );
            {
                std::string orig = fn->getOriginalName();
                std::string mangled = orig;
                std::replace(mangled.begin(), mangled.end(), '.', '_');
                e.registerFunctionReturnType(orig, retCt);
                e.registerFunctionReturnType(mangled, retCt);
            }

            if (allNativeCallable && retNative && !hasDeferOrTry) {
                std::string orig = fn->getOriginalName();
                std::string mangled = orig;
                std::replace(mangled.begin(), mangled.end(), '.', '_');
                std::string moduleMangled = mangled;
                if (!fn->getTargetModule().empty())
                    moduleMangled = fn->getTargetModule() + "_" + mangled;

                // Primary: the module-qualified name. External callers
                // (`vfft.fft_kernel(...)` from another module) look this up.
                e.registerNativeVariant(
                    moduleMangled,
                    moduleMangled + "_native"
                );

                // Secondary: the bare name. Calls from inside the same
                // module (`fft_kernel(...)` inside forward/inverse) go
                // through FunctionCallNode::getCExpr, which has no prefix
                // at hand and can only query the bare name. Without this,
                // intra-module calls silently fall back to the boxed ABI,
                // producing `fn_vfft_fft_kernel(7, args)` instead of the
                // native call.
                if (moduleMangled != mangled) {
                    e.registerNativeVariant(mangled, moduleMangled + "_native");
                }
            }
        }
    }

    for (const auto& stmt : statements)
        if (stmt)
            stmt->compile(e);
}

void ProgramNode::compileAliased(C_Emitter& e, const std::string& alias) const {
    e.registerGroup(alias);
    e.pushGlobalContext();
    e.emit("// --- import as " + alias + " ---");

    for (const auto& stmt : statements) {
        if (!stmt)
            continue;

        if (stmt->type() == NodeType::FUNCTION) {
            auto* fn = static_cast<FunctionNode*>(stmt.get());
            e.popGlobalContext();
            fn->compileAs(e, alias + "_" + fn->getOriginalName());
            e.pushGlobalContext();
        } else if (stmt->type() == NodeType::ASSIGNMENT) {
            auto* assign = static_cast<AssignmentNode*>(stmt.get());
            std::string mangled =
                "v_" + alias + "_" + assign->getOriginalName();
            std::replace(mangled.begin(), mangled.end(), '.', '_');

            CType declared = CType::fromVType(assign->getExpectedType());

            if (declared.isPrimitive()) {
                e.declareGlobal(mangled, declared);
                e.emitGlobalDecl(
                    declared.cTypeName() + " " + mangled + " = 0;"
                );
            } else {
                e.registerDeclaration(mangled);
                e.emitGlobalDecl("VyneValue " + mangled + ";");
            }

            e.popGlobalContext();
            std::string val = assign->getRHS()->getCExpr(e);
            if (declared.isPrimitive()) {
                std::string init = coerceToNative(
                    e,
                    assign->getRHS(),
                    val,
                    declared.toVType()
                );
                e.emit(mangled + " = " + init + ";");
            } else {
                e.emit(mangled + " = " + e.boxAny(val) + ";");
            }
            e.pushGlobalContext();
        } else if (stmt->type() == NodeType::IMPORT) {
            continue;
        } else {
            e.popGlobalContext();
            stmt->compile(e);
            e.pushGlobalContext();
        }
    }
    e.popGlobalContext();

    std::string modVirtualVar = "v_" + alias;
    std::replace(modVirtualVar.begin(), modVirtualVar.end(), '.', '_');

    if (e.getGlobalVars().count(modVirtualVar) == 0) {
        e.registerDeclaration(modVirtualVar);
        e.emitGlobalDecl("VyneValue " + modVirtualVar + ";");
    }

    e.pushMainContext();
    e.emit("// Virtual Module registration for " + alias);
    e.emitBlockOpen("{");
    std::string tempS = e.newTemp("mod_s");
    e.emit(
        "VyneStruct* " + tempS +
        " = (VyneStruct*)arena_alloc(sizeof(VyneStruct));"
    );
    e.emit(tempS + "->type_name = \"" + alias + "\";");
    e.emit(tempS + "->field_count = 0;");
    e.emit(tempS + "->fields = NULL;");
    e.emit(tempS + "->methods = NULL;");
    e.emit(tempS + "->method_count = 0;");
    e.emit(tempS + "->field_cache[0] = -1;");
    e.emit(tempS + "->field_cache[1] = -1;");
    e.emit(tempS + "->field_cache[2] = -1;");
    e.emit(tempS + "->field_cache[3] = -1;");
    e.emit(
        modVirtualVar + ".type = V_STRUCT; " + modVirtualVar +
        ".as.strct = " + tempS + ";"
    );

    for (const auto& stmt : statements) {
        if (stmt && stmt->type() == NodeType::FUNCTION) {
            auto* fn = static_cast<FunctionNode*>(stmt.get());
            std::string fnOrigName = fn->getOriginalName();
            std::string fnRealCName = "fn_" + alias + "_" + fnOrigName;
            std::replace(fnRealCName.begin(), fnRealCName.end(), '.', '_');

            std::string wrapperName = "wrap_" + alias + "_" + fnOrigName;
            std::replace(wrapperName.begin(), wrapperName.end(), '.', '_');

            e.pushFunctionContext();
            e.emitGlobalDecl(
                "VyneValue " + wrapperName + "(int arg_count, VyneValue* args);"
            );
            e.emitBlockOpen(
                "VyneValue " + wrapperName +
                "(int arg_count, VyneValue* args) {"
            );
            e.emit("if (arg_count > 0) {");
            e.emit("    return " + fnRealCName + "(arg_count - 1, args + 1);");
            e.emit("}");
            e.emit("return " + fnRealCName + "(arg_count, args);");
            e.emitBlockClose();
            e.popFunctionContext();

            e.emit(
                "vyne_register_method(\"" + alias + "\", \"" + fnOrigName +
                "\", " + wrapperName + ");"
            );
        }
    }
    e.emitBlockClose();
    e.popMainContext();
}

std::string ProgramNode::getCExpr(C_Emitter& e) const {
    compile(e);
    return "vyne_null()";
}

void BlockNode::compile(C_Emitter& e) const {
    e.emitBlockOpen("{");
    for (const auto& stmt : statements)
        if (stmt)
            stmt->compile(e);
    e.emitBlockClose();
}

std::string BlockNode::getCExpr(C_Emitter& e) const {
    if (statements.empty())
        return "vyne_null()";

    if (statements.size() == 1) {
        return statements[0]->getCExpr(e);
    }

    for (size_t i = 0; i + 1 < statements.size(); ++i) {
        if (statements[i])
            statements[i]->compile(e);
    }
    return statements.back()->getCExpr(e);
}
// ============================================================
// TERNARY
// ============================================================

std::string TernaryNode::getCExpr(C_Emitter& e) const {
    std::string cond = emitNativeCondition(e, condition.get());
    std::string tVal = e.boxAny(trueExpr->getCExpr(e));
    std::string fVal = e.boxAny(falseExpr->getCExpr(e));
    std::string temp = e.newTemp("tern");
    e.emit(
        "VyneValue " + temp + " = (" + cond + ") ? " + tVal + " : " + fVal + ";"
    );
    return temp;
}

void TernaryNode::compile(C_Emitter& e) const {
    getCExpr(e);
}

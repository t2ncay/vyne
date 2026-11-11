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

// --- AFTER ---
#include "detail/codegen_helpers.h"
#include "autodiff.h"

// Program and block emission, plus ternary expressions.
// Keep expressions that emit statements in evaluation order; see README.md.
// ============================================================
// PROGRAM / BLOCK
// ============================================================

// Native-variant viability gate.
//
// An Array<Float64> / Array<Int64> parameter lowers to a bare
// `double* __restrict__` / `int64_t* __restrict__` in the native ABI
// (see emitNativeFunctionBody: RawArrayPtr). That representation keeps
// the ABI unboxed but drops the length and the container — so *any*
// method call on the parameter (`.size()`, `.length()`, `.push()`,
// `.pop()`, `.sort()`, …) cannot be lowered from a raw pointer alone.
//
// Kernel functions work around this deliberately by taking an explicit
// `n :: Int64` parameter; they never call a method on an array argument.
// Wrapper functions that do call one — `vlin.add_bias` uses
// `if b.size() != c` — cannot be emitted as native variants.
//
// The walk is intentionally conservative. It descends through the AST
// nodes whose sub-nodes expose public accessors (BinOp, Unary,
// Assignment, IndexAccess, If, Block, FunctionCall, MemberAccess). Nodes
// whose sub-tree is not publicly reachable (WhileNode body, ForNode body,
// ReturnNode expression) are skipped. A `.size()` hidden inside one of
// those skipped sub-trees is caught by the defense-in-depth check in
// tryEmitTypedArrayMethod, which throws a clear Compile Error rather
// than emitting invalid C.
static bool callsAnyMethodOnArrayParam(
    const ASTNode* n,
    const std::unordered_set<std::string>& arrayParams
) {
    if (!n)
        return false;

    if (n->type() == NodeType::METHOD_CALL) {
        auto* mc = static_cast<const MethodCallNode*>(n);
        const ASTNode* recv = mc->getReceiver();

        // Only a *direct* variable receiver is the problematic case.
        // `m.data.size()` where `m` is a boxed Matrix parameter routes
        // through vyne_struct_get (returning a VyneValue array), so it
        // still works. But `b.size()` where `b` is a raw-array parameter
        // has no length to read.
        if (recv && recv->type() == NodeType::VARIABLE) {
            auto* vn = static_cast<const VariableNode*>(recv);
            if (arrayParams.count(vn->getOriginalName())) {
                return true;
            }
        }

        // Recurse into receiver (nested calls) and arguments.
        if (callsAnyMethodOnArrayParam(recv, arrayParams))
            return true;
        for (const auto& a : mc->getArguments())
            if (callsAnyMethodOnArrayParam(a.get(), arrayParams))
                return true;
        return false;
    }

    switch (n->type()) {
    case NodeType::BINARY_OP: {
        auto* b = static_cast<const BinOpNode*>(n);
        return callsAnyMethodOnArrayParam(b->leftNode.get(), arrayParams) ||
               callsAnyMethodOnArrayParam(b->rightNode.get(), arrayParams);
    }
    case NodeType::UNARY: {
        auto* u = static_cast<const UnaryNode*>(n);
        return callsAnyMethodOnArrayParam(u->getRight(), arrayParams);
    }
    case NodeType::ASSIGNMENT: {
        auto* a = static_cast<const AssignmentNode*>(n);
        return callsAnyMethodOnArrayParam(a->getRHS(), arrayParams);
    }
    case NodeType::INDEX_ACCESS: {
        auto* ia = static_cast<const IndexAccessNode*>(n);
        return callsAnyMethodOnArrayParam(ia->getBase(), arrayParams) ||
               callsAnyMethodOnArrayParam(ia->getIndex(), arrayParams);
    }
    case NodeType::MEMBER_ACCESS: {
        auto* ma = static_cast<const MemberAccessNode*>(n);
        return callsAnyMethodOnArrayParam(ma->getReceiver(), arrayParams);
    }
    case NodeType::IF: {
        auto* i = static_cast<const IfNode*>(n);
        return callsAnyMethodOnArrayParam(i->condition.get(), arrayParams) ||
               callsAnyMethodOnArrayParam(i->body.get(), arrayParams) ||
               callsAnyMethodOnArrayParam(i->elseBody.get(), arrayParams);
    }
    case NodeType::BLOCK: {
        auto* b = static_cast<const BlockNode*>(n);
        for (const auto& s : b->statements)
            if (callsAnyMethodOnArrayParam(s.get(), arrayParams))
                return true;
        return false;
    }
    case NodeType::FUNCTION_CALL: {
        auto* fc = static_cast<const FunctionCallNode*>(n);
        for (const auto& a : fc->getArguments())
            if (callsAnyMethodOnArrayParam(a.get(), arrayParams))
                return true;
        return false;
    }
    default:
        return false;
    }
}

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

    // Custom adjoint convention: a function named `<fn>_vjp` supplies the
    // reverse pass for `<fn>`. Register all of them before the differentiable
    // pre-pass so a @differentiable function can use its custom VJP even
    // when the pair is declared later in the file.
    for (const auto& stmt : statements) {
        if (!stmt || stmt->type() != NodeType::FUNCTION)
            continue;
        auto* fn = static_cast<const FunctionNode*>(stmt.get());
        const std::string& rawName = fn->getOriginalName();
        const std::string suffix = "_vjp";
        if (rawName.size() <= suffix.size() ||
            rawName.compare(rawName.size() - suffix.size(), suffix.size(), suffix) != 0)
            continue;
        std::string base = rawName.substr(0, rawName.size() - suffix.size());
        std::string mangled = rawName;
        std::replace(mangled.begin(), mangled.end(), '.', '_');
        if (!fn->getTargetModule().empty())
            mangled = fn->getTargetModule() + "_" + mangled;
        std::string cName = "fn_" + mangled;
        e.registerCustomVJP(base, cName);
        if (!fn->getTargetModule().empty())
            e.registerCustomVJP(fn->getTargetModule() + "." + base, cName);
        e.emitGlobalDecl(
            "VyneValue " + cName + "(int arg_count, VyneValue* args);"
        );
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

            // Phase B (AD): register differentiable functions and emit a
            // forward prototype for their companion before any call site
            // is lowered. This makes `grad(f, x)` legal even when `f` is
            // declared later in the file.
            if (fn->isDifferentiable()) {
                std::string mangled = fn->getOriginalName();
                std::replace(mangled.begin(), mangled.end(), '.', '_');
                if (!fn->getTargetModule().empty())
                    mangled = fn->getTargetModule() + "_" + mangled;

                size_t arity = fn->getParameters().size();
                std::vector<VType> paramTypes;
                paramTypes.reserve(arity);
                for (const auto& p : fn->getParameters())
                    paramTypes.push_back(p.type);

                const std::string* custom =
                    e.lookupCustomVJP(fn->getOriginalName());
                if (custom) {
                    e.registerDifferentiable(
                        fn->getOriginalName(), *custom, arity, paramTypes, true
                    );
                    e.registerDifferentiable(
                        mangled, *custom, arity, paramTypes, true
                    );
                    if (!fn->getTargetModule().empty()) {
                        e.registerDifferentiable(
                            fn->getTargetModule() + "." + fn->getOriginalName(),
                            *custom,
                            arity,
                            paramTypes,
                            true
                        );
                    }
                } else {
                    std::string why;
                    if (!vyne_ad::validate(*fn, &why)) {
                        throw std::runtime_error(
                            "Compile Error (VNE-120): @differentiable function '" +
                            fn->getOriginalName() +
                            "' is outside the supported AD slice: " + why +
                            " (line " + std::to_string(fn->lineNumber) + ")."
                        );
                    }
                    std::string adj = vyne_ad::adjointCName(mangled);
                    e.registerDifferentiable(
                        fn->getOriginalName(), adj, arity, paramTypes
                    );
                    e.registerDifferentiable(mangled, adj, arity, paramTypes);
                    if (!fn->getTargetModule().empty()) {
                        e.registerDifferentiable(
                            fn->getTargetModule() + "." + fn->getOriginalName(),
                            adj,
                            arity,
                            paramTypes
                        );
                    }
                    e.emitGlobalDecl(
                        vyne_ad::adjointPrototype(*fn, mangled) + ";"
                    );
                }
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
                if (p.type == VType::Slice &&
                    p.sliceElemType != VType::Unknown) {
                    ct.args.push_back(CType::fromVType(p.sliceElemType));
                }
                paramTypes.push_back(ct);

                bool paramOK =
                    ct.isPrimitive() ||
                    (ct.kind == CType::Kind::Array && !ct.args.empty()) ||
                    (ct.kind == CType::Kind::Slice && !ct.args.empty());

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

            // Native-variant viability: reject functions whose bodies
            // invoke any method on an Array<Float64>/Array<Int64>
            // parameter. Such parameters lower to raw `double*` in the
            // native ABI (no length, no container), so `.size()`,
            // `.length()`, `.push()`, `.pop()`, etc. cannot be lowered
            // from that representation. `vlin.add_bias` is the case that
            // exposed this: `if b.size() != c` reached the boxed-bind
            // path and emitted `VyneValue v = <double-pointer>`.
            //
            // Falling back to the boxed ABI is correct — just slower.
            // Functions that pass an explicit `n :: Int64` (all of the
            // k_* kernels do) have no method calls on their array
            // parameters and remain unaffected.
            bool callsArrayMethodOnParam = false;
            {
                std::unordered_set<std::string> arrayParams;
                for (const auto& p : fn->getParameters()) {
                    if (p.type == VType::Array &&
                        p.arrayElemType != VType::Unknown) {
                        arrayParams.insert(p.name);
                    }
                }
                if (!arrayParams.empty()) {
                    for (const auto& s : fn->getBody()) {
                        if (s &&
                            callsAnyMethodOnArrayParam(s.get(), arrayParams)) {
                            callsArrayMethodOnParam = true;
                            break;
                        }
                    }
                }
            }

            // Array-parameter @differentiable functions are lowered
            // through the boxed primal in this phase. A native variant can
            // box raw `double*` array parameters into calls that expect a
            // VyneValue array (the vstat/vlin kernel surface), which is not
            // a valid ABI. The adjoint is emitted separately and is always
            // native.
            bool adArrayFunction = fn->isDifferentiable();
            if (adArrayFunction) {
                bool hasArray = false;
                for (const auto& p : fn->getParameters())
                    if (p.type == VType::Array)
                        hasArray = true;
                adArrayFunction = hasArray;
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

            if (allNativeCallable && retNative && !hasDeferOrTry &&
                !callsArrayMethodOnParam && !adArrayFunction) {
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

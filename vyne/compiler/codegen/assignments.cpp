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

#include "ctype.h"
#include "detail/codegen_helpers.h"

// Variable reads and assignments, including typed native initialization and
// boxing boundaries. Keep expressions that emit statements in evaluation order;
// see README.md.
// ============================================================
// VARIABLES AND ASSIGNMENTS
// ============================================================

std::string VariableNode::getCExpr(C_Emitter& e) const {
    std::string sanitized = originalName;
    std::replace(sanitized.begin(), sanitized.end(), '.', '_');

    std::string prefix = e.getActiveFunctionPrefix();
    std::string resolved;

    if (!prefix.empty()) {
        std::string localName = "v_" + prefix + "_" + sanitized;
        if (e.isLocalDeclared(localName)) {
            resolved = localName;
        } else {
            resolved = "v_" + sanitized;
        }
    } else {
        resolved = "v_" + sanitized;
    }

    if (e.isReference(resolved)) {
        return "(*" + resolved + ")";
    }
    return resolved;
}

void VariableNode::compile(C_Emitter& e) const {
    // Bare variable reference as a statement is a no-op in C
}

std::string AssignmentNode::getCExpr(C_Emitter& e) const {
    std::string name = "v_" + originalName;
    std::replace(name.begin(), name.end(), '.', '_');
    return name;
}

// M1 (issue #79): build the initializer expression for a native-typed local
// declaration or assignment of static type `declared`.
//
//   - NumberNode literal → raw numeric literal (C implicitly widens int →
//   double)
//   - Existing native expression of the same kind → used directly
//   - Anything else (boxed VyneValue) → runtime-checked coercion into the
//     native type, mirroring the ForNode bound conversion and the
//     interpreter's convertIfNeeded semantics.
static std::string nativeInit(
    C_Emitter& e,
    const ASTNode* rhs,
    const std::string& val,
    const CType& declared
) {
    if (rhs->type() == NodeType::NUMBER) {
        auto* num = static_cast<const NumberNode*>(rhs);
        if ((declared.kind == CType::Kind::Int64 &&
             num->getStaticType() == VType::Int64) ||
            (declared.kind == CType::Kind::Float64)) {
            return num->nativeLiteral();
        }
    }
    if (rhs->type() == NodeType::BOOLEAN &&
        declared.kind == CType::Kind::Bool) {
        return static_cast<const BooleanNode*>(rhs)->nativeLiteral();
    }

    const CType* vt = e.exprNativeType(val);
    if (vt && vt->isPrimitive()) {
        // Native value already in hand → same kind directly, other numeric
        // kind via a C cast (mirrors the interpreter's convertIfNeeded).
        if (vt->kind == declared.kind)
            return val;
        if (declared.kind == CType::Kind::Float64 &&
            vt->kind == CType::Kind::Int64)
            return "(double)(" + val + ")";
        if (declared.kind == CType::Kind::Int64 &&
            vt->kind == CType::Kind::Float64)
            return "(int64_t)(" + val + ")";
        return val;
    }

    switch (declared.kind) {
    case CType::Kind::Int64:
        return "((" + val + ").type == V_INT64) ? (" + val +
               ").as.i64 : (int64_t)(" + val + ").as.f64";
    case CType::Kind::Float64:
        return "((" + val + ").type == V_FLOAT64) ? (" + val +
               ").as.f64 : (double)(" + val + ").as.i64";
    case CType::Kind::Bool:
        return "((" + val + ").as.i64 != 0)";
    default:
        return val;
    }
}

// ============================================================
// Region escape check (Phase 1)
// ------------------------------------------------------------
// Rejects the pattern:
//
//     outer = <value allocated inside the current region>;
//
// where `outer` was declared at a shallower region depth than
// the current one. The value would dangle after the region's
// rewind.
//
// Safe cases:
//   - RHS has a primitive static type (Int64/Float64/Bool/Null):
//     copied by value, no arena involvement.
//   - RHS is a variable declared at the same or shallower depth.
//   - RHS is a variable that has been committed via region.commit.
//   - LHS is a fresh declaration: it dies with the region, no leak.
//
// Unsafe cases (rejected):
//   - Any other assignment where LHS is a live outer variable.
//
// This is deliberately syntactic. §6.4 of the paper names the
// patterns the check cannot see (indirect escape through a
// function return, escape through a container, etc.).
// ============================================================

static void checkRegionEscape(
    C_Emitter& e,
    const std::string& lhsVarName,
    const std::string& lhsDisplayName,
    const ASTNode* rhs,
    int lineNumber
) {
    if (!rhs)
        return;
    if (!e.hasRegion())
        return;

    int lhsDepth = e.lookupLocalRegionDepth(lhsVarName);
    // Globals and variables we can't find are treated as depth 0.
    // A global declared at the top level is at depth 0 by definition.
    if (lhsDepth < 0)
        lhsDepth = 0;

    int curDepth = e.currentRegionDepth();
    if (lhsDepth >= curDepth)
        return; // LHS is as deep or deeper — safe

    // Static string literals lower to `vyne_string_static("…")`, which
    // returns a pointer into .rodata — never the arena. Such a pointer
    // cannot dangle across a rewind, so a `StringNode` RHS is safe by
    // construction. Computed strings (`+`, `string(...)`, interpolated
    // literals) still lower to fresh arena allocations and fall through
    // to the checks below.
    if (rhs->type() == NodeType::STRING) {
        return;
    }

    // Primitive RHS: copied by value, always safe to escape.
    VType st = resolveRHSKind(e, rhs);
    if (st == VType::Int64 || st == VType::Float64 || st == VType::Bool ||
        st == VType::Null) {
        return;
    }

    if (rhs->type() == NodeType::FUNCTION_CALL ||
        rhs->type() == NodeType::METHOD_CALL) {

        std::string name;
        std::string recvPath;

        if (rhs->type() == NodeType::FUNCTION_CALL) {
            name = static_cast<const FunctionCallNode*>(rhs)->getOriginalName();
        } else {
            auto* mc = static_cast<const MethodCallNode*>(rhs);
            name = mc->getMethodName();

            // Receiver path is the group / module name for group calls
            // (`vlin.cross_entropy`), or the dotted path for interfaces
            // (`a.b.Ctor`). Same extraction MethodCallNode::getCExpr uses.
            // NB: MethodCallNode::getReceiver() returns ASTNode* (raw),
            // not a smart pointer — no .get() here.
            const ASTNode* recv = mc->getReceiver();
            if (recv) {
                if (recv->type() == NodeType::VARIABLE) {
                    recvPath = static_cast<const VariableNode*>(recv)
                                   ->getOriginalName();
                } else if (recv->type() == NodeType::MEMBER_ACCESS) {
                    recvPath = static_cast<const MemberAccessNode*>(recv)
                                   ->getFullPath();
                }
            }
        }

        auto primitiveReturn = [&](const std::string& n) -> bool {
            if (n.empty())
                return false;
            const CType* retCt = e.getFunctionReturnType(n);
            return retCt && retCt->isPrimitive();
        };

        // Try every name the callee might be registered under. Bare name
        // covers free functions and interface methods; the mangled form
        // covers dotted names; the receiver-qualified forms cover group
        // methods, which GroupNode::compile registers as `<group>_<method>`
        // (see the fix in groups_modules.cpp).
        std::vector<std::string> candidates;
        candidates.push_back(name);

        std::string mangled = name;
        std::replace(mangled.begin(), mangled.end(), '.', '_');
        if (mangled != name)
            candidates.push_back(mangled);

        if (!recvPath.empty()) {
            std::string qualified = recvPath + "_" + name;
            std::replace(qualified.begin(), qualified.end(), '.', '_');
            candidates.push_back(qualified);

            std::string dotted = recvPath + "." + name;
            candidates.push_back(dotted);
        }

        for (const auto& c : candidates) {
            if (primitiveReturn(c))
                return;
        }

        // ---- Last-resort suffix match ------------------------------
        // The candidate list only covers spellings we anticipated:
        // bare name, name with `.` mangled to `_`, and the receiver
        // path prefix. If the callee lives one level deeper — e.g.
        // `cross_entropy` is an interface method on `Reductions`
        // nested inside `group vlin`, and was registered as
        // `vlin_Reductions_cross_entropy` — none of the three guesses
        // hit. Search the whole table for any key that ends with the
        // method name behind a `_` or `.` boundary, and accept if the
        // matched return type is primitive. We still reject non-
        // primitive matches, so this only ever turns false positives
        // into passes — never a real escape into a false negative.
        for (const auto& kv : e.getAllFunctionReturnTypes()) {
            const std::string& reg = kv.first;
            const CType& rc = kv.second;
            if (reg.size() <= name.size())
                continue;
            size_t prefixLen = reg.size() - name.size();
            if (reg.compare(prefixLen, name.size(), name) != 0)
                continue;
            char sep = reg[prefixLen - 1];
            if (sep != '_' && sep != '.')
                continue;
            if (rc.isPrimitive()) {
                // Found a qualified name ending in <method> whose
                // return type is primitive. That's a proof the
                // callee's value fits in a native register.
                return;
            }
        }
        // ---- end suffix match --------------------------------------
    }

    if (rhsIsRootedOutsideRegion(e, rhs)) {
        return;
    }

    throw std::runtime_error(
        "Escape Error (VNE-070): variable '" + lhsDisplayName +
        "' is declared outside the current region (depth " +
        std::to_string(lhsDepth) +
        ") but is being assigned a value "
        "allocated inside a region (current depth " +
        std::to_string(curDepth) + ") at line " + std::to_string(lineNumber) +
        ".\n"
        "  The value would dangle after the region's rewind.\n"
        "  Fix one of these ways:\n"
        "    - declare '" +
        lhsDisplayName +
        "' inside the region, or\n"
        "    - copy the value out with region.commit(tmp) and assign "
        "from tmp, or\n"
        "    - move the assignment to before the region."
    );
}

// ============================================================
// Shape-typed scratch store
// ------------------------------------------------------------
// Lower `scratch_dst = rhs` into an element-wise copy. Three
// cases, dispatched on the static type of `rhs`:
//
//   1. rhs is another scratch of matching shape and element kind
//        -> memcpy, no per-element work
//   2. rhs is a typed array (VyneArray_f64 / VyneArray_i64) of
//      matching element kind
//        -> element loop, one load and one store per element,
//           no runtime tag check
//   3. rhs is boxed (VyneValue)
//        -> runtime shape check, then per-element tag check and
//           numeric coercion
//
// The destination is a raw C array (double v_x[N] / int64_t
// v_x[N]) declared by ScratchNode::compile. The caller guarantees
// dstType.hasShape() and dstType.args[0] is Float64 or Int64.
// ============================================================

static std::string _shapeStr(const CType& ct) {
    std::string s;
    for (size_t i = 0; i < ct.shape.size(); ++i) {
        if (i)
            s += ", ";
        s += std::to_string(ct.shape[i]);
    }
    return s;
}

static void emitScratchStore(
    C_Emitter& e,
    const std::string& dstName,
    const CType& dstType,
    const ASTNode* rhsNode,
    int lineNumber
) {
    VType elem =
        dstType.args.empty() ? VType::Float64 : dstType.args[0].toVType();
    int64_t n = dstType.numElements();
    const char* elemCName = (elem == VType::Float64) ? "double" : "int64_t";

    std::string src = rhsNode->getCExpr(e);
    const CType* rt = e.lookupType(src);

    // ---- Case 1: scratch-to-scratch, same element kind ----
    if (rt && rt->hasShape() && !rt->args.empty() &&
        rt->args[0].toVType() == elem) {
        if (!rt->sameShape(dstType)) {
            throw std::runtime_error(
                "Shape Error: cannot assign scratch of shape [" +
                _shapeStr(*rt) + "] to scratch of shape [" +
                _shapeStr(dstType) + "] (line " + std::to_string(lineNumber) +
                ")."
            );
        }
        e.emit(
            "memcpy(" + dstName + ", " + src + ", " + std::to_string(n) +
            " * sizeof(" + elemCName + "));"
        );
        return;
    }

    // ---- Case 2: typed array (VyneArray_f64 / _i64) -> scratch ----
    if (rt && rt->kind == CType::Kind::Array && !rt->args.empty() &&
        rt->args[0].toVType() == elem) {
        std::string k = e.newTemp("k");
        e.emitBlockOpen(
            "for (int64_t " + k + " = 0; " + k + " < " + std::to_string(n) +
            "; ++" + k + ") {"
        );
        e.emit(dstName + "[" + k + "] = " + src + ".data[" + k + "];");
        e.emitBlockClose();
        return;
    }

    // ---- Case 3: boxed source ----
    // Shape is a runtime property here. If the src is a scratch or
    // typed array we'd have hit case 1 or 2; anything else is a
    // VyneValue, and we validate at runtime.
    std::string srcVar = e.newTemp("src");
    e.emit("VyneValue " + srcVar + " = " + e.boxAny(src) + ";");
    e.emit(
        "if (" + srcVar + ".type != V_ARRAY || " + srcVar +
        ".as.arr->size != " + std::to_string(n) + ") {"
    );
    e.emit(
        "    fprintf(stderr, \"Runtime error: shape mismatch "
        "assigning to a scratch of " +
        std::to_string(n) + " elements\\n\"); exit(1);"
    );
    e.emit("}");

    std::string k = e.newTemp("k");
    e.emitBlockOpen(
        "for (int64_t " + k + " = 0; " + k + " < " + std::to_string(n) +
        "; ++" + k + ") {"
    );
    std::string cell = srcVar + ".as.arr->elements[" + k + "]";
    if (elem == VType::Float64) {
        e.emit(
            dstName + "[" + k + "] = ((" + cell + ").type == V_FLOAT64) ? (" +
            cell + ").as.f64 : (double)(" + cell + ").as.i64;"
        );
    } else {
        e.emit(
            dstName + "[" + k + "] = ((" + cell + ").type == V_INT64) ? (" +
            cell + ").as.i64 : (int64_t)(" + cell + ").as.f64;"
        );
    }
    e.emitBlockClose();
}

void AssignmentNode::compile(C_Emitter& e) const {
    if (isReference) {
        throw std::runtime_error(
            "Compile Error: Reference variables are not supported by the C "
            "backend "
            "(line " +
            std::to_string(lineNumber) + "). Use the interpreter instead."
        );
    }

    // Slice 3b: unboxing helper. Closes the reverse direction of
    // boxAny. If `valExpr` is already a matching native struct, use it
    // directly. Otherwise it is a boxed VyneValue; bind once, extract
    // each field with vyne_struct_get, and return a compound literal
    // of the native typedef. `ns` must be the registry entry for the
    // target interface.
    auto unboxToNative = [&](const std::string& valExpr,
                             const CType& target,
                             const auto* ns) -> std::string {
        const CType* vt = e.lookupType(valExpr);
        if (vt && vt->kind == CType::Kind::Struct && vt->nativeCStruct &&
            vt->mangledName == target.mangledName) {
            return valExpr;
        }
        std::string boxed = e.newTemp("sv");
        e.emit("VyneValue " + boxed + " = " + e.boxAny(valExpr) + ";");

        std::string init = "((" + ns->tag + "){";
        for (size_t i = 0; i < ns->fieldNames.size(); ++i) {
            if (i)
                init += ", ";
            init += "." + ns->fieldNames[i] + " = ";
            uint32_t fid = StringPool::intern(ns->fieldNames[i]);
            std::string cell = e.newTemp("cf");
            e.emit(
                "VyneValue " + cell + " = vyne_struct_get(" + boxed + ", " +
                std::to_string(fid) + ");"
            );
            CType ft = ns->fieldTypes[i];
            if (ft.kind == CType::Kind::Float64) {
                init += "(" + cell + ".type == V_FLOAT64) ? " + cell +
                        ".as.f64 : (double)" + cell + ".as.i64";
            } else if (ft.kind == CType::Kind::Int64) {
                init += "(" + cell + ".type == V_INT64) ? " + cell +
                        ".as.i64 : (int64_t)" + cell + ".as.f64";
            } else if (ft.kind == CType::Kind::Bool) {
                init += "(" + cell + ".as.i64 != 0)";
            } else if (
                ft.kind == CType::Kind::Str || ft.kind == CType::Kind::Map
            ) {
                // The native struct stores String and Map fields as a
                // bare VyneValue (see cFieldTypeFor in interfaces.cpp).
                // The boxed cell is exactly that, so pass it through.
                init += cell;
            } else if (ft.kind == CType::Kind::Array && !ft.args.empty()) {
                const CType& el = ft.args[0];
                if (el.kind == CType::Kind::Struct && el.nativeCStruct) {
                    throw std::runtime_error(
                        "Compile Error: unboxing a native struct with an "
                        "Array<Struct> field is not implemented yet "
                        "(field '" +
                        ns->fieldNames[i] + "' of '" + target.mangledName +
                        "', line " + std::to_string(lineNumber) +
                        ").\n"
                        "  Assign to the array field element-by-element, "
                        "or construct the struct from a native source."
                    );
                }
                if (el.kind == CType::Kind::Float64) {
                    init += "vyne_value_to_array_f64(" + cell + ")";
                } else if (el.kind == CType::Kind::Int64) {
                    init += "vyne_value_to_array_i64(" + cell + ")";
                } else {
                    init += cell;
                }
            } else {
                throw std::runtime_error(
                    "internal: unboxToNative has no case for CType::Kind " +
                    std::to_string((int)ft.kind) + " on field '" +
                    ns->fieldNames[i] + "' of '" + target.mangledName +
                    "' (line " + std::to_string(lineNumber) + ")."
                );
            }
        }
        init += "})";
        return init;
    };

    // --- Region escape check (Phase 1) --------------------------------
    // Only checks reassignments to already-declared variables. A fresh
    // declaration inside a region dies with the region, no leak.
    {
        std::string sanitized = originalName;
        std::replace(sanitized.begin(), sanitized.end(), '.', '_');
        std::string prefix = e.getActiveFunctionPrefix();
        std::string bareName = "v_" + sanitized;
        std::string localName =
            prefix.empty() ? bareName : ("v_" + prefix + "_" + sanitized);

        bool isExisting =
            e.isLocalDeclared(localName) || e.isGlobalDeclared(bareName);
        bool useGlobalHere = e.getGlobalVars().count(bareName) > 0;

        if (isExisting) {
            std::string target = useGlobalHere ? bareName : localName;
            checkRegionEscape(e, target, originalName, rhs.get(), lineNumber);
        }
    }
    // --- end escape check ---------------------------------------------

    std::string sanitized = originalName;
    std::replace(sanitized.begin(), sanitized.end(), '.', '_');

    std::string prefix = e.getActiveFunctionPrefix();
    std::string bareName = "v_" + sanitized;
    bool hasGlobal = e.getGlobalVars().count(bareName) > 0;

    // Use the parser's explicit "fresh decl" flag, not `expectedType`.
    // The parser sets `expectedType` from the symbol table even for
    // pure reassignments (to satisfy the strict-mode check), so
    // `expectedType != Unknown` can't distinguish `x :: T = ...` from
    // `x = ...` when x already exists. See the comment on
    // AssignmentNode::isFreshDeclaration in ast.h.
    bool isDeclaration = isFreshDeclaration();

    bool useGlobal;
    if (prefix.empty()) {
        useGlobal = hasGlobal || e.isTopLevelOfMain();
    } else if (!isDeclaration && hasGlobal) {
        useGlobal = true;
    } else {
        useGlobal = false;
    }

    std::string varName;
    if (prefix.empty() || useGlobal) {
        varName = bareName;
    } else {
        varName = "v_" + prefix + "_" + sanitized;
    }

    if (!declaredTypeName.empty() && expectedType == VType::Struct) {
        if (useGlobal)
            e.setGlobalStructType(bareName, declaredTypeName);
        else
            e.setLocalStructType(varName, declaredTypeName);
    }

    if (useGlobal) {
        CType declared = CType::fromVType(expectedType);

        if (isDeclaration && declared.kind == CType::Kind::Array) {
            // Element type source order: literal inference, then the declared
            // Array<T> annotation on the assignment target.
            VType elem = inferArrayElemType(rhs.get());
            if (elem == VType::Unknown) {
                elem = getArrayElemType();
            }

            // Struct-element arrays need the vyne_Array_vyne_<tag>
            // container, not the f64/i64 ctor chosen below. Skip the
            // primitive fast path so the Slice 3e-global handler further
            // down (or the boxed fallback) owns the declaration.
            if (elem != VType::Unknown && elem != VType::Struct) {
                // Fresh typed-array allocation from an empty literal:
                //   x :: Array<Float64> = [];
                // lowers to `vyne_array_f64_create(0)`
                if (rhs->type() == NodeType::ARRAY) {
                    auto* arrRhs = static_cast<const ArrayNode*>(rhs.get());
                    if (arrRhs->getElements().empty()) {
                        std::string ctor = (elem == VType::Float64)
                                               ? "vyne_array_f64_create"
                                               : "vyne_array_i64_create";
                        CType arrType;
                        arrType.kind = CType::Kind::Array;
                        arrType.args.push_back(CType::fromVType(elem));
                        e.declareGlobal(varName, arrType);
                        e.emitGlobalDecl(
                            CType::arrayContainerName(elem) + " " + varName +
                            ";"
                        );
                        e.emit(varName + " = " + ctor + "(0);");
                        return;
                    }
                }

                std::string val = rhs->getCExpr(e);
                const CType* rt = e.lookupType(val);
                if (rt && rt->kind == CType::Kind::Array && !rt->args.empty() &&
                    rt->args[0].toVType() == elem) {
                    CType arrType;
                    arrType.kind = CType::Kind::Array;
                    arrType.args.push_back(CType::fromVType(elem));
                    e.declareGlobal(varName, arrType);
                    e.emitGlobalDecl(
                        CType::arrayContainerName(elem) + " " + varName + ";"
                    );
                    e.emit(varName + " = " + val + ";");
                    return;
                }

                // RHS didn't produce a matching typed array — box it.
                e.registerDeclaration(varName);
                e.emit("VyneValue " + varName + " = " + e.boxAny(val) + ";");
                return;
            }

            // Slice 3e-global: struct-element array. `inferArrayElemType`
            // only recognizes primitives, so `Array<Point>` lands here with
            // elem == Unknown. The primitive block above did NOT evaluate
            // the RHS, so this is the first and only evaluation. Look at
            // the RHS's actual CType — the ArrayNode already built the
            // native container — and declare the global at that type so
            // subsequent index and field reads stay native.
            {
                std::string val = rhs->getCExpr(e);
                const CType* rt = e.lookupType(val);
                if (rt && rt->kind == CType::Kind::Array && !rt->args.empty()) {
                    const CType& elemCt = rt->args[0];
                    if (elemCt.kind == CType::Kind::Struct &&
                        elemCt.nativeCStruct) {
                        const auto* ns = e.getNativeCStruct(elemCt.mangledName);
                        if (ns) {
                            CType arrType = *rt;
                            e.declareGlobal(bareName, arrType);
                            e.emitGlobalDecl(
                                "vyne_Array_" + ns->tag + " " + bareName + ";"
                            );
                            e.emit(bareName + " = " + val + ";");
                            return;
                        }
                    }
                }
                // Non-struct RHS with an unresolvable annotation. The RHS
                // is already evaluated; declare the global boxed and store,
                // rather than falling through (which would re-evaluate it).
                e.declareGlobal(
                    bareName,
                    CType::fromKind(CType::Kind::Unknown)
                );
                e.emitGlobalDecl("VyneValue " + bareName + ";");
                e.emit(bareName + " = " + e.boxAny(val) + ";");
                return;
            }
        }

        const CType* existing = e.lookupGlobalType(bareName);
        if (existing && existing->kind == CType::Kind::Array &&
            !existing->args.empty()) {
            VType elem = existing->args[0].toVType();

            // Empty array literal on the RHS of a reassignment:
            //   re = [];
            // is the canonical "reset and refill" idiom. It allocates a
            // fresh typed container of the same element type as the
            // destination, no RHS boxing involved.
            if (rhs->type() == NodeType::ARRAY) {
                auto* arrRhs = static_cast<const ArrayNode*>(rhs.get());
                if (arrRhs->getElements().empty()) {
                    std::string ctor = (elem == VType::Float64)
                                           ? "vyne_array_f64_create"
                                           : "vyne_array_i64_create";
                    e.emit(bareName + " = " + ctor + "(0);");
                    return;
                }
            }

            std::string val = rhs->getCExpr(e);
            const CType* rt = e.lookupType(val);
            if (rt && rt->kind == CType::Kind::Array && !rt->args.empty() &&
                rt->args[0].toVType() == elem) {
                e.emit(bareName + " = " + val + ";");
            } else {
                throw std::runtime_error(
                    "Compile Error: cannot reassign typed-array global '" +
                    originalName +
                    "' from a value of a different element type (line " +
                    std::to_string(lineNumber) + ")."
                );
            }
            return;
        }

        if (!hasGlobal) {
            if (isDeclaration && declared.isPrimitive()) {
                e.declareGlobal(bareName, declared);
                e.emitGlobalDecl(
                    declared.cTypeName() + " " + bareName + " = 0;"
                );
            } else {
                // Slice 3b: a fresh global whose declared type names a
                // C-eligible interface stores the native C struct
                // directly instead of a boxed VyneValue. Subsequent
                // member reads lower to `.field` on this variable.
                const auto* ns = (isDeclaration && !declaredTypeName.empty())
                                     ? e.getNativeCStruct(declaredTypeName)
                                     : nullptr;
                if (ns) {
                    CType ct;
                    ct.kind = CType::Kind::Struct;
                    ct.mangledName = declaredTypeName;
                    ct.nativeCStruct = true;
                    e.declareGlobal(bareName, ct);
                    e.emitGlobalDecl(ns->tag + " " + bareName + ";");
                } else {
                    e.declareGlobal(
                        bareName,
                        CType::fromKind(CType::Kind::Unknown)
                    );
                    e.emitGlobalDecl("VyneValue " + bareName + ";");
                }
            }
        }

        const CType* reg = e.lookupGlobalType(bareName);
        if (reg && reg->isPrimitive()) {
            std::string val = rhs->getCExpr(e);
            std::string init = nativeInit(e, rhs.get(), val, *reg);
            e.emit(bareName + " = " + init + ";");
        } else if (
            reg && reg->kind == CType::Kind::Struct && reg->nativeCStruct
        ) {
            // Slice 3b: global target is a native C struct. The RHS
            // may already be native (constructor / same-interface
            // variable) or a boxed VyneValue (native-ABI struct
            // return, e.g. `vml.adam(...)`). `unboxToNative` covers
            // both cases — it passes native through and unboxes
            // boxed field by field.
            const auto* ns = e.getNativeCStruct(reg->mangledName);
            if (!ns) {
                throw std::runtime_error(
                    "internal: nativeCStruct CType without registry entry "
                    "for '" +
                    reg->mangledName + "' (line " + std::to_string(lineNumber) +
                    ")."
                );
            }
            std::string val = rhs->getCExpr(e);
            std::string srcExpr = unboxToNative(val, *reg, ns);
            e.emit(bareName + " = " + srcExpr + ";");
        } else {
            std::string val = rhs->getCExpr(e);
            e.emit(bareName + " = " + e.boxAny(val) + ";");
        }
        return;
    }

    // --- fresh local declaration ---
    if (!e.isLocalDeclared(varName)) {
        CType declared = CType::fromVType(expectedType);

        // Empty array literal fast path stays first — it must NOT call
        // rhs->getCExpr(), which would box the empty literal.
        if (isDeclaration && declared.kind == CType::Kind::Array &&
            rhs->type() == NodeType::ARRAY) {
            auto* arrRhs = static_cast<const ArrayNode*>(rhs.get());
            if (arrRhs->getElements().empty()) {
                // Preferred source: the parser's arrayElemType, set from
                // the `Array<T>` annotation. Fall back to scanning the
                // raw annotation text only when the parser missed it.
                VType elem = getArrayElemType();
                if (elem == VType::Unknown && !declaredTypeName.empty()) {
                    const std::string& dt = declaredTypeName;
                    if (dt.find("Int64") != std::string::npos)
                        elem = VType::Int64;
                    else if (dt.find("Float64") != std::string::npos)
                        elem = VType::Float64;
                }

                // Struct element: the primitive ctor below does not apply.
                // Fall through to the generic path, which will declare the
                // local from the RHS's own CType (or box it).
                if (elem != VType::Unknown && elem != VType::Struct) {
                    std::string ctor = (elem == VType::Float64)
                                           ? "vyne_array_f64_create"
                                           : "vyne_array_i64_create";
                    CType arrType;
                    arrType.kind = CType::Kind::Array;
                    arrType.args.push_back(CType::fromVType(elem));
                    e.declareLocal(varName, arrType);
                    e.emit(
                        CType::arrayContainerName(elem) + " " + varName +
                        " = " + ctor + "(0);"
                    );
                    return;
                }
                // elem == Unknown: bare `Array` annotation. Fall through
                // to the generic path.
            }
        }

        // Emit the RHS once. Its CType tells us what we actually got —
        // a typed array from a native-dispatching call, a primitive, or
        // a boxed VyneValue.
        std::string val = rhs->getCExpr(e);
        const CType* rt = e.lookupType(val);

        // (A) Typed-array RHS: keep the variable typed so downstream
        //     native dispatch sees the right CType. Annotation is
        //     preferred but not required — the RHS's own CType is
        //     authoritative.
        if (rt && rt->kind == CType::Kind::Array && !rt->args.empty()) {
            const CType& elemCt = rt->args[0];

            // Slice 3e+3b-ext: struct-element array. The container's
            // C type is emitted by VYNE_DEFINE_STRUCT_ARRAY; declare
            // the local at that type.
            if (elemCt.kind == CType::Kind::Struct && elemCt.nativeCStruct) {
                const auto* ns = e.getNativeCStruct(elemCt.mangledName);
                if (ns) {
                    CType arrType = *rt;
                    e.declareLocal(varName, arrType);
                    e.emit(
                        "vyne_Array_" + ns->tag + " " + varName + " = " + val +
                        ";"
                    );
                    return;
                }
            }

            VType elem = elemCt.toVType();
            if (elem == VType::Float64 || elem == VType::Int64) {
                CType arrType = *rt;
                e.declareLocal(varName, arrType);
                e.emit(
                    CType::arrayContainerName(elem) + " " + varName + " = " +
                    val + ";"
                );
                return;
            }
        }

        // (A') Slice 3b: fresh local declaration whose annotation
        //      names a C-eligible interface. Declare the local at the
        //      native C type and let `unboxToNative` handle either
        //      RHS form (native already, or boxed VyneValue).
        //
        //      Fires on the *declared* type, not the RHS CType, so
        //      `p :: Point = someBoxedCall()` still gets a native
        //      local — the unboxing happens once at the boundary
        //      instead of on every subsequent field read.
        if (isDeclaration && !declaredTypeName.empty()) {
            const auto* ns = e.getNativeCStruct(declaredTypeName);
            if (ns) {
                CType ct;
                ct.kind = CType::Kind::Struct;
                ct.mangledName = declaredTypeName;
                ct.nativeCStruct = true;

                std::string srcExpr = unboxToNative(val, ct, ns);
                e.declareLocal(varName, ct);
                e.emit(ns->tag + " " + varName + " = " + srcExpr + ";");
                return;
            }
        }

        // (B) Primitive declaration with annotation.
        if (isDeclaration && declared.isPrimitive()) {
            std::string init = nativeInit(e, rhs.get(), val, declared);
            e.declareLocal(varName, declared);
            e.emit(declared.cTypeName() + " " + varName + " = " + init + ";");
            return;
        }

        // (C) Boxed fallback.
        e.registerDeclaration(varName);
        e.emit("VyneValue " + varName + " = " + e.boxAny(val) + ";");
        return;
    }

    // --- reassignment to an existing local ---
    const CType* existing = e.lookupLocalType(varName);

    if (!existing) {
        std::string val = rhs->getCExpr(e);
        const CType* rt = e.lookupType(val);
        if (rt && rt->kind == CType::Kind::Array && !rt->args.empty() &&
            (rt->args[0].toVType() == VType::Float64 ||
             rt->args[0].toVType() == VType::Int64)) {
            e.declareLocal(varName, *rt);
            e.emit(
                CType::arrayContainerName(rt->args[0].toVType()) + " " +
                varName + " = " + val + ";"
            );
            return;
        }
        e.emit(varName + " = " + e.boxAny(val) + ";");
        return;
    }

    if (existing && existing->isPrimitive()) {
        std::string val = rhs->getCExpr(e);
        std::string init = nativeInit(e, rhs.get(), val, *existing);
        e.emit(varName + " = " + init + ";");
        return;
    }

    // --- shape-typed scratch reassignment: rhs is copied element-wise
    //     into the destination's raw C array ---
    if (existing && existing->hasShape()) {
        emitScratchStore(e, varName, *existing, rhs.get(), lineNumber);
        return;
    }

    if (existing && existing->kind == CType::Kind::Array &&
        !existing->args.empty()) {
        VType elem = existing->args[0].toVType();

        // Empty array literal on the RHS of a local reassignment:
        //     x = [];
        // is the "reset and refill" idiom. Reuse the local's declared
        // element type rather than boxing a fresh VyneValue and then
        // failing the element-type match below.
        //
        // The global branch already has this fast path; the local
        // branch did not.
        if (rhs->type() == NodeType::ARRAY) {
            auto* arrRhs = static_cast<const ArrayNode*>(rhs.get());
            if (arrRhs->getElements().empty()) {
                std::string ctor = (elem == VType::Float64)
                                       ? "vyne_array_f64_create"
                                       : "vyne_array_i64_create";
                e.emit(varName + " = " + ctor + "(0);");
                return;
            }
        }

        std::string val = rhs->getCExpr(e);
        const CType* rt = e.lookupType(val);
        if (rt && rt->kind == CType::Kind::Array && !rt->args.empty() &&
            rt->args[0].toVType() == elem) {
            e.emit(varName + " = " + val + ";");
        } else {
            throw std::runtime_error(
                "Compile Error: cannot reassign typed array '" + originalName +
                "' from a value of a different element type (line " +
                std::to_string(lineNumber) + ")."
            );
        }
        return;
    }

    // --- Slice 3b: native C struct local reassignment ---
    if (existing && existing->kind == CType::Kind::Struct &&
        existing->nativeCStruct) {
        const auto* ns = e.getNativeCStruct(existing->mangledName);
        if (!ns) {
            throw std::runtime_error(
                "internal: nativeCStruct CType without registry entry "
                "for '" +
                existing->mangledName + "' (line " +
                std::to_string(lineNumber) + ")."
            );
        }
        std::string val = rhs->getCExpr(e);
        std::string srcExpr = unboxToNative(val, *existing, ns);
        e.emit(varName + " = " + srcExpr + ";");
        return;
    }

    // --- boxed local ---
    std::string val = rhs->getCExpr(e);
    e.emit(varName + " = " + e.boxAny(val) + ";");
}

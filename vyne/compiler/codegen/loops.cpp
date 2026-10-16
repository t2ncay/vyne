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
#include <unordered_set>

// For-loop lowering, numeric bounds, and iteration forms.
// Keep expressions that emit statements in evaluation order; see README.md.
// ============================================================
// FOR NODE
// ============================================================

// M1 (issue #79): build an int64_t bound expression for `through x :: lo..hi`.
//   - Int64 literal      → raw literal
//   - native Int64 local → used directly
//   - native Float64     → truncated
//   - boxed VyneValue    → runtime-coerced exactly like the old path
static std::string
int64Bound(C_Emitter& e, const ASTNode* node, const std::string& expr) {
    if (node && node->type() == NodeType::NUMBER) {
        auto* num = static_cast<const NumberNode*>(node);
        if (num->getStaticType() == VType::Int64)
            return num->nativeLiteral();
    }
    if (const CType* ct = e.exprNativeType(expr)) {
        if (ct->kind == CType::Kind::Int64)
            return expr;
        if (ct->kind == CType::Kind::Float64)
            return "(int64_t)(" + expr + ")";
    }
    return "((" + expr + ".type == V_INT64) ? " + expr + ".as.i64 : (int64_t)" +
           expr + ".as.f64)";
}

// ============================================================
// SIMD-safety analysis for the #pragma omp simd hint
// ------------------------------------------------------------
// Emitting `#pragma omp simd` is a programmer assertion to the C
// compiler that the loop has:
//   - no loop-carried dependencies,
//   - no side effects the compiler must preserve in order,
//   - no control flow that can leave the loop mid-iteration.
// A false assertion here is not a compile error — it is UB the C
// compiler may exploit. GCC refuses to vectorize most violations
// today because it cannot see inside the called functions, but the
// pragma is still wrong and will bite on any future inlining or LTO
// change.
//
// We assert the hint only when the AST proves all three properties.
// Returns true iff the loop body is:
//   1. A single IndexAssignmentNode  (or a one-statement block
//      wrapping one).
//   2. The destination index is exactly the loop iterator — a
//      plain VariableNode with the iterator's name. This rules out
//      strided writes like `out[c * R + r] = ...` in vlin_k_transpose.
//   3. The RHS is pure — no function calls (vmath_random_float,
//      vmath_sigmoid_f64, ...), no division / modulo / floor-div
//      (all three emit an `exit(1)` on zero, which is not legal
//      inside a vectorized loop).
// ============================================================

static bool rhsIsPure(const ASTNode* rhs) {
    if (!rhs)
        return true;
    switch (rhs->type()) {
    case NodeType::NUMBER:
    case NodeType::BOOLEAN:
    case NodeType::VARIABLE:
    case NodeType::NULLTYPE:
        return true;

    case NodeType::INDEX_ACCESS: {
        auto* ia = static_cast<const IndexAccessNode*>(rhs);
        return rhsIsPure(ia->getBase()) && rhsIsPure(ia->getIndex());
    }

    // Field read on a struct or a static group. Pure: it lowers to either
    // a struct_get (interpreter semantics: a lookup, no state change) or
    // to a hoisted native temp once the emitter's field cache has been
    // primed by hoistLoopInvariantFieldLoads. Recurse into the receiver so
    // `foo().field` and `arr[i].field` still fail the check.
    case NodeType::MEMBER_ACCESS: {
        auto* ma = static_cast<const MemberAccessNode*>(rhs);
        return rhsIsPure(ma->getReceiver());
    }

    case NodeType::BINARY_OP: {
        auto* bop = static_cast<const BinOpNode*>(rhs);
        // Division, floor-divide, modulo all lower to a runtime
        // `if (rhs == 0) { fprintf(stderr, ...); exit(1); }` guard
        // in the integer and float fast paths. `exit` from a
        // vectorized lane is UB.
        if (bop->op == VTokenType::Division ||
            bop->op == VTokenType::Floor_Divide ||
            bop->op == VTokenType::Modulo) {
            return false;
        }
        return rhsIsPure(bop->leftNode.get()) &&
               rhsIsPure(bop->rightNode.get());
    }

    case NodeType::UNARY: {
        auto* un = static_cast<const UnaryNode*>(rhs);
        return rhsIsPure(un->getRight());
    }

    // FunctionCallNode, MethodCallNode, everything else: not pure.
    // This is the branch that excludes vmath.random_float (RNG
    // state mutation) and the vmath.sigmoid / tanh / relu / clamp
    // family (cross-TU calls GCC cannot vectorize anyway without
    // LTO).
    default:
        return false;
    }
}

static bool
loopBodyIsSimdSafe(const ASTNode* body, const std::string& iteratorName) {
    // 1. Locate the single IndexAssignmentNode.
    const IndexAssignmentNode* ia = nullptr;
    if (body && body->type() == NodeType::BLOCK) {
        auto* blk = static_cast<const BlockNode*>(body);

        // Shape A: single-statement store.
        //   through i :: 0..n-1 -> loop { out[i] = <pure>; };
        if (blk->statements.size() == 1 && blk->statements[0] &&
            blk->statements[0]->type() == NodeType::INDEX_ASSIGNMENT) {
            ia = static_cast<const IndexAssignmentNode*>(
                blk->statements[0].get()
            );
        }
        // Shape B: one leading fresh decl + the store.
        //   through i :: 0..n-1 -> loop {
        //       a :: Float64 = m.data[i];
        //       out.data[i]  = a * (1 - a);
        //   };
        // The decl is a fresh AssignmentNode (marks `:=` in source); its
        // initializer is the only thing that can smuggle in a call, so we
        // check it with the same rhsIsPure walk. The store's RHS may
        // reference `a`, which is a VariableNode — already on the pure
        // whitelist — so it passes transitively.
        else if (
            blk->statements.size() == 2 && blk->statements[0] &&
            blk->statements[1] &&
            blk->statements[0]->type() == NodeType::ASSIGNMENT &&
            blk->statements[1]->type() == NodeType::INDEX_ASSIGNMENT
        ) {
            auto* decl =
                static_cast<const AssignmentNode*>(blk->statements[0].get());
            if (!decl->isFreshDeclaration() || !rhsIsPure(decl->getRHS()))
                return false;
            ia = static_cast<const IndexAssignmentNode*>(
                blk->statements[1].get()
            );
        }
    } else if (body && body->type() == NodeType::INDEX_ASSIGNMENT) {
        ia = static_cast<const IndexAssignmentNode*>(body);
    }
    if (!ia)
        return false;

    // 2. Destination index must be the loop iterator verbatim.
    const ASTNode* idx = ia->getIndex();
    if (!idx || idx->type() != NodeType::VARIABLE)
        return false;
    auto* idxVar = static_cast<const VariableNode*>(idx);
    if (idxVar->getOriginalName() != iteratorName)
        return false;

    // 3. RHS must be pure.
    return rhsIsPure(ia->getRHS());
}

// Hoist loop-invariant field loads.
//
// `through i :: 0..n-1 -> loop { a := m.data[i]; out.data[i] = a * (1-a); }`
// lowers to a per-iteration `vyne_value_to_array_f64(vyne_struct_get(m, id))`
// unless the load is moved above the loop. Only the element index is
// loop-dependent — the field pointer is not. GCC cannot hoist it across
// a cross-TU call, so the emitted C must.
//
// We walk the body once, top-level statements only (no descent into
// nested control flow — those blocks have their own scoping, and any
// field read inside them is not guaranteed to dominate the whole body).
// For each distinct (receiver, field) pair whose receiver is not the
// loop iterator and whose field resolves to a typed array on the
// receiver's interface, we emit a hoisted load and store it in the
// emitter's field cache. `MemberAccessNode::getCExpr` then returns the
// hoisted temp whenever the body reads that field, so the loop body
// becomes a pure array read.
//
// Candidates come in three syntactic forms, all of which mean "load
// the field, then index it":
//   IndexAccess(MemberAccess(var, field), idx)
//   IndexAssignment(MemberAccess(var, field), idx, rhs)
//   Assignment(fresh, rhs = IndexAccess(MemberAccess(var, field), idx))
static void hoistVisit(
    C_Emitter& e,
    const ASTNode* node,
    const std::string& iteratorName,
    std::unordered_set<std::string>& seen
) {
    if (!node)
        return;

    switch (node->type()) {
    case NodeType::MEMBER_ACCESS: {
        auto* ma = static_cast<const MemberAccessNode*>(node);
        const ASTNode* recvNode = ma->getReceiver();
        if (recvNode && recvNode->type() == NodeType::VARIABLE) {
            auto* var = static_cast<const VariableNode*>(recvNode);
            if (var->getOriginalName() != iteratorName) {
                const std::string& recvName = var->getOriginalName();
                const std::string& memberName = ma->getMemberName();
                std::string cacheKey = recvName + "." + memberName;

                if (seen.insert(cacheKey).second &&
                    !e.getFieldCache(cacheKey)) {

                    std::string ifaceName =
                        e.lookupStructInterfaceName(recvName);
                    if (!ifaceName.empty()) {
                        VType elem =
                            e.getInterfaceArrayElem(ifaceName, memberName);
                        if (elem == VType::Int64 || elem == VType::Float64) {
                            std::string sanitized = recvName;
                            std::replace(
                                sanitized.begin(),
                                sanitized.end(),
                                '.',
                                '_'
                            );
                            std::string prefix = e.getActiveFunctionPrefix();
                            std::string recvC;
                            if (prefix.empty()) {
                                recvC = "v_" + sanitized;
                            } else {
                                std::string localName =
                                    "v_" + prefix + "_" + sanitized;
                                recvC = e.isLocalDeclared(localName)
                                            ? localName
                                            : ("v_" + sanitized);
                            }
                            std::string recvBoxed = e.boxAny(recvC);
                            uint32_t fid = StringPool::intern(memberName);
                            std::string temp = e.newTemp("fld");
                            const char* cName = (elem == VType::Float64)
                                                    ? "VyneArray_f64"
                                                    : "VyneArray_i64";
                            const char* fn = (elem == VType::Float64)
                                                 ? "vyne_value_to_array_f64"
                                                 : "vyne_value_to_array_i64";

                            e.emit(
                                std::string(cName) + " " + temp + " = " + fn +
                                "(vyne_struct_get(" + recvBoxed + ", " +
                                std::to_string(fid) + "));"
                            );

                            CType ct;
                            ct.kind = CType::Kind::Array;
                            ct.args.push_back(CType::fromVType(elem));
                            e.declareNativeTemp(temp, ct);
                            e.setFieldCache(cacheKey, temp, ct);
                        }
                    }
                }
            }
        }
        // Recurse into the receiver anyway — nested field reads on
        // another struct may also want hoisting.
        hoistVisit(e, recvNode, iteratorName, seen);
        break;
    }

    case NodeType::BINARY_OP: {
        auto* b = static_cast<const BinOpNode*>(node);
        hoistVisit(e, b->leftNode.get(), iteratorName, seen);
        hoistVisit(e, b->rightNode.get(), iteratorName, seen);
        break;
    }

    case NodeType::UNARY: {
        auto* u = static_cast<const UnaryNode*>(node);
        hoistVisit(e, u->getRight(), iteratorName, seen);
        break;
    }

    case NodeType::INDEX_ACCESS: {
        auto* ia = static_cast<const IndexAccessNode*>(node);
        hoistVisit(e, ia->getBase(), iteratorName, seen);
        hoistVisit(e, ia->getIndex(), iteratorName, seen);
        break;
    }

    case NodeType::INDEX_ASSIGNMENT: {
        auto* ias = static_cast<const IndexAssignmentNode*>(node);
        hoistVisit(e, ias->getBase(), iteratorName, seen);
        hoistVisit(e, ias->getIndex(), iteratorName, seen);
        hoistVisit(e, ias->getRHS(), iteratorName, seen);
        break;
    }

    case NodeType::ASSIGNMENT: {
        auto* a = static_cast<const AssignmentNode*>(node);
        hoistVisit(e, a->getRHS(), iteratorName, seen);
        break;
    }

    case NodeType::METHOD_CALL: {
        auto* mc = static_cast<const MethodCallNode*>(node);
        hoistVisit(e, mc->getReceiver(), iteratorName, seen);
        for (const auto& a : mc->getArguments())
            hoistVisit(e, a.get(), iteratorName, seen);
        break;
    }

    case NodeType::FUNCTION_CALL: {
        auto* fc = static_cast<const FunctionCallNode*>(node);
        for (const auto& a : fc->getArguments())
            hoistVisit(e, a.get(), iteratorName, seen);
        break;
    }

    case NodeType::BLOCK: {
        auto* blk = static_cast<const BlockNode*>(node);
        for (const auto& s : blk->statements)
            hoistVisit(e, s.get(), iteratorName, seen);
        break;
    }

    case NodeType::IF: {
        auto* f = static_cast<const IfNode*>(node);
        hoistVisit(e, f->condition.get(), iteratorName, seen);
        hoistVisit(e, f->body.get(), iteratorName, seen);
        hoistVisit(e, f->elseBody.get(), iteratorName, seen);
        break;
    }

    // Nested loops introduce their own iterator binding; do NOT walk
    // into them. Also stops the walk at WHILE bodies for the same
    // reason.
    case NodeType::FOR:
    case NodeType::WHILE:
        break;

    default:
        break;
    }
}

static void hoistLoopInvariantFieldLoads(
    C_Emitter& e,
    const ASTNode* body,
    const std::string& iteratorName
) {
    if (!body || body->type() != NodeType::BLOCK)
        return;
    std::unordered_set<std::string> seen;
    hoistVisit(e, body, iteratorName, seen);
}

void ForNode::compile(C_Emitter& e) const {
    // -----------------------------------------------------------------
    // Range fast path (unchanged, M1).
    // -----------------------------------------------------------------
    if (iterable->type() == NodeType::RANGE) {
        auto* rng = static_cast<RangeNode*>(iterable.get());
        std::string lo = rng->getLeft()->getCExpr(e);
        std::string hi = rng->getRight()->getCExpr(e);
        std::string loT = e.newTemp("lo_i");
        std::string hiT = e.newTemp("hi_i");
        std::string iv = e.newTemp("i");
        std::string elemVar = "v_" + iteratorName;

        e.emit(
            "int64_t " + loT + " = " + int64Bound(e, rng->getLeft(), lo) + ";"
        );
        e.emit(
            "int64_t " + hiT + " = " + int64Bound(e, rng->getRight(), hi) + ";"
        );

        // Vectorization hint. Fires only when:
        //   1. We are inside a native-variant body (see
        //      C_Emitter::inNativeVariant). The same AST body on the
        //      boxed path lowers to vyne_index_get / vyne_binop /
        //      vyne_index_set — calls with side effects the pragma
        //      implicitly promises the compiler can ignore. Emitting
        //      the hint there is a correctness bug waiting for a
        //      compiler upgrade to expose it.
        //   2. The body is a single IndexAssignmentNode — the shape
        //      every kernel in vlin/Kernels.vy has:
        //      `through i :: 0..n-1 -> loop { out[i] = <expr>; }`.
        //      Everything else (if/else bodies, multi-statement
        //      bodies, accumulator loops) is left alone.
        hoistLoopInvariantFieldLoads(e, body.get(), iteratorName);

        if (e.isInNativeVariant() &&
            loopBodyIsSimdSafe(body.get(), iteratorName)) {
            e.emit("#pragma omp simd");
        }

        e.emitBlockOpen(
            "for (int64_t " + iv + " = " + loT + "; " + iv + " <= " + hiT +
            "; ++" + iv + ") {"
        );
        e.declareLocal(elemVar, CType::fromVType(VType::Int64));
        e.emit("int64_t " + elemVar + " = " + iv + ";");
        if (body)
            body->compile(e);
        e.emitBlockClose();
        return;
    }

    std::string collection = iterable->getCExpr(e);

    {
        const CType* ct = e.lookupType(collection);
        if (ct && ct->kind == CType::Kind::Array && !ct->args.empty()) {
            VType elem = ct->args[0].toVType();
            CType elemType = CType::fromVType(elem);
            std::string iv = e.newTemp("i");
            std::string elemVar = "v_" + iteratorName;

            e.emitBlockOpen(
                "for (int64_t " + iv + " = 0; " + iv + " < " + collection +
                ".size; ++" + iv + ") {"
            );
            e.declareLocal(elemVar, elemType);
            e.emit(
                elemType.cTypeName() + " " + elemVar + " = " + collection +
                ".data[" + iv + "];"
            );
            if (body)
                body->compile(e);
            e.emitBlockClose();
            return;
        }
    }

    // -----------------------------------------------------------------
    // Boxed path (unchanged).
    // -----------------------------------------------------------------
    std::string iTemp = e.newTemp("i");
    std::string sizeTemp = e.newTemp("sz");
    std::string elemVar = "v_" + iteratorName;

    e.emitBlockOpen("if (" + collection + ".type == V_ARRAY) {");
    e.emit("int64_t " + sizeTemp + " = " + collection + ".as.arr->size;");
    e.emitBlockOpen(
        "for (int64_t " + iTemp + " = 0; " + iTemp + " < " + sizeTemp + "; " +
        iTemp + "++) {"
    );
    e.emit(
        "VyneValue " + elemVar + " = vyne_array_get(" + collection +
        ", vyne_int(" + iTemp + "));"
    );
    if (body)
        body->compile(e);
    e.emitBlockClose();
    e.emitBlockClose();
}

std::string ForNode::getCExpr(C_Emitter& e) const {
    if (mode == ForMode::LOOP) {
        compile(e);
        return "vyne_null()";
    }

    // -----------------------------------------------------------------
    // Fast path for `through x :: lo..hi -> collect|filter|every|unique`
    //
    // Same idea as ForNode::compile's fast path, but with the accumulator
    // plumbing that the non-LOOP modes need. The per-mode body emission is
    // duplicated below because the loop header differs (native for-loop vs
    // array indexing); the accumulator semantics are identical.
    // -----------------------------------------------------------------
    if (iterable->type() == NodeType::RANGE) {
        auto* rng = static_cast<RangeNode*>(iterable.get());
        std::string lo = rng->getLeft()->getCExpr(e);
        std::string hi = rng->getRight()->getCExpr(e);
        std::string loT = e.newTemp("lo_i");
        std::string hiT = e.newTemp("hi_i");
        std::string iv = e.newTemp("i");
        std::string elemVar = "v_" + iteratorName;

        e.emit(
            "int64_t " + loT + " = " + int64Bound(e, rng->getLeft(), lo) + ";"
        );
        e.emit(
            "int64_t " + hiT + " = " + int64Bound(e, rng->getRight(), hi) + ";"
        );

        // --- EVERY mode ---
        if (mode == ForMode::EVERY) {
            std::string everyTemp = e.newTemp("every");
            std::string resTemp = e.newTemp("every_res");
            e.emit("bool " + everyTemp + " = true;");
            e.emitBlockOpen(
                "for (int64_t " + iv + " = " + loT + "; " + iv + " <= " + hiT +
                "; ++" + iv + ") {"
            );
            e.declareLocal(elemVar, CType::fromVType(VType::Int64));
            e.emit("int64_t " + elemVar + " = " + iv + ";");
            std::string cond = emitNativeCondition(e, body.get());
            e.emitBlockOpen("if (!(" + cond + ")) {");
            e.emit(everyTemp + " = false;");
            e.emit("break;");
            e.emitBlockClose();
            e.emitBlockClose();
            e.emit("VyneValue " + resTemp + " = vyne_bool(" + everyTemp + ");");
            return resTemp;
        }

        // --- COLLECT / FILTER / UNIQUE ---
        std::string listTemp = e.newTemp("res");
        e.emit("VyneValue " + listTemp + " = vyne_array_create(0);");
        e.emitBlockOpen(
            "for (int64_t " + iv + " = " + loT + "; " + iv + " <= " + hiT +
            "; ++" + iv + ") {"
        );
        e.declareLocal(elemVar, CType::fromVType(VType::Int64));
        e.emit("int64_t " + elemVar + " = " + iv + ";");

        switch (mode) {
        case ForMode::COLLECT: {
            std::string tmp = e.newTemp("coll");
            e.emit("VyneValue " + tmp + " = vyne_null();");
            if (body) {
                std::string result = e.boxAny(body->getCExpr(e));
                e.emit(tmp + " = " + result + ";");
            }
            e.emit("vyne_array_push(" + listTemp + ", " + tmp + ");");
            break;
        }
        case ForMode::FILTER: {
            std::string cond = e.boxAny(body->getCExpr(e));
            e.emitBlockOpen("if (vyne_is_truthy(" + cond + ")) {");
            e.emit(
                "vyne_array_push(" + listTemp + ", " + e.boxAny(elemVar) + ");"
            );
            e.emitBlockClose();
            break;
        }
        case ForMode::UNIQUE: {
            std::string dupCheck = e.newTemp("seen");
            e.emit(
                "bool " + dupCheck + " = vyne_array_contains(" + listTemp +
                ", " + e.boxAny(elemVar) + ");"
            );
            e.emitBlockOpen("if (!" + dupCheck + ") {");
            e.emit(
                "vyne_array_push(" + listTemp + ", " + e.boxAny(elemVar) + ");"
            );
            e.emitBlockClose();
            break;
        }
        default:
            break;
        }

        e.emitBlockClose();
        return listTemp;
    }

    {
        std::string rawCollection = iterable->getCExpr(e);
        const CType* ct = e.lookupType(rawCollection);
        if (ct && ct->kind == CType::Kind::Array && !ct->args.empty()) {
            VType elem = ct->args[0].toVType();
            CType elemType = CType::fromVType(elem);
            VType bodyType = body ? body->getStaticType() : VType::Unknown;

            if (mode == ForMode::COLLECT && bodyType == elem) {
                std::string outName = e.newTemp("cfld");
                std::string ctor = (elem == VType::Float64)
                                       ? "vyne_array_f64_create"
                                       : "vyne_array_i64_create";
                std::string pushFn = (elem == VType::Float64)
                                         ? "vyne_array_f64_push"
                                         : "vyne_array_i64_push";

                e.emit(
                    CType::arrayContainerName(elem) + " " + outName + " = " +
                    ctor + "(" + rawCollection + ".size);"
                );
                e.emit(outName + ".size = 0;");

                std::string iv = e.newTemp("i");
                e.emitBlockOpen(
                    "for (int64_t " + iv + " = 0; " + iv + " < " +
                    rawCollection + ".size; ++" + iv + ") {"
                );
                std::string elemVar = "v_" + iteratorName;
                e.declareLocal(elemVar, elemType);
                e.emit(
                    elemType.cTypeName() + " " + elemVar + " = " +
                    rawCollection + ".data[" + iv + "];"
                );
                std::string rawExpr = body->getCExpr(e);
                std::string native =
                    coerceToNative(e, body.get(), rawExpr, elem);
                e.emit(pushFn + "(&" + outName + ", " + native + ");");
                e.emitBlockClose();

                CType outCT;
                outCT.kind = CType::Kind::Array;
                outCT.args.push_back(elemType);
                e.declareNativeTemp(outName, outCT);
                return outName;
            }

            if (mode == ForMode::EVERY) {
                std::string every = e.newTemp("every");
                std::string res = e.newTemp("every_res");
                e.emit("bool " + every + " = true;");

                std::string iv = e.newTemp("i");
                e.emitBlockOpen(
                    "for (int64_t " + iv + " = 0; " + iv + " < " +
                    rawCollection + ".size; ++" + iv + ") {"
                );
                std::string elemVar = "v_" + iteratorName;
                e.declareLocal(elemVar, elemType);
                e.emit(
                    elemType.cTypeName() + " " + elemVar + " = " +
                    rawCollection + ".data[" + iv + "];"
                );
                std::string cond = emitNativeCondition(e, body.get());
                e.emitBlockOpen("if (!(" + cond + ")) {");
                e.emit(every + " = false;");
                e.emit("break;");
                e.emitBlockClose();
                e.emitBlockClose();
                e.emit("VyneValue " + res + " = vyne_bool(" + every + ");");
                return res;
            }
        }
    }

    // --- Boxed fallback -----------------------------------------------
    // The previous version emitted `if (collection.type == V_ARRAY) {`
    // as a guard. For an `Array<Float64>` or `Array<Int64>` the boxed
    // wrapper is V_F64_ARRAY / V_I64_ARRAY, so the guard was false, the
    // loop body never ran, and the collect returned an empty array.
    // Bind the wrapper once (avoids a fresh allocation per iteration)
    // and use _vyne_array_size, which handles all three representations.
    std::string rawCollection = iterable->getCExpr(e);
    std::string collection = e.newTemp("coll");
    e.emit("VyneValue " + collection + " = " + e.boxAny(rawCollection) + ";");
    std::string iTemp = e.newTemp("i");
    std::string sizeTemp = e.newTemp("sz");
    std::string elemVar = "v_" + iteratorName;

    // --- EVERY mode: short-circuiting boolean AND over elements ---
    if (mode == ForMode::EVERY) {
        std::string everyTemp = e.newTemp("every");
        std::string resTemp = e.newTemp("every_res");
        e.emit("bool " + everyTemp + " = true;");
        e.emit(
            "int64_t " + sizeTemp + " = _vyne_array_size(" + collection + ");"
        );
        e.emitBlockOpen(
            "for (int64_t " + iTemp + " = 0; " + iTemp + " < " + sizeTemp +
            "; " + iTemp + "++) {"
        );
        e.emit(
            "VyneValue " + elemVar + " = vyne_array_get(" + collection +
            ", vyne_int(" + iTemp + "));"
        );
        std::string cond = emitNativeCondition(e, body.get());
        e.emitBlockOpen("if (!(" + cond + ")) {");
        e.emit(everyTemp + " = false;");
        e.emit("break;");
        e.emitBlockClose();
        e.emitBlockClose();
        e.emit("VyneValue " + resTemp + " = vyne_bool(" + everyTemp + ");");
        return resTemp;
    }

    // --- COLLECT / FILTER / UNIQUE mode ---
    std::string listTemp = e.newTemp("res");
    e.emit("VyneValue " + listTemp + " = vyne_array_create(0);");
    e.emit("int64_t " + sizeTemp + " = _vyne_array_size(" + collection + ");");
    e.emitBlockOpen(
        "for (int64_t " + iTemp + " = 0; " + iTemp + " < " + sizeTemp + "; " +
        iTemp + "++) {"
    );
    e.emit(
        "VyneValue " + elemVar + " = vyne_array_get(" + collection +
        ", vyne_int(" + iTemp + "));"
    );

    switch (mode) {
    case ForMode::COLLECT: {
        // Explicit temp pattern: `tmp = null; tmp = <body>; push(tmp);`.
        // Guarantees a value on every path even when the body is an
        // if/else whose branches produce different expressions.
        std::string tmp = e.newTemp("coll");
        e.emit("VyneValue " + tmp + " = vyne_null();");
        if (body) {
            std::string result = e.boxAny(body->getCExpr(e));
            e.emit(tmp + " = " + result + ";");
        }
        e.emit("vyne_array_push(" + listTemp + ", " + tmp + ");");
        break;
    }
    case ForMode::FILTER: {
        std::string cond = e.boxAny(body->getCExpr(e));
        e.emitBlockOpen("if (vyne_is_truthy(" + cond + ")) {");
        e.emit("vyne_array_push(" + listTemp + ", " + e.boxAny(elemVar) + ");");
        e.emitBlockClose();
        break;
    }
    case ForMode::UNIQUE: {
        std::string dupCheck = e.newTemp("seen");
        e.emit(
            "bool " + dupCheck + " = vyne_array_contains(" + listTemp + ", " +
            e.boxAny(elemVar) + ");"
        );
        e.emitBlockOpen("if (!" + dupCheck + ") {");
        e.emit("vyne_array_push(" + listTemp + ", " + e.boxAny(elemVar) + ");");
        e.emitBlockClose();
        break;
    }
    default:
        break;
    }

    e.emitBlockClose();
    return listTemp;
}

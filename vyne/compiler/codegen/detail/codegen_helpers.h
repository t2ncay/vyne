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
#include "../codegen.h"

// Shared, translation-unit-local emission helpers from the original
// codegen.cpp. These helpers do not retain mutable state; per-compilation state
// lives in C_Emitter.
// ============================================================
// LITERALS
// ============================================================

static std::string floatLit(double v) {
    char buf[32];
    auto [p, ec] =
        std::to_chars(buf, buf + sizeof(buf), v, std::chars_format::general);
    if (ec != std::errc{})
        return "0.0";
    return std::string(buf, p);
}

static VType inferArrayElemType(const ASTNode* node) {
    if (!node || node->type() != NodeType::ARRAY)
        return VType::Unknown;
    auto* arr = static_cast<const ArrayNode*>(node);
    const auto& elems = arr->getElements();
    if (elems.empty())
        return VType::Unknown;

    VType common = VType::Unknown;
    for (const auto& el : elems) {
        VType et = el->getStaticType();
        if (et != VType::Int64 && et != VType::Float64)
            return VType::Unknown;
        if (common == VType::Unknown) {
            common = et;
        } else if (common != et) {
            if ((common == VType::Int64 && et == VType::Float64) ||
                (common == VType::Float64 && et == VType::Int64)) {
                common = VType::Float64;
            } else {
                return VType::Unknown;
            }
        }
    }
    return common;
}
// Coerce a C expression string to a native C value of static type `want`.
// Same shape as the `operand` lambda in BinOpNode::getCExpr.
static std::string coerceToNative(
    C_Emitter& e,
    const ASTNode* node,
    const std::string& expr,
    VType want
) {
    if (node && node->type() == NodeType::NUMBER) {
        auto* num = static_cast<const NumberNode*>(node);
        if ((want == VType::Int64 && num->getStaticType() == VType::Int64) ||
            (want == VType::Float64)) {
            return num->nativeLiteral();
        }
    }
    if (node && node->type() == NodeType::BOOLEAN && want == VType::Bool) {
        return static_cast<const BooleanNode*>(node)->nativeLiteral();
    }
    const CType* ct = e.exprNativeType(expr);
    if (ct && ct->isPrimitive()) {
        if (ct->toVType() == want)
            return expr;
        if (want == VType::Float64 && ct->kind == CType::Kind::Int64)
            return "(double)(" + expr + ")";
        if (want == VType::Int64 && ct->kind == CType::Kind::Float64)
            return "(int64_t)(" + expr + ")";
        return expr;
    }
    if (want == VType::Float64)
        return "((" + expr + ").type == V_FLOAT64) ? (" + expr +
               ").as.f64 : (double)(" + expr + ").as.i64";
    if (want == VType::Int64)
        return "((" + expr + ").type == V_INT64) ? (" + expr +
               ").as.i64 : (int64_t)(" + expr + ").as.f64";
    if (want == VType::Bool)
        return "((" + expr + ").as.i64 != 0)";
    return expr;
}

// Resolve the "kind" of an RHS expression without emitting any code.
// The emitter's static type table is authoritative for variables and
// typed-array index reads; the AST is a fallback for literals. BinOps
// are handled syntactically because the AST's own getStaticType() only
// resolves when every leaf is statically typed, which is rare inside a
// region.
//
// Returns VType::Unknown when nothing can prove the kind; callers
// must treat Unknown as unsafe (fail closed).
static VType resolveRHSKind(C_Emitter& e, const ASTNode* rhs) {
    if (!rhs)
        return VType::Null;

    // --- Literals ------------------------------------------------
    switch (rhs->type()) {
    case NodeType::NUMBER:
    case NodeType::BOOLEAN:
        return rhs->getStaticType();
    case NodeType::NULLTYPE:
        return VType::Null;
    default:
        break;
    }

    // --- Variables: emitter's type table is authoritative --------
    if (rhs->type() == NodeType::VARIABLE) {
        auto* var = static_cast<const VariableNode*>(rhs);
        std::string n = var->getOriginalName();
        std::replace(n.begin(), n.end(), '.', '_');
        std::string prefix = e.getActiveFunctionPrefix();
        if (!prefix.empty()) {
            if (const CType* ct = e.lookupType("v_" + prefix + "_" + n))
                return ct->toVType();
        }
        if (const CType* ct = e.lookupType("v_" + n))
            return ct->toVType();
        return VType::Unknown;
    }

    // --- Index reads on typed arrays / scratch -------------------
    if (rhs->type() == NodeType::INDEX_ACCESS) {
        auto* ia = static_cast<const IndexAccessNode*>(rhs);
        const ASTNode* baseNode = ia->getBase();
        if (!baseNode || baseNode->type() != NodeType::VARIABLE)
            return VType::Unknown;
        auto* bv = static_cast<const VariableNode*>(baseNode);
        std::string b = bv->getOriginalName();
        std::replace(b.begin(), b.end(), '.', '_');
        std::string prefix = e.getActiveFunctionPrefix();
        const CType* bct = nullptr;
        if (!prefix.empty())
            bct = e.lookupType("v_" + prefix + "_" + b);
        if (!bct)
            bct = e.lookupType("v_" + b);
        if (!bct)
            return VType::Unknown;
        if (bct->kind == CType::Kind::Array && !bct->args.empty())
            return bct->args[0].toVType();
        if (bct->hasShape() && !bct->args.empty())
            return bct->args[0].toVType();
        return VType::Unknown;
    }

    // --- Binary operations ---------------------------------------
    // `BinOpNode::op` is a public field (see ast.h). We read it
    // directly rather than adding a getOp() accessor.
    if (rhs->type() == NodeType::BINARY_OP) {
        auto* bop = static_cast<const BinOpNode*>(rhs);
        switch (bop->op) {
        case VTokenType::Double_Equals:
        case VTokenType::Not_Equal:
        case VTokenType::Greater:
        case VTokenType::Smaller:
        case VTokenType::Greater_Or_Equal:
        case VTokenType::Smaller_Or_Equal:
        case VTokenType::And:
        case VTokenType::Or:
            return VType::Bool;
        case VTokenType::Substract:
        case VTokenType::Multiply:
        case VTokenType::Division:
        case VTokenType::Floor_Divide:
        case VTokenType::Modulo:
        case VTokenType::Power:
            return VType::Float64;
        case VTokenType::Add: {
            VType lt = resolveRHSKind(e, bop->leftNode.get());
            VType rt = resolveRHSKind(e, bop->rightNode.get());
            bool lNum = (lt == VType::Int64 || lt == VType::Float64);
            bool rNum = (rt == VType::Int64 || rt == VType::Float64);
            if (lNum && rNum) {
                // Int64 + Int64 stays Int64; mixed promotes to Float64.
                // Either is primitive, which is all the escape check
                // needs. Return Int64 for both-Int64 to preserve the
                // more precise type.
                return (lt == VType::Int64 && rt == VType::Int64)
                           ? VType::Int64
                           : VType::Float64;
            }
            return VType::Unknown;
        }
        default:
            return VType::Unknown;
        }
    }

    if (rhs->type() == NodeType::FUNCTION_CALL) {
        auto* fc = static_cast<const FunctionCallNode*>(rhs);
        const std::string& name = fc->getOriginalName();

        // Bare name — covers top-level and interface methods.
        if (const CType* ct = e.getFunctionReturnType(name))
            return ct->toVType();

        // Dotted name → underscore-mangled form, e.g.
        // `alias.fn` → `alias_fn`, registered by
        // ImportNode::compile's alias branch.
        std::string mangled = name;
        std::replace(mangled.begin(), mangled.end(), '.', '_');
        if (mangled != name) {
            if (const CType* ct = e.getFunctionReturnType(mangled))
                return ct->toVType();
        }

        return VType::Unknown;
    }

    if (rhs->type() == NodeType::METHOD_CALL) {
        auto* mc = static_cast<const MethodCallNode*>(rhs);
        const std::string& name = mc->getMethodName();

        // Bare method name.
        if (const CType* ct = e.getFunctionReturnType(name))
            return ct->toVType();

        std::string mangled = name;
        std::replace(mangled.begin(), mangled.end(), '.', '_');
        if (mangled != name) {
            if (const CType* ct = e.getFunctionReturnType(mangled))
                return ct->toVType();
        }

        // Receiver-qualified forms. `vml.cross_entropy(...)` registers
        // as `vml_cross_entropy` (group-method mangling) and
        // `vml.cross_entropy` (dotted form); `a.b.Ctor(...)` on an
        // interface registers as the underscore-mangled interface name
        // plus the method.
        const ASTNode* recv = mc->getReceiver();
        if (recv) {
            std::string recvPath;
            if (recv->type() == NodeType::VARIABLE) {
                recvPath =
                    static_cast<const VariableNode*>(recv)->getOriginalName();
            } else if (recv->type() == NodeType::MEMBER_ACCESS) {
                recvPath =
                    static_cast<const MemberAccessNode*>(recv)->getFullPath();
            }

            if (!recvPath.empty()) {
                std::string qualified = recvPath + "_" + name;
                std::replace(qualified.begin(), qualified.end(), '.', '_');
                if (const CType* ct = e.getFunctionReturnType(qualified))
                    return ct->toVType();

                std::string dotted = recvPath + "." + name;
                if (const CType* ct = e.getFunctionReturnType(dotted))
                    return ct->toVType();
            }
        }

        return VType::Unknown;
    }

    return VType::Unknown;
}

// Emit a C expression usable directly in `if (...)` / `while (...)`.
// Returns a native C boolean expression when the AST proves a
// comparison or logical combination of native scalars; otherwise
// falls back to box + truthiness (identical to the current code).
//
// Contract: on every path the operand getCExpr() calls happen exactly
// once, in source order (left, then right). Mirrors the ordering in
// BinOpNode::getCExpr and never reorders statements.
static std::string emitNativeCondition(C_Emitter& e, const ASTNode* node) {
    if (!node)
        return "false";

    // Boolean literal — already a C `bool`.
    if (node->type() == NodeType::BOOLEAN) {
        return static_cast<const BooleanNode*>(node)->nativeLiteral();
    }

    // Bare native scalar as a condition. `if (x)` with x :: Int64
    // lowers to `(x != 0)`. Matches vyne_is_truthy's semantics for
    // Int64/Float64 exactly (NaN != 0 is true, so NaN stays truthy).
    if (node->type() == NodeType::VARIABLE) {
        VType t = resolveRHSKind(e, node);
        if (t == VType::Int64 || t == VType::Float64) {
            std::string expr = node->getCExpr(e);
            const CType* ct = e.exprNativeType(expr);
            if (ct && ct->toVType() == t)
                return "(" + expr + " != 0)";
        } else if (t == VType::Bool) {
            std::string expr = node->getCExpr(e);
            const CType* ct = e.exprNativeType(expr);
            if (ct && ct->toVType() == t)
                return expr;
        }
    }

    if (node->type() == NodeType::BINARY_OP) {
        auto* bop = static_cast<const BinOpNode*>(node);

        // Logical AND / OR — recurse. Both operands' statements are
        // emitted before the returned C expression is used, so C's
        // short-circuit does not skip any runtime effect (getCExpr
        // hoists calls and side effects to statements). Matches the
        // current vyne_binop behavior for VBOP_AND / VBOP_OR.
        if (bop->op == VTokenType::And || bop->op == VTokenType::Or) {
            std::string l = emitNativeCondition(e, bop->leftNode.get());
            std::string r = emitNativeCondition(e, bop->rightNode.get());
            const char* opStr = (bop->op == VTokenType::And) ? "&&" : "||";
            return "((" + l + ") " + opStr + " (" + r + "))";
        }

        // Comparison of two native scalars.
        switch (bop->op) {
        case VTokenType::Double_Equals:
        case VTokenType::Not_Equal:
        case VTokenType::Greater:
        case VTokenType::Smaller:
        case VTokenType::Greater_Or_Equal:
        case VTokenType::Smaller_Or_Equal: {
            VType lt = resolveRHSKind(e, bop->leftNode.get());
            VType rt = resolveRHSKind(e, bop->rightNode.get());
            bool lNum = (lt == VType::Int64 || lt == VType::Float64);
            bool rNum = (rt == VType::Int64 || rt == VType::Float64);
            if (!lNum || !rNum)
                break;

            VType common = (lt == VType::Float64 || rt == VType::Float64)
                               ? VType::Float64
                               : VType::Int64;

            // Materialize both operands once, left then right.
            std::string l = bop->leftNode->getCExpr(e);
            std::string r = bop->rightNode->getCExpr(e);

            auto toNative = [&](const ASTNode* n,
                                const std::string& s) -> std::string {
                if (n->type() == NodeType::NUMBER) {
                    auto* num = static_cast<const NumberNode*>(n);
                    if (common == VType::Float64 ||
                        num->getStaticType() == VType::Int64) {
                        return num->nativeLiteral();
                    }
                }
                if (const CType* ct = e.exprNativeType(s)) {
                    if (ct->toVType() == common)
                        return s;
                    if (common == VType::Float64 &&
                        ct->kind == CType::Kind::Int64) {
                        return "(double)(" + s + ")";
                    }
                }
                return std::string("(") + s + ").as." +
                       (common == VType::Float64 ? "f64" : "i64");
            };

            std::string lv = toNative(bop->leftNode.get(), l);
            std::string rv = toNative(bop->rightNode.get(), r);
            const char* opStr = "==";
            switch (bop->op) {
            case VTokenType::Double_Equals:
                opStr = "==";
                break;
            case VTokenType::Not_Equal:
                opStr = "!=";
                break;
            case VTokenType::Greater:
                opStr = ">";
                break;
            case VTokenType::Smaller:
                opStr = "<";
                break;
            case VTokenType::Greater_Or_Equal:
                opStr = ">=";
                break;
            case VTokenType::Smaller_Or_Equal:
                opStr = "<=";
                break;
            default:
                break;
            }
            return "(" + lv + " " + opStr + " " + rv + ")";
        }
        default:
            break;
        }
    }

    // Slow path — identical to the current code at every call site.
    std::string cond = e.boxAny(node->getCExpr(e));
    return "vyne_is_truthy(" + cond + ")";
}

// True iff the RHS expression's ultimate base variable is rooted
// outside the currently-open region — either as a global (conceptually
// at depth 0), or as a local declared at a shallower region depth than
// the current one. A read from such a variable produces a value whose
// lifetime is at least as long as the destination's, so storing it
// into an outer container is safe.
//
// Used by the escape checks in collections.cpp (IndexAssignmentNode),
// members.cpp (MemberAssignmentNode), and assignments.cpp
// (checkRegionEscape) to accept patterns like `outer[i] = other[j]`
// where both arrays are declared outside the region but their element
// types are not statically provable.
static bool rhsIsRootedOutsideRegion(C_Emitter& e, const ASTNode* rhs) {
    if (!rhs)
        return false;

    const ASTNode* root = rhs;
    while (root) {
        if (root->type() == NodeType::INDEX_ACCESS) {
            root = static_cast<const IndexAccessNode*>(root)->getBase();
        } else if (root->type() == NodeType::MEMBER_ACCESS) {
            root = static_cast<const MemberAccessNode*>(root)->getReceiver();
        } else {
            break;
        }
    }

    if (!root || root->type() != NodeType::VARIABLE)
        return false;

    auto* rv = static_cast<const VariableNode*>(root);
    std::string rn = rv->getOriginalName();
    std::replace(rn.begin(), rn.end(), '.', '_');

    // A global is conceptually at depth 0.
    if (e.isGlobalDeclared("v_" + rn))
        return true;

    std::string rpfx = e.getActiveFunctionPrefix();
    std::string rlname = rpfx.empty() ? ("v_" + rn) : ("v_" + rpfx + "_" + rn);
    int rd = e.lookupLocalRegionDepth(rlname);
    return rd >= 0 && rd < e.currentRegionDepth();
}

std::optional<std::string> tryEmitBlasCall(
    C_Emitter& e,
    const std::string& recvPath,
    const std::string& methodName,
    const std::vector<std::unique_ptr<ASTNode>>& arguments
);

std::optional<std::string> tryEmitNativeCall(
    C_Emitter& e,
    const std::string& mangledCallee,
    const std::string& lookupName,
    const std::vector<ASTNode*>& orderedArgs,
    const std::vector<std::string>& argCExprs,
    const std::vector<VType>& argVTypes,
    const std::vector<const CType*>& argCTypes
);
void registerInterfaceLayoutsFromMembers(
    C_Emitter& e,
    const std::string& interfaceName,
    const std::string& moduleName,
    const std::vector<InterfaceMember>& members
);

// Slice 3g: predicate shared by the fixed-point pass in
// ProgramNode::compile and the emitter inside InterfaceNode::compile.
bool interfaceIsCEligible(C_Emitter& e, const InterfaceNode& iface);

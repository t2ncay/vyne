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

#include "autodiff.h"
#include "detail/codegen_helpers.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <stdexcept>

namespace vyne_ad {
namespace {

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------

std::string sanitizeName(const std::string& name) {
    std::string out;
    out.reserve(name.size());
    for (char c : name) {
        if (std::isalnum((unsigned char)c))
            out.push_back(c);
        else
            out.push_back('_');
    }
    return out;
}

std::string lastSegment(const std::string& name) {
    auto dot = name.find_last_of('.');
    return dot == std::string::npos ? name : name.substr(dot + 1);
}

std::string valueName(const std::string& gradName, const std::string& varName) {
    return "v_" + gradName + "_" + sanitizeName(varName);
}

std::string deltaName(const std::string& gradName, const std::string& varName) {
    return "d_" + gradName + "_" + sanitizeName(varName);
}

bool isUnaryMath(const std::string& base) {
    return base == "sin" || base == "cos" || base == "exp" || base == "log" ||
           base == "sqrt" || base == "tanh" || base == "abs" || base == "relu";
}

// Combine two additive terms, dropping the provably-zero side.
std::string addTerms(const std::string& a, const std::string& b) {
    if (a == "0.0")
        return b;
    if (b == "0.0")
        return a;
    return "(" + a + " + " + b + ")";
}

std::string mulTerms(const std::string& a, const std::string& b) {
    return "((" + a + ") * (" + b + "))";
}

// ---------------------------------------------------------------------------
// Variable collection / dependency test
// ---------------------------------------------------------------------------

bool collectVars(
    const ASTNode* node,
    std::set<std::string>& out,
    std::string& err
);

bool collectCallArgs(
    const std::string& name,
    const std::vector<std::unique_ptr<ASTNode>>& args,
    std::set<std::string>& out,
    std::string& err
);

bool collectVarsCall(
    const FunctionCallNode* call,
    std::set<std::string>& out,
    std::string& err
) {
    return collectCallArgs(
        call->getOriginalName(),
        call->getArguments(),
        out,
        err
    );
}

bool collectCallArgs(
    const std::string& name,
    const std::vector<std::unique_ptr<ASTNode>>& args,
    std::set<std::string>& out,
    std::string& err
) {
    const std::string base = lastSegment(name);

    if (base == "pow" && args.size() == 2) {
        for (const auto& a : args)
            if (!collectVars(a.get(), out, err))
                return false;
        return true;
    }
    if (isUnaryMath(base) && args.size() == 1) {
        return collectVars(args[0].get(), out, err);
    }

    err = "call to '" + name + "' has no registered adjoint rule";
    return false;
}

bool collectVars(
    const ASTNode* node,
    std::set<std::string>& out,
    std::string& err
) {
    if (!node)
        return true;

    switch (node->type()) {
    case NodeType::NUMBER:
        return true;

    case NodeType::VARIABLE:
        out.insert(static_cast<const VariableNode*>(node)->getOriginalName());
        return true;

    case NodeType::BINARY_OP: {
        auto* b = static_cast<const BinOpNode*>(node);
        return collectVars(b->leftNode.get(), out, err) &&
               collectVars(b->rightNode.get(), out, err);
    }

    case NodeType::UNARY: {
        auto* u = static_cast<const UnaryNode*>(node);
        return collectVars(u->getRight(), out, err);
    }

    case NodeType::FUNCTION_CALL:
        return collectVarsCall(
            static_cast<const FunctionCallNode*>(node),
            out,
            err
        );

    case NodeType::METHOD_CALL: {
        auto* c = static_cast<const MethodCallNode*>(node);
        return collectCallArgs(c->getMethodName(), c->getArguments(), out, err);
    }

    default:
        err = "expression kind is not differentiable yet";
        return false;
    }
}

bool dependsOn(const ASTNode* node, const std::string& var) {
    if (!node)
        return false;
    switch (node->type()) {
    case NodeType::NUMBER:
        return false;
    case NodeType::VARIABLE:
        return static_cast<const VariableNode*>(node)->getOriginalName() == var;
    case NodeType::BINARY_OP: {
        auto* b = static_cast<const BinOpNode*>(node);
        return dependsOn(b->leftNode.get(), var) ||
               dependsOn(b->rightNode.get(), var);
    }
    case NodeType::UNARY:
        return dependsOn(static_cast<const UnaryNode*>(node)->getRight(), var);
    case NodeType::FUNCTION_CALL: {
        auto* c = static_cast<const FunctionCallNode*>(node);
        for (const auto& a : c->getArguments())
            if (dependsOn(a.get(), var))
                return true;
        return false;
    }
    case NodeType::METHOD_CALL: {
        auto* c = static_cast<const MethodCallNode*>(node);
        for (const auto& a : c->getArguments())
            if (dependsOn(a.get(), var))
                return true;
        return false;
    }
    default:
        return true;
    }
}

// ---------------------------------------------------------------------------
// Value expression emission (the primal recomputation in the adjoint)
// ---------------------------------------------------------------------------

std::string
valueExpr(const ASTNode* node, const std::string& gradName, std::string& err);

std::string valueCallArgs(
    const std::string& name,
    const std::vector<std::unique_ptr<ASTNode>>& args,
    const std::string& gradName,
    std::string& err
) {
    const std::string base = lastSegment(name);

    if (base == "pow" && args.size() == 2) {
        std::string l = valueExpr(args[0].get(), gradName, err);
        std::string r = valueExpr(args[1].get(), gradName, err);
        if (!err.empty())
            return "0.0";
        return "pow((" + l + "), (" + r + "))";
    }

    if (isUnaryMath(base) && args.size() == 1) {
        std::string a = valueExpr(args[0].get(), gradName, err);
        if (!err.empty())
            return "0.0";
        if (base == "abs")
            return "fabs((" + a + "))";
        if (base == "relu")
            return "(((" + a + ") > 0.0) ? (" + a + ") : 0.0)";
        return base + "((" + a + "))";
    }

    err = "call to '" + name + "' has no registered adjoint rule";
    return "0.0";
}

std::string valueCall(
    const FunctionCallNode* call,
    const std::string& gradName,
    std::string& err
) {
    return valueCallArgs(
        call->getOriginalName(),
        call->getArguments(),
        gradName,
        err
    );
}

std::string
valueExpr(const ASTNode* node, const std::string& gradName, std::string& err) {
    if (!node) {
        err = "empty expression";
        return "0.0";
    }

    switch (node->type()) {
    case NodeType::NUMBER:
        return static_cast<const NumberNode*>(node)->nativeLiteral();

    case NodeType::VARIABLE:
        return valueName(
            gradName,
            static_cast<const VariableNode*>(node)->getOriginalName()
        );

    case NodeType::BINARY_OP: {
        auto* b = static_cast<const BinOpNode*>(node);
        std::string l = valueExpr(b->leftNode.get(), gradName, err);
        std::string r = valueExpr(b->rightNode.get(), gradName, err);
        if (!err.empty())
            return "0.0";
        switch (b->op) {
        case VTokenType::Add:
            return "((" + l + ") + (" + r + "))";
        case VTokenType::Substract:
            return "((" + l + ") - (" + r + "))";
        case VTokenType::Multiply:
            return "((" + l + ") * (" + r + "))";
        case VTokenType::Division:
            return "((" + l + ") / (" + r + "))";
        case VTokenType::Power:
            return "pow((" + l + "), (" + r + "))";
        default:
            err = "operator is not differentiable yet";
            return "0.0";
        }
    }

    case NodeType::UNARY: {
        auto* u = static_cast<const UnaryNode*>(node);
        std::string v = valueExpr(u->getRight(), gradName, err);
        if (!err.empty())
            return "0.0";
        switch (u->getOp()) {
        case VTokenType::Substract:
            return "(-(" + v + "))";
        case VTokenType::Add:
            return v;
        default:
            err = "unary operator is not differentiable yet";
            return "0.0";
        }
    }

    case NodeType::FUNCTION_CALL:
        return valueCall(
            static_cast<const FunctionCallNode*>(node),
            gradName,
            err
        );

    case NodeType::METHOD_CALL: {
        auto* c = static_cast<const MethodCallNode*>(node);
        return valueCallArgs(
            c->getMethodName(),
            c->getArguments(),
            gradName,
            err
        );
    }

    default:
        err = "expression is not differentiable yet";
        return "0.0";
    }
}

// ---------------------------------------------------------------------------
// Symbolic partial derivatives
// ---------------------------------------------------------------------------

std::string partialExpr(
    const ASTNode* node,
    const std::string& wrt,
    const std::string& gradName,
    std::string& err
);

std::string partialCallArgs(
    const std::string& name,
    const std::vector<std::unique_ptr<ASTNode>>& args,
    const std::string& wrt,
    const std::string& gradName,
    std::string& err
) {
    const std::string base = lastSegment(name);

    if (base == "pow" && args.size() == 2) {
        const ASTNode* a = args[0].get();
        const ASTNode* b = args[1].get();
        if (!dependsOn(a, wrt) && !dependsOn(b, wrt))
            return "0.0";

        std::string L = valueExpr(a, gradName, err);
        std::string R = valueExpr(b, gradName, err);
        std::string dL = partialExpr(a, wrt, gradName, err);
        std::string dR = partialExpr(b, wrt, gradName, err);
        if (!err.empty())
            return "0.0";

        if (dR == "0.0")
            return mulTerms(
                mulTerms("(" + R + ")", "pow((" + L + "), (" + R + ") - 1.0)"),
                dL
            );
        if (dL == "0.0")
            return mulTerms(
                mulTerms("pow((" + L + "), (" + R + "))", "log((" + L + "))"),
                dR
            );
        return mulTerms(
            "pow((" + L + "), (" + R + "))",
            addTerms(
                mulTerms("log((" + L + "))", dR),
                mulTerms(dL, "(" + R + ") / (" + L + ")")
            )
        );
    }

    if (isUnaryMath(base) && args.size() == 1) {
        const ASTNode* a = args[0].get();
        if (!dependsOn(a, wrt))
            return "0.0";

        std::string A = valueExpr(a, gradName, err);
        std::string dA = partialExpr(a, wrt, gradName, err);
        if (!err.empty())
            return "0.0";

        if (base == "sin")
            return mulTerms("cos((" + A + "))", dA);
        if (base == "cos")
            return mulTerms("(-sin((" + A + ")))", dA);
        if (base == "exp")
            return mulTerms("exp((" + A + "))", dA);
        if (base == "log")
            return "(" + dA + ") / (" + A + ")";
        if (base == "sqrt")
            return "(" + dA + ") / (2.0 * sqrt((" + A + ")))";
        if (base == "tanh")
            return mulTerms(
                "(1.0 - tanh((" + A + ")) * tanh((" + A + ")))",
                dA
            );
        if (base == "abs")
            return "(((" + A + ") < 0.0) ? (-(" + dA + ")) : (" + dA + "))";
        if (base == "relu")
            return "(((" + A + ") > 0.0) ? (" + dA + ") : 0.0)";
    }

    err = "call to '" + name + "' has no registered adjoint rule";
    return "0.0";
}

std::string partialCall(
    const FunctionCallNode* call,
    const std::string& wrt,
    const std::string& gradName,
    std::string& err
) {
    return partialCallArgs(
        call->getOriginalName(),
        call->getArguments(),
        wrt,
        gradName,
        err
    );
}

std::string partialExpr(
    const ASTNode* node,
    const std::string& wrt,
    const std::string& gradName,
    std::string& err
) {
    if (!node || !dependsOn(node, wrt))
        return "0.0";

    switch (node->type()) {
    case NodeType::NUMBER:
        return "0.0";

    case NodeType::VARIABLE:
        return static_cast<const VariableNode*>(node)->getOriginalName() == wrt
                   ? "1.0"
                   : "0.0";

    case NodeType::BINARY_OP: {
        auto* b = static_cast<const BinOpNode*>(node);
        const ASTNode* l = b->leftNode.get();
        const ASTNode* r = b->rightNode.get();

        switch (b->op) {
        case VTokenType::Add:
            return addTerms(
                partialExpr(l, wrt, gradName, err),
                partialExpr(r, wrt, gradName, err)
            );
        case VTokenType::Substract:
            return addTerms(
                partialExpr(l, wrt, gradName, err),
                "(-(" + partialExpr(r, wrt, gradName, err) + "))"
            );
        case VTokenType::Multiply: {
            std::string L = valueExpr(l, gradName, err);
            std::string R = valueExpr(r, gradName, err);
            std::string dL = partialExpr(l, wrt, gradName, err);
            std::string dR = partialExpr(r, wrt, gradName, err);
            if (!err.empty())
                return "0.0";
            return addTerms(
                dL == "0.0" ? "0.0" : mulTerms(dL, R),
                dR == "0.0" ? "0.0" : mulTerms(L, dR)
            );
        }
        case VTokenType::Division: {
            std::string L = valueExpr(l, gradName, err);
            std::string R = valueExpr(r, gradName, err);
            std::string dL = partialExpr(l, wrt, gradName, err);
            std::string dR = partialExpr(r, wrt, gradName, err);
            if (!err.empty())
                return "0.0";
            std::string num = addTerms(
                dL == "0.0" ? "0.0" : mulTerms(dL, R),
                dR == "0.0" ? "0.0" : "(-(" + mulTerms(L, dR) + "))"
            );
            return "(" + num + ") / ((" + R + ") * (" + R + "))";
        }
        case VTokenType::Power: {
            std::string L = valueExpr(l, gradName, err);
            std::string R = valueExpr(r, gradName, err);
            std::string dL = partialExpr(l, wrt, gradName, err);
            std::string dR = partialExpr(r, wrt, gradName, err);
            if (!err.empty())
                return "0.0";
            if (dR == "0.0")
                return mulTerms(
                    mulTerms(
                        "(" + R + ")",
                        "pow((" + L + "), (" + R + ") - 1.0)"
                    ),
                    dL
                );
            if (dL == "0.0")
                return mulTerms(
                    mulTerms(
                        "pow((" + L + "), (" + R + "))",
                        "log((" + L + "))"
                    ),
                    dR
                );
            return mulTerms(
                "pow((" + L + "), (" + R + "))",
                addTerms(
                    mulTerms("log((" + L + "))", dR),
                    mulTerms(dL, "(" + R + ") / (" + L + ")")
                )
            );
        }
        default:
            err = "operator is not differentiable yet";
            return "0.0";
        }
    }

    case NodeType::UNARY: {
        auto* u = static_cast<const UnaryNode*>(node);
        std::string d = partialExpr(u->getRight(), wrt, gradName, err);
        if (!err.empty())
            return "0.0";
        if (u->getOp() == VTokenType::Substract)
            return "(-(" + d + "))";
        if (u->getOp() == VTokenType::Add)
            return d;
        err = "unary operator is not differentiable yet";
        return "0.0";
    }

    case NodeType::FUNCTION_CALL:
        return partialCall(
            static_cast<const FunctionCallNode*>(node),
            wrt,
            gradName,
            err
        );

    case NodeType::METHOD_CALL: {
        auto* c = static_cast<const MethodCallNode*>(node);
        return partialCallArgs(
            c->getMethodName(),
            c->getArguments(),
            wrt,
            gradName,
            err
        );
    }

    default:
        err = "expression is not differentiable yet";
        return "0.0";
    }
}

// ---------------------------------------------------------------------------
// Body analysis
// ---------------------------------------------------------------------------

struct LocalDef {
    std::string name;
    const ASTNode* rhs = nullptr;
    int line = 0;
    VType type = VType::Unknown;
};

// ===========================================================================
// Array AD (Phase B, reductions)
// ===========================================================================
// This slice differentiates Float64 -> Float64 functions whose cost is a
// scalar expression over recognized array reductions:
//
//   mse(a,b) / k_mse_sum(a,b,n)
//   mae(a,b) / k_mae_sum(a,b,n)
//   dot(a,b) / k_dot(a,b,n)
//   sum(a,n) / mean(a,n)
//   norm_sq(a,n) / sum_sq(a,n) / abs_sum(a,n)
//
// Array values may be parameters or free globals (read-only constants).
// Array locals are deferred. The VJP for each reduction is emitted as a
// loop that accumulates into the caller-provided output containers, so
// the result is an `Array<Float64>` gradient of the same length.

struct ArrayAnalysis {
    std::vector<LocalDef> locals;
    const ASTNode* returnExpr = nullptr;

    std::set<std::string> paramsScalar;
    std::set<std::string> paramsArray;
    std::set<std::string> localScalar; // all locals
    std::set<std::string> localFloat;  // Float64 locals (differentiated)
    std::set<std::string> freeScalars;
    std::set<std::string> freeArrays;
};

int reductionArity(const std::string& base) {
    if (base == "mse" || base == "k_mse_sum" || base == "mae" ||
        base == "k_mae_sum" || base == "dot" || base == "k_dot")
        return 2;
    if (base == "sum" || base == "k_sum" || base == "mean" ||
        base == "norm_sq" || base == "k_norm_sq" || base == "sum_sq" ||
        base == "k_sum_sq" || base == "abs_sum" || base == "k_abs_sum")
        return 1;
    return 0;
}

bool getCallInfo(
    const ASTNode* node,
    std::string& name,
    const std::vector<std::unique_ptr<ASTNode>>*& args
) {
    if (!node)
        return false;
    if (node->type() == NodeType::FUNCTION_CALL) {
        auto* c = static_cast<const FunctionCallNode*>(node);
        name = c->getOriginalName();
        args = &c->getArguments();
        return true;
    }
    if (node->type() == NodeType::METHOD_CALL) {
        auto* c = static_cast<const MethodCallNode*>(node);
        name = c->getMethodName();
        args = &c->getArguments();
        return true;
    }
    return false;
}

bool containsReduction(const ASTNode* node) {
    if (!node)
        return false;

    std::string name;
    const std::vector<std::unique_ptr<ASTNode>>* args = nullptr;
    if (getCallInfo(node, name, args)) {
        if (reductionArity(lastSegment(name)) > 0)
            return true;
        for (const auto& a : *args)
            if (containsReduction(a.get()))
                return true;
        return false;
    }

    switch (node->type()) {
    case NodeType::BINARY_OP: {
        auto* b = static_cast<const BinOpNode*>(node);
        return containsReduction(b->leftNode.get()) ||
               containsReduction(b->rightNode.get());
    }
    case NodeType::UNARY:
        return containsReduction(
            static_cast<const UnaryNode*>(node)->getRight()
        );
    case NodeType::BUILTIN_CALL: {
        auto* c = static_cast<const BuiltInCallNode*>(node);
        for (const auto& a : c->getArguments())
            if (containsReduction(a.get()))
                return true;
        return false;
    }
    default:
        return false;
    }
}

// Classify a scalar expression, recording array-valued operands. Rejects
// array-valued arithmetic, array locals, and unknown calls.
bool classifyArrayExpr(
    const ASTNode* node,
    ArrayAnalysis& A,
    std::string& reason
) {
    if (!node)
        return true;

    std::string name;
    const std::vector<std::unique_ptr<ASTNode>>* args = nullptr;
    if (getCallInfo(node, name, args)) {
        const std::string base = lastSegment(name);
        const int arity = reductionArity(base);
        if (arity > 0) {
            if (args->size() < (size_t)arity) {
                reason = "reduction '" + name + "' has too few arguments";
                return false;
            }
            for (int i = 0; i < arity; ++i) {
                const ASTNode* a = (*args)[i].get();
                if (!a || a->type() != NodeType::VARIABLE) {
                    reason = "in this phase every array argument of '" + name +
                             "' must be a plain variable";
                    return false;
                }
                auto* v = static_cast<const VariableNode*>(a);
                const std::string& vn = v->getOriginalName();
                if (A.paramsArray.count(vn))
                    continue;
                if (A.localScalar.count(vn)) {
                    reason =
                        "array locals are not supported yet ('" + vn + "')";
                    return false;
                }
                if (A.freeScalars.count(vn)) {
                    reason = "variable '" + vn +
                             "' is used both as a scalar and as an array";
                    return false;
                }
                A.freeArrays.insert(vn);
            }
            for (size_t i = (size_t)arity; i < args->size(); ++i)
                if (!classifyArrayExpr((*args)[i].get(), A, reason))
                    return false;
            return true;
        }

        if (base == "size") {
            if (args->size() != 0) {
                reason = ".size() takes no arguments";
                return false;
            }
            // The receiver is materialized by MethodCallNode; accept the
            // common `array.size()` shape and mark the receiver as array.
            return true;
        }

        if (isUnaryMath(base) || base == "pow") {
            for (const auto& a : *args)
                if (!classifyArrayExpr(a.get(), A, reason))
                    return false;
            return true;
        }

        reason = "call to '" + name + "' is not part of the array AD subset";
        return false;
    }

    switch (node->type()) {
    case NodeType::NUMBER:
        return true;

    case NodeType::VARIABLE: {
        auto* v = static_cast<const VariableNode*>(node);
        const std::string& vn = v->getOriginalName();
        if (A.paramsScalar.count(vn) || A.paramsArray.count(vn) ||
            A.localScalar.count(vn)) {
            if (A.paramsArray.count(vn)) {
                reason = "array parameter '" + vn + "' is used as a scalar";
                return false;
            }
            return true;
        }
        if (A.freeArrays.count(vn)) {
            reason = "array '" + vn + "' is used as a scalar";
            return false;
        }
        A.freeScalars.insert(vn);
        return true;
    }

    case NodeType::BINARY_OP: {
        auto* b = static_cast<const BinOpNode*>(node);
        return classifyArrayExpr(b->leftNode.get(), A, reason) &&
               classifyArrayExpr(b->rightNode.get(), A, reason);
    }

    case NodeType::UNARY:
        return classifyArrayExpr(
            static_cast<const UnaryNode*>(node)->getRight(),
            A,
            reason
        );

    case NodeType::BUILTIN_CALL: {
        auto* c = static_cast<const BuiltInCallNode*>(node);
        const std::string& fn = c->getFunctionName();
        if ((fn == "float64" || fn == "int64") && c->getArguments().size() == 1)
            return classifyArrayExpr(c->getArguments()[0].get(), A, reason);
        reason = "builtin '" + fn + "' is not part of the array AD subset";
        return false;
    }

    default:
        reason = "expression is not part of the array AD subset";
        return false;
    }
}

bool analyzeArrayBody(
    const FunctionNode& fn,
    ArrayAnalysis& A,
    std::string& reason
) {
    for (const auto& p : fn.getParameters()) {
        if (p.type == VType::Float64 && !p.isReference) {
            A.paramsScalar.insert(p.name);
        } else if (
            p.type == VType::Array && p.arrayElemType == VType::Float64 &&
            !p.isReference
        ) {
            A.paramsArray.insert(p.name);
        } else {
            reason =
                "parameter '" + p.name + "' is not Float64 or Array<Float64>";
            return false;
        }
    }

    if (fn.getReturnType() != VType::Float64) {
        reason = "array AD currently requires a Float64 return type";
        return false;
    }

    const auto& body = fn.getBody();
    for (size_t i = 0; i < body.size(); ++i) {
        const auto& stmt = body[i];
        if (!stmt)
            continue;

        if (stmt->type() == NodeType::ASSIGNMENT) {
            auto* a = static_cast<const AssignmentNode*>(stmt.get());
            if (!a->isFreshDeclaration()) {
                reason = "assignment to '" + a->getOriginalName() +
                         "' mutates a local; use fresh ':=' declarations";
                return false;
            }
            if (a->getExpectedType() == VType::Array) {
                reason = "array locals are not supported yet ('" +
                         a->getOriginalName() + "')";
                return false;
            }
            if (!classifyArrayExpr(a->getRHS(), A, reason)) {
                reason = "in '" + a->getOriginalName() + "': " + reason;
                return false;
            }
            A.localScalar.insert(a->getOriginalName());
            if (a->getExpectedType() == VType::Float64 ||
                a->getExpectedType() == VType::Unknown)
                A.localFloat.insert(a->getOriginalName());
            A.locals.push_back(
                {a->getOriginalName(),
                 a->getRHS(),
                 a->lineNumber,
                 a->getExpectedType()}
            );
            continue;
        }

        if (stmt->type() == NodeType::RETURN) {
            if (A.returnExpr) {
                reason = "multiple return statements are not supported";
                return false;
            }
            A.returnExpr =
                static_cast<const ReturnNode*>(stmt.get())->getExpression();
            if (!A.returnExpr) {
                reason = "return statement has no expression";
                return false;
            }
            if (!classifyArrayExpr(A.returnExpr, A, reason)) {
                reason = "in return: " + reason;
                return false;
            }
            for (size_t j = i + 1; j < body.size(); ++j)
                if (body[j]) {
                    reason = "statement after return is not supported";
                    return false;
                }
            continue;
        }

        reason = "only fresh scalar declarations and a final return are "
                 "supported in array AD";
        return false;
    }

    if (!A.returnExpr) {
        reason = "body never returns a value";
        return false;
    }
    return true;
}

struct ArrayCtx {
    std::string gradName;
    const ArrayAnalysis* A = nullptr;
    std::set<std::string> unavailable;

    std::string fwd(const std::string& name) const {
        if (A->paramsScalar.count(name) || A->paramsArray.count(name) ||
            A->localScalar.count(name))
            return "v_" + gradName + "_" + sanitizeName(name);
        return "v_" + sanitizeName(name);
    }
};

// `containsReduction` plus the analysis knowledge that a local may be a
// reduction result stored in a scalar temporary.
bool containsReductionCtx(const ASTNode* node, const ArrayCtx& ctx) {
    if (!node)
        return false;

    if (node->type() == NodeType::VARIABLE) {
        auto* v = static_cast<const VariableNode*>(node);
        return ctx.unavailable.count(v->getOriginalName()) > 0;
    }
    if (node->type() == NodeType::NUMBER)
        return false;

    std::string name;
    const std::vector<std::unique_ptr<ASTNode>>* args = nullptr;
    if (getCallInfo(node, name, args)) {
        if (reductionArity(lastSegment(name)) > 0)
            return true;
        for (const auto& a : *args)
            if (containsReductionCtx(a.get(), ctx))
                return true;
        return false;
    }

    switch (node->type()) {
    case NodeType::BINARY_OP: {
        auto* b = static_cast<const BinOpNode*>(node);
        return containsReductionCtx(b->leftNode.get(), ctx) ||
               containsReductionCtx(b->rightNode.get(), ctx);
    }
    case NodeType::UNARY:
        return containsReductionCtx(
            static_cast<const UnaryNode*>(node)->getRight(),
            ctx
        );
    case NodeType::BUILTIN_CALL: {
        auto* c = static_cast<const BuiltInCallNode*>(node);
        for (const auto& a : c->getArguments())
            if (containsReductionCtx(a.get(), ctx))
                return true;
        return false;
    }
    default:
        return false;
    }
}

std::string
valueExprCtx(const ASTNode* node, const ArrayCtx& ctx, std::string& err) {
    if (!node) {
        err = "empty expression";
        return "0.0";
    }

    if (node->type() == NodeType::NUMBER)
        return static_cast<const NumberNode*>(node)->nativeLiteral();

    if (node->type() == NodeType::VARIABLE) {
        auto* v = static_cast<const VariableNode*>(node);
        const std::string& n = v->getOriginalName();
        if (ctx.unavailable.count(n)) {
            err = "value of '" + n +
                  "' depends on a reduction and is not materialized in this "
                  "phase";
            return "0.0";
        }
        return ctx.fwd(n);
    }

    if (node->type() == NodeType::BINARY_OP) {
        auto* b = static_cast<const BinOpNode*>(node);
        std::string l = valueExprCtx(b->leftNode.get(), ctx, err);
        std::string r = valueExprCtx(b->rightNode.get(), ctx, err);
        if (!err.empty())
            return "0.0";
        switch (b->op) {
        case VTokenType::Add:
            return "((" + l + ") + (" + r + "))";
        case VTokenType::Substract:
            return "((" + l + ") - (" + r + "))";
        case VTokenType::Multiply:
            return "((" + l + ") * (" + r + "))";
        case VTokenType::Division:
            return "((" + l + ") / (" + r + "))";
        case VTokenType::Power:
            return "pow((" + l + "), (" + r + "))";
        default:
            err = "operator is not supported in the array AD subset";
            return "0.0";
        }
    }

    if (node->type() == NodeType::UNARY) {
        auto* u = static_cast<const UnaryNode*>(node);
        std::string v = valueExprCtx(u->getRight(), ctx, err);
        if (!err.empty())
            return "0.0";
        if (u->getOp() == VTokenType::Substract)
            return "(-(" + v + "))";
        if (u->getOp() == VTokenType::Add)
            return v;
        err = "unary operator is not supported in the array AD subset";
        return "0.0";
    }

    if (node->type() == NodeType::BUILTIN_CALL) {
        auto* c = static_cast<const BuiltInCallNode*>(node);
        if (c->getArguments().size() == 1) {
            std::string v = valueExprCtx(c->getArguments()[0].get(), ctx, err);
            if (!err.empty())
                return "0.0";
            if (c->getFunctionName() == "float64")
                return "((double)(" + v + "))";
            if (c->getFunctionName() == "int64")
                return "((int64_t)(" + v + "))";
        }
        err = "builtin is not supported in the array AD subset";
        return "0.0";
    }

    std::string name;
    const std::vector<std::unique_ptr<ASTNode>>* args = nullptr;
    if (getCallInfo(node, name, args)) {
        const std::string base = lastSegment(name);
        if (reductionArity(base) > 0) {
            err = "a reduction value is needed as a coefficient; this phase "
                  "supports reductions only in additive/constant-scaled "
                  "positions";
            return "0.0";
        }
        if (base == "size" && args->empty()) {
            if (node->type() == NodeType::METHOD_CALL) {
                auto* mc = static_cast<const MethodCallNode*>(node);
                if (mc->getReceiver()->type() == NodeType::VARIABLE) {
                    auto* rv =
                        static_cast<const VariableNode*>(mc->getReceiver());
                    return "((double)(" + ctx.fwd(rv->getOriginalName()) +
                           ".size))";
                }
            }
            err = ".size() receiver is not a variable";
            return "0.0";
        }
        if (isUnaryMath(base) && args->size() == 1) {
            std::string a = valueExprCtx((*args)[0].get(), ctx, err);
            if (!err.empty())
                return "0.0";
            if (base == "abs")
                return "fabs((" + a + "))";
            if (base == "relu")
                return "(((" + a + ") > 0.0) ? (" + a + ") : 0.0)";
            return base + "((" + a + "))";
        }
        if (base == "pow" && args->size() == 2) {
            std::string a = valueExprCtx((*args)[0].get(), ctx, err);
            std::string b = valueExprCtx((*args)[1].get(), ctx, err);
            if (!err.empty())
                return "0.0";
            return "pow((" + a + "), (" + b + "))";
        }
        err = "call '" + name + "' is not supported in the array AD subset";
        return "0.0";
    }

    err = "expression is not supported in the array AD subset";
    return "0.0";
}

bool emitArrayVJPs(
    C_Emitter& e,
    const ASTNode* node,
    const std::string& dCoeff,
    const ArrayCtx& ctx,
    std::string& err
);

bool emitReductionVJP(
    C_Emitter& e,
    const std::string& name,
    const std::vector<std::unique_ptr<ASTNode>>& args,
    const std::string& dCoeff,
    const ArrayCtx& ctx,
    std::string& err
) {
    const std::string base = lastSegment(name);
    const int arity = reductionArity(base);
    if (arity == 0 || args.size() < (size_t)arity) {
        err = "malformed reduction '" + name + "'";
        return false;
    }

    const ArrayAnalysis& A = *ctx.A;
    std::vector<std::string> vn;
    std::vector<std::string> fwd;
    std::vector<bool> isParam;
    for (int i = 0; i < arity; ++i) {
        if (args[i]->type() != NodeType::VARIABLE) {
            err = "array argument of '" + name + "' must be a variable";
            return false;
        }
        auto* v = static_cast<const VariableNode*>(args[i].get());
        vn.push_back(v->getOriginalName());
        fwd.push_back(ctx.fwd(vn.back()));
        isParam.push_back(A.paramsArray.count(vn.back()) > 0);
    }

    const std::string n = fwd[0] + ".size";
    std::string loop = e.newTemp("adi");

    auto out = [&](size_t i) { return "__d_" + sanitizeName(vn[i]); };

    auto emitLoop = [&](const std::string& body) {
        e.emit(
            "for (int64_t " + loop + " = 0; " + loop + " < (int64_t)(" + n +
            "); ++" + loop + ") {"
        );
        e.emit("    " + body);
        e.emit("}");
    };

    const std::string idx = "[" + loop + "]";

    if (base == "mse" || base == "k_mse_sum") {
        const bool mean = (base == "mse");
        const std::string scale = mean ? "(2.0 / (double)(" + n + "))" : "2.0";
        const std::string diff = "((" + fwd[0] + ".data" + idx + ") - (" +
                                 fwd[1] + ".data" + idx + "))";
        std::string body;
        if (isParam[0])
            body += out(0) + "->data" + idx + " += (" + scale + ") * " + diff +
                    " * (" + dCoeff + ");";
        if (isParam[1]) {
            if (!body.empty())
                body += " ";
            body += out(1) + "->data" + idx + " -= (" + scale + ") * " + diff +
                    " * (" + dCoeff + ");";
        }
        if (!body.empty())
            emitLoop(body);
        return true;
    }

    if (base == "mae" || base == "k_mae_sum") {
        const bool mean = (base == "mae");
        const std::string scale = mean ? "(1.0 / (double)(" + n + "))" : "1.0";
        const std::string diff = "((" + fwd[0] + ".data" + idx + ") - (" +
                                 fwd[1] + ".data" + idx + "))";
        std::string body;
        if (isParam[0])
            body += out(0) + "->data" + idx + " += ((" + diff +
                    ") < 0.0 ? -((" + scale + ") * (" + dCoeff + ")) : ((" +
                    scale + ") * (" + dCoeff + ")));";
        if (isParam[1]) {
            if (!body.empty())
                body += " ";
            body += out(1) + "->data" + idx + " += ((" + diff + ") < 0.0 ? ((" +
                    scale + ") * (" + dCoeff + ")) : -((" + scale + ") * (" +
                    dCoeff + ")));";
        }
        if (!body.empty())
            emitLoop(body);
        return true;
    }

    if (base == "dot" || base == "k_dot") {
        std::string body;
        if (isParam[0])
            body += out(0) + "->data" + idx + " += (" + fwd[1] + ".data" + idx +
                    ") * (" + dCoeff + ");";
        if (isParam[1]) {
            if (!body.empty())
                body += " ";
            body += out(1) + "->data" + idx + " += (" + fwd[0] + ".data" + idx +
                    ") * (" + dCoeff + ");";
        }
        if (!body.empty())
            emitLoop(body);
        return true;
    }

    if (base == "sum" || base == "k_sum" || base == "mean") {
        if (isParam[0]) {
            const std::string term =
                (base == "mean") ? "((" + dCoeff + ") / (double)(" + n + "))"
                                 : "(" + dCoeff + ")";
            emitLoop(out(0) + "->data" + idx + " += " + term + ";");
        }
        return true;
    }

    if (base == "norm_sq" || base == "k_norm_sq" || base == "sum_sq" ||
        base == "k_sum_sq") {
        if (isParam[0])
            emitLoop(
                out(0) + "->data" + idx + " += 2.0 * (" + fwd[0] + ".data" +
                idx + ") * (" + dCoeff + ");"
            );
        return true;
    }

    if (base == "abs_sum" || base == "k_abs_sum") {
        if (isParam[0])
            emitLoop(
                out(0) + "->data" + idx + " += ((" + fwd[0] + ".data" + idx +
                ") < 0.0 ? -(" + dCoeff + ") : (" + dCoeff + "));"
            );
        return true;
    }

    err = "no array VJP rule for '" + name + "'";
    return false;
}

bool emitArrayVJPs(
    C_Emitter& e,
    const ASTNode* node,
    const std::string& dCoeff,
    const ArrayCtx& ctx,
    std::string& err
) {
    if (!node)
        return true;

    if (node->type() == NodeType::NUMBER)
        return true;

    if (node->type() == NodeType::VARIABLE) {
        auto* v = static_cast<const VariableNode*>(node);
        const std::string& n = v->getOriginalName();
        const ArrayAnalysis& A = *ctx.A;
        if (A.paramsArray.count(n) || A.freeArrays.count(n)) {
            err = "array '" + n + "' is used outside a reduction";
            return false;
        }
        if (A.localFloat.count(n) || A.paramsScalar.count(n)) {
            e.emit(deltaName(ctx.gradName, n) + " += (" + dCoeff + ");");
        }
        return true;
    }

    if (node->type() == NodeType::BINARY_OP) {
        auto* b = static_cast<const BinOpNode*>(node);
        const ASTNode* l = b->leftNode.get();
        const ASTNode* r = b->rightNode.get();

        switch (b->op) {
        case VTokenType::Add:
            return emitArrayVJPs(e, l, dCoeff, ctx, err) &&
                   emitArrayVJPs(e, r, dCoeff, ctx, err);
        case VTokenType::Substract:
            return emitArrayVJPs(e, l, dCoeff, ctx, err) &&
                   emitArrayVJPs(e, r, "(-(" + dCoeff + "))", ctx, err);
        case VTokenType::Multiply: {
            const bool lr = containsReductionCtx(l, ctx);
            const bool rr = containsReductionCtx(r, ctx);
            if (lr && rr) {
                err = "product of two reduction terms is not supported";
                return false;
            }
            if (lr) {
                std::string coef = valueExprCtx(r, ctx, err);
                if (!err.empty())
                    return false;
                return emitArrayVJPs(
                    e,
                    l,
                    "((" + dCoeff + ") * (" + coef + "))",
                    ctx,
                    err
                );
            }
            if (rr) {
                std::string coef = valueExprCtx(l, ctx, err);
                if (!err.empty())
                    return false;
                return emitArrayVJPs(
                    e,
                    r,
                    "((" + dCoeff + ") * (" + coef + "))",
                    ctx,
                    err
                );
            }
            return true;
        }
        case VTokenType::Division: {
            const bool lr = containsReductionCtx(l, ctx);
            const bool rr = containsReductionCtx(r, ctx);
            if (lr && rr) {
                err = "division of two reduction terms is not supported";
                return false;
            }
            if (lr) {
                std::string den = valueExprCtx(r, ctx, err);
                if (!err.empty())
                    return false;
                return emitArrayVJPs(
                    e,
                    l,
                    "((" + dCoeff + ") / (" + den + "))",
                    ctx,
                    err
                );
            }
            if (rr) {
                std::string num = valueExprCtx(l, ctx, err);
                std::string den = valueExprCtx(r, ctx, err);
                if (!err.empty())
                    return false;
                return emitArrayVJPs(
                    e,
                    r,
                    "(-((" + num + ") * (" + dCoeff + ")) / ((" + den +
                        ") * (" + den + ")))",
                    ctx,
                    err
                );
            }
            return true;
        }
        default:
            err = "operator is not supported in array AD";
            return false;
        }
    }

    if (node->type() == NodeType::UNARY) {
        auto* u = static_cast<const UnaryNode*>(node);
        if (u->getOp() == VTokenType::Substract)
            return emitArrayVJPs(
                e,
                u->getRight(),
                "(-(" + dCoeff + "))",
                ctx,
                err
            );
        if (u->getOp() == VTokenType::Add)
            return emitArrayVJPs(e, u->getRight(), dCoeff, ctx, err);
        err = "unary operator is not supported in array AD";
        return false;
    }

    std::string name;
    const std::vector<std::unique_ptr<ASTNode>>* args = nullptr;
    if (getCallInfo(node, name, args)) {
        if (reductionArity(lastSegment(name)) > 0)
            return emitReductionVJP(e, name, *args, dCoeff, ctx, err);

        // Scalar-only calls and `.size()` are constants for VJP purposes.
        if (!containsReductionCtx(node, ctx))
            return true;
        err = "call '" + name + "' is not supported in array AD";
        return false;
    }

    if (node->type() == NodeType::BUILTIN_CALL) {
        if (!containsReductionCtx(node, ctx))
            return true;
        err = "builtin is not supported in array AD";
        return false;
    }

    err = "expression is not supported in array AD";
    return false;
}

bool analyzeBody(
    const FunctionNode& fn,
    std::vector<LocalDef>& locals,
    const ASTNode*& returnExpr,
    std::string& reason
) {
    locals.clear();
    returnExpr = nullptr;

    const auto& params = fn.getParameters();
    std::set<std::string> paramNames;
    for (const auto& p : params) {
        paramNames.insert(p.name);
        if (p.type != VType::Float64) {
            reason = "parameter '" + p.name +
                     "' is not Float64; this AD slice differentiates "
                     "scalar Float64 functions only";
            return false;
        }
        if (p.isReference) {
            reason = "reference parameter '" + p.name +
                     "' is not supported by the AD slice";
            return false;
        }
    }

    if (fn.getReturnType() != VType::Float64) {
        reason = "return type is not Float64; this AD slice differentiates "
                 "Float64 -> Float64 functions only";
        return false;
    }

    const auto& body = fn.getBody();
    std::set<std::string> localNames;

    for (size_t i = 0; i < body.size(); ++i) {
        const auto& stmt = body[i];
        if (!stmt)
            continue;

        if (stmt->type() == NodeType::ASSIGNMENT) {
            if (returnExpr) {
                reason = "a declaration appears after the return statement";
                return false;
            }
            auto* a = static_cast<const AssignmentNode*>(stmt.get());
            if (!a->isFreshDeclaration()) {
                reason = "assignment to '" + a->getOriginalName() +
                         "' mutates a local; @differentiable bodies must use "
                         "fresh declarations (SSA style)";
                return false;
            }
            if (a->getExpectedType() != VType::Float64 &&
                a->getExpectedType() != VType::Unknown) {
                reason = "local '" + a->getOriginalName() + "' is not Float64";
                return false;
            }
            if (paramNames.count(a->getOriginalName()) ||
                localNames.count(a->getOriginalName())) {
                reason =
                    "local '" + a->getOriginalName() + "' shadows another name";
                return false;
            }
            std::string err;
            valueExpr(a->getRHS(), "probe", err);
            if (!err.empty()) {
                reason = "in '" + a->getOriginalName() + "': " + err;
                return false;
            }
            {
                std::set<std::string> refs;
                std::string cerr;
                if (!collectVars(a->getRHS(), refs, cerr)) {
                    reason = "in '" + a->getOriginalName() + "': " + cerr;
                    return false;
                }
                for (const auto& r : refs) {
                    if (!paramNames.count(r) && !localNames.count(r)) {
                        reason = "'" + a->getOriginalName() + "' reads '" + r +
                                 "', which is not a parameter or an earlier "
                                 "local; @differentiable bodies must be "
                                 "closed over their Float64 parameters";
                        return false;
                    }
                }
            }
            localNames.insert(a->getOriginalName());
            locals.push_back(
                {a->getOriginalName(), a->getRHS(), a->lineNumber}
            );
            continue;
        }

        if (stmt->type() == NodeType::RETURN) {
            if (returnExpr) {
                reason = "multiple return statements are not supported";
                return false;
            }
            auto* r = static_cast<const ReturnNode*>(stmt.get());
            returnExpr = r->getExpression();
            if (!returnExpr) {
                reason = "return statement has no expression";
                return false;
            }
            std::string err;
            valueExpr(returnExpr, "probe", err);
            if (!err.empty()) {
                reason = "in return: " + err;
                return false;
            }
            // `return` must be the final statement.
            for (size_t j = i + 1; j < body.size(); ++j)
                if (body[j]) {
                    reason = "statement after return is not supported";
                    return false;
                }
            continue;
        }

        reason = "only fresh Float64 declarations and a final return are "
                 "supported inside @differentiable in this phase "
                 "(no branches, loops, or calls without adjoint rules)";
        return false;
    }

    if (!returnExpr) {
        reason = "body never returns a value";
        return false;
    }

    {
        std::set<std::string> refs;
        std::string cerr;
        if (!collectVars(returnExpr, refs, cerr)) {
            reason = "in return: " + cerr;
            return false;
        }
        for (const auto& r : refs) {
            if (!paramNames.count(r) && !localNames.count(r)) {
                reason = "return reads '" + r +
                         "', which is not a parameter or a local; "
                         "@differentiable bodies must be closed over their "
                         "Float64 parameters";
                return false;
            }
        }
    }
    return true;
}

} // namespace

bool validate(const FunctionNode& fn, std::string* reason) {
    bool anyArray = false;
    for (const auto& p : fn.getParameters())
        if (p.type == VType::Array)
            anyArray = true;

    std::string why;
    bool ok = false;
    if (anyArray) {
        ArrayAnalysis A;
        ok = analyzeArrayBody(fn, A, why);
    } else {
        std::vector<LocalDef> locals;
        const ASTNode* ret = nullptr;
        ok = analyzeBody(fn, locals, ret, why);
    }
    if (!ok && reason)
        *reason = why;
    return ok;
}

std::string adjointCName(const std::string& mangledName) {
    return "fn_" + mangledName + "_grad";
}

std::string
adjointPrototype(const FunctionNode& fn, const std::string& mangledName) {
    const std::string gradName = mangledName + "_grad";
    std::string s = "void " + adjointCName(mangledName) + "(";

    bool first = true;
    for (const auto& p : fn.getParameters()) {
        if (!first)
            s += ", ";
        first = false;
        const char* ty = (p.type == VType::Array) ? "VyneArray_f64" : "double";
        s += std::string(ty) + " " + valueName(gradName, p.name);
    }
    if (!first)
        s += ", ";
    s += "double __d_out";

    for (const auto& p : fn.getParameters()) {
        if (p.type == VType::Array)
            s += ", VyneArray_f64* __d_" + sanitizeName(p.name);
        else
            s += ", double* __d_" + sanitizeName(p.name);
    }

    s += ")";
    return s;
}

static void emitArrayAdjoint(
    C_Emitter& e,
    const FunctionNode& fn,
    const std::string& mangledName
);

void emitAdjoint(
    C_Emitter& e,
    const FunctionNode& fn,
    const std::string& mangledName
) {
    bool anyArray = false;
    for (const auto& p : fn.getParameters())
        if (p.type == VType::Array)
            anyArray = true;
    if (anyArray) {
        emitArrayAdjoint(e, fn, mangledName);
        return;
    }

    std::vector<LocalDef> locals;
    const ASTNode* returnExpr = nullptr;
    std::string reason;
    if (!analyzeBody(fn, locals, returnExpr, reason)) {
        throw std::runtime_error(
            "Compile Error (VNE-120): @differentiable function '" +
            fn.getOriginalName() + "' is outside the supported AD slice: " +
            reason + " (line " + std::to_string(fn.lineNumber) + ")."
        );
    }

    const std::string gradName = mangledName + "_grad";
    const std::string proto = adjointPrototype(fn, mangledName);

    e.pushFunctionContext();
    e.enterFunction(gradName);
    e.setInNativeVariant(true);

    e.emitGlobalDecl(proto + ";");
    e.emit("// reverse-mode adjoint of " + fn.getOriginalName());
    e.emitBlockOpen(proto + " {");

    // Parameters are already named and typed by the signature; register
    // them so the emitter's static-type queries stay accurate.
    for (const auto& p : fn.getParameters()) {
        CType ct = CType::fromVType(VType::Float64);
        e.declareLocal(valueName(gradName, p.name), ct);
    }

    // Forward recomputation. These locals are the tape. They live in the
    // caller's C block / region and die when it closes — no heap tape.
    std::string err;
    for (const auto& l : locals) {
        std::string rhs = valueExpr(l.rhs, gradName, err);
        if (!err.empty()) {
            throw std::runtime_error(
                "Compile Error (VNE-121): " + err + " (line " +
                std::to_string(l.line) + ")."
            );
        }
        std::string cname = valueName(gradName, l.name);
        e.emit("double " + cname + " = " + rhs + ";");
        e.declareLocal(cname, CType::fromVType(VType::Float64));
    }

    // Accumulators, seeded to zero.
    for (const auto& p : fn.getParameters()) {
        e.emit("double " + deltaName(gradName, p.name) + " = 0.0;");
    }
    for (const auto& l : locals) {
        e.emit("double " + deltaName(gradName, l.name) + " = 0.0;");
    }

    // Seed the output gradient into the returned expression.
    {
        std::set<std::string> retVars;
        if (!collectVars(returnExpr, retVars, err)) {
            throw std::runtime_error(
                "Compile Error (VNE-122): " + err + " (line " +
                std::to_string(fn.lineNumber) + ")."
            );
        }
        for (const auto& v : retVars) {
            std::string p = partialExpr(returnExpr, v, gradName, err);
            if (!err.empty()) {
                throw std::runtime_error(
                    "Compile Error (VNE-123): " + err + " (line " +
                    std::to_string(fn.lineNumber) + ")."
                );
            }
            if (p == "0.0")
                continue;
            e.emit(deltaName(gradName, v) + " += (" + p + ") * __d_out;");
        }
    }

    // Reverse sweep.
    for (auto it = locals.rbegin(); it != locals.rend(); ++it) {
        const std::string dv = deltaName(gradName, it->name);
        std::set<std::string> deps;
        if (!collectVars(it->rhs, deps, err)) {
            throw std::runtime_error(
                "Compile Error (VNE-124): " + err + " (line " +
                std::to_string(it->line) + ")."
            );
        }
        for (const auto& u : deps) {
            std::string p = partialExpr(it->rhs, u, gradName, err);
            if (!err.empty()) {
                throw std::runtime_error(
                    "Compile Error (VNE-125): " + err + " (line " +
                    std::to_string(it->line) + ")."
                );
            }
            if (p == "0.0")
                continue;
            e.emit(deltaName(gradName, u) + " += (" + p + ") * " + dv + ";");
        }
    }

    // Publish the input gradients.
    for (const auto& p : fn.getParameters()) {
        e.emit(
            "*__d_" + sanitizeName(p.name) + " = " +
            deltaName(gradName, p.name) + ";"
        );
    }

    e.emitBlockClose();
    e.emit("");

    e.exitFunction();
    e.setInNativeVariant(false);
    e.popFunctionContext();
}

// ---------------------------------------------------------------------------
// Array adjoint emission
// ---------------------------------------------------------------------------

static void emitArrayAdjoint(
    C_Emitter& e,
    const FunctionNode& fn,
    const std::string& mangledName
) {
    ArrayAnalysis A;
    std::string reason;
    if (!analyzeArrayBody(fn, A, reason)) {
        throw std::runtime_error(
            "Compile Error (VNE-120): @differentiable function '" +
            fn.getOriginalName() + "' is outside the supported AD slice: " +
            reason + " (line " + std::to_string(fn.lineNumber) + ")."
        );
    }

    const std::string gradName = mangledName + "_grad";
    const std::string proto = adjointPrototype(fn, mangledName);

    e.pushFunctionContext();
    e.enterFunction(gradName);
    e.setInNativeVariant(true);

    e.emitGlobalDecl(proto + ";");
    e.emit("// reverse-mode (array VJP) adjoint of " + fn.getOriginalName());
    e.emitBlockOpen(proto + " {");

    for (const auto& p : fn.getParameters()) {
        if (p.type == VType::Array) {
            CType arr;
            arr.kind = CType::Kind::Array;
            arr.args.push_back(CType::fromVType(VType::Float64));
            e.declareLocal(valueName(gradName, p.name), arr);
        } else {
            e.declareLocal(
                valueName(gradName, p.name),
                CType::fromVType(VType::Float64)
            );
        }
    }

    ArrayCtx ctx;
    ctx.gradName = gradName;
    ctx.A = &A;
    for (const auto& l : A.locals)
        if (containsReduction(l.rhs))
            ctx.unavailable.insert(l.name);

    std::string err;

    // Forward recomputation for scalar locals that do not depend on a
    // reduction. These are the only values needed as VJP coefficients.
    for (const auto& l : A.locals) {
        if (ctx.unavailable.count(l.name))
            continue;
        std::string rhs = valueExprCtx(l.rhs, ctx, err);
        if (!err.empty()) {
            throw std::runtime_error(
                "Compile Error (VNE-121): " + err + " (line " +
                std::to_string(l.line) + ")."
            );
        }
        CType ct = (l.type == VType::Int64) ? CType::fromVType(VType::Int64)
                                            : CType::fromVType(VType::Float64);
        std::string cname = ctx.fwd(l.name);
        e.emit(ct.cTypeName() + " " + cname + " = " + rhs + ";");
        e.declareLocal(cname, ct);
    }

    // Scalar accumulators.
    for (const auto& n : A.paramsScalar)
        e.emit("double " + deltaName(gradName, n) + " = 0.0;");
    for (const auto& n : A.localFloat)
        e.emit("double " + deltaName(gradName, n) + " = 0.0;");

    // Array outputs are caller-provided containers; zero them first so the
    // VJP loops can accumulate.
    for (const auto& p : fn.getParameters()) {
        if (p.type != VType::Array)
            continue;
        std::string out = "__d_" + sanitizeName(p.name);
        std::string i = e.newTemp("adz");
        e.emit(
            "for (int64_t " + i + " = 0; " + i + " < (int64_t)" + out +
            "->size; ++" + i + ") { " + out + "->data[" + i + "] = 0.0; }"
        );
    }

    // Seed the returned scalar expression.
    if (!emitArrayVJPs(e, A.returnExpr, "__d_out", ctx, err)) {
        throw std::runtime_error(
            "Compile Error (VNE-122): " + err + " (line " +
            std::to_string(fn.lineNumber) + ")."
        );
    }

    // Reverse sweep over scalar locals.
    for (auto it = A.locals.rbegin(); it != A.locals.rend(); ++it) {
        if (!A.localFloat.count(it->name))
            continue;
        const std::string dv = deltaName(gradName, it->name);
        if (!emitArrayVJPs(e, it->rhs, dv, ctx, err)) {
            throw std::runtime_error(
                "Compile Error (VNE-123): " + err + " (line " +
                std::to_string(it->line) + ")."
            );
        }
    }

    // Publish scalar gradients. Array gradients were written directly
    // into the out containers by the VJP loops.
    for (const auto& p : fn.getParameters()) {
        if (p.type == VType::Array)
            continue;
        e.emit(
            "*__d_" + sanitizeName(p.name) + " = " +
            deltaName(gradName, p.name) + ";"
        );
    }

    e.emitBlockClose();
    e.emit("");

    e.exitFunction();
    e.setInNativeVariant(false);
    e.popFunctionContext();
}

// ---------------------------------------------------------------------------
// `grad(f, x)` and `backward(loss)` lowering
// ---------------------------------------------------------------------------

std::string emitGradCall(C_Emitter& e, const FunctionCallNode& call) {
    const auto& args = call.getArguments();
    if (args.size() != 2) {
        throw std::runtime_error(
            "Compile Error (VNE-125): grad(f, x) takes exactly 2 arguments "
            "(line " +
            std::to_string(call.lineNumber) + ")."
        );
    }

    std::string fnName;
    if (args[0]->type() == NodeType::VARIABLE) {
        fnName =
            static_cast<const VariableNode*>(args[0].get())->getOriginalName();
    } else if (args[0]->type() == NodeType::MEMBER_ACCESS) {
        fnName =
            static_cast<const MemberAccessNode*>(args[0].get())->getFullPath();
    } else {
        throw std::runtime_error(
            "Compile Error (VNE-125): the first argument of grad() must name "
            "a @differentiable function (line " +
            std::to_string(call.lineNumber) + ")."
        );
    }

    const C_Emitter::ADInfo* info = e.lookupDifferentiable(fnName);
    if (!info) {
        throw std::runtime_error(
            "Compile Error (VNE-125): grad() target '" + fnName +
            "' is not @differentiable or has not been seen yet (line " +
            std::to_string(call.lineNumber) + ")."
        );
    }
    if (info->arity != 1) {
        throw std::runtime_error(
            "Compile Error (VNE-125): grad(f, x) currently supports unary "
            "functions only; '" +
            fnName + "' has " + std::to_string(info->arity) +
            " parameters (line " + std::to_string(call.lineNumber) + ")."
        );
    }

    // Array unary: f(Array<Float64>) -> Float64, so grad returns a new
    // Array<Float64> of the same length.
    if (info->paramTypes.size() == 1 && info->paramTypes[0] == VType::Array) {
        std::string raw = args[1]->getCExpr(e);
        std::string arr;
        const CType* ct = e.exprNativeType(raw);
        if (ct && ct->kind == CType::Kind::Array) {
            arr = raw;
        } else {
            arr = "vyne_value_to_array_f64(" + e.boxAny(raw) + ")";
        }

        std::string tmp = e.newTemp("ad_grad");
        e.emit(
            "VyneArray_f64 " + tmp + " = vyne_array_f64_create((int64_t)(" +
            arr + ").size);"
        );
        e.emit(info->adjointName + "(" + arr + ", 1.0, &" + tmp + ");");

        CType outCt;
        outCt.kind = CType::Kind::Array;
        outCt.args.push_back(CType::fromVType(VType::Float64));
        e.declareLocal(tmp, outCt);
        return tmp;
    }

    std::string x;
    if (args[1]->type() == NodeType::NUMBER) {
        x = static_cast<const NumberNode*>(args[1].get())->nativeLiteral();
    } else {
        std::string raw = args[1]->getCExpr(e);
        x = e.nativeRead(raw, VType::Float64);
    }

    std::string tmp = e.newTemp("ad_grad");
    e.emit("double " + tmp + " = 0.0;");
    e.emit(info->adjointName + "(" + x + ", 1.0, &" + tmp + ");");
    e.declareLocal(tmp, CType::fromVType(VType::Float64));
    return tmp;
}

std::string emitBackwardCall(C_Emitter& e, const FunctionCallNode& call) {    const auto& args = call.getArguments();
    if (args.size() != 1) {
        throw std::runtime_error(
            "Compile Error (VNE-126): backward(seed) takes exactly 1 "
            "argument (line " +
            std::to_string(call.lineNumber) + ")."
        );
    }
    if (args[0]->type() != NodeType::VARIABLE) {
        throw std::runtime_error(
            "Compile Error (VNE-126): backward(seed) expects the variable "
            "bound to a @differentiable call (line " +
            std::to_string(call.lineNumber) + ")."
        );
    }

    auto* var = static_cast<const VariableNode*>(args[0].get());
    const std::string binding = var->getOriginalName();
    const C_Emitter::BackwardCall* rec = e.lookupBackwardCall(binding);
    if (!rec) {
        throw std::runtime_error(
            "Compile Error (VNE-126): '" + binding +
            "' was not bound to a @differentiable call inside this "
            "@backward region (line " +
            std::to_string(call.lineNumber) + ")."
        );
    }

    const C_Emitter::ADInfo* info = e.lookupDifferentiable(rec->callee);
    if (!info) {
        throw std::runtime_error(
            "Compile Error (VNE-126): callee '" + rec->callee +
            "' is not @differentiable (line " +
            std::to_string(call.lineNumber) + ")."
        );
    }
    if (rec->args.size() != info->arity) {
        throw std::runtime_error(
            "Compile Error (VNE-126): recorded call arity does not match "
            "the @differentiable signature (line " +
            std::to_string(call.lineNumber) + ")."
        );
    }
    // Custom `<fn>_vjp` adjoint: call it with the original boxed arguments.
    // The custom function owns its gradient storage (for example a global
    // Grads object filled by vconv backward kernels).
    if (info->custom) {
        std::vector<std::string> vals;
        vals.reserve(rec->args.size());
        for (const ASTNode* a : rec->args)
            vals.push_back(e.boxAny(a->getCExpr(e)));
        std::string arr = e.newTemp("vjp_args");
        e.emit(
            "VyneValue " + arr + "[" + std::to_string(vals.size()) + "];"
        );
        for (size_t i = 0; i < vals.size(); ++i)
            e.emit(arr + "[" + std::to_string(i) + "] = " + vals[i] + ";");
        e.emit(
            info->adjointName + "(" + std::to_string(vals.size()) + ", " +
            arr + ");"
        );
        return "vyne_null()";
    }
    for (VType t : info->paramTypes) {
        if (t == VType::Array) {
            throw std::runtime_error(
                "Compile Error (VNE-126): backward() on array parameters is "
                "deferred; use `grad(f, x)` for Array<Float64> gradients "
                "(line " +
                std::to_string(call.lineNumber) + ")."
            );
        }
    }

    std::vector<std::string> argExprs;
    std::vector<std::string> argVars;
    std::vector<std::string> grads;

    for (const ASTNode* a : rec->args) {
        if (a->type() != NodeType::VARIABLE) {
            throw std::runtime_error(
                "Compile Error (VNE-126): in this phase backward() requires "
                "every argument to the @differentiable call to be a plain "
                "variable (line " +
                std::to_string(call.lineNumber) + ")."
            );
        }
        auto* v = static_cast<const VariableNode*>(a);
        std::string raw = a->getCExpr(e);
        argExprs.push_back(e.nativeRead(raw, VType::Float64));
        argVars.push_back(v->getOriginalName());

        std::string g = e.newTemp("ad_d_" + sanitizeName(v->getOriginalName()));
        e.emit("double " + g + " = 0.0;");
        e.declareLocal(g, CType::fromVType(VType::Float64));
        grads.push_back(g);
    }

    std::string callText = info->adjointName + "(";
    for (size_t i = 0; i < argExprs.size(); ++i) {
        if (i)
            callText += ", ";
        callText += argExprs[i];
    }
    callText += ", 1.0";
    for (const auto& g : grads)
        callText += ", &" + g;
    callText += ");";
    e.emit(callText);

    for (size_t i = 0; i < argVars.size(); ++i)
        e.registerGradientAccess(argVars[i], grads[i]);

    return "vyne_null()";
}

// ---------------------------------------------------------------------------
// `@implicit` region lowering
// ---------------------------------------------------------------------------

namespace {

std::string joinComma(const std::vector<std::string>& items) {
    std::string out;
    for (size_t i = 0; i < items.size(); ++i) {
        if (i)
            out += ", ";
        out += items[i];
    }
    return out;
}

std::string implicitDoubleExpr(C_Emitter& e, const ASTNode* node) {
    if (!node)
        return "0.0";
    if (node->type() == NodeType::NUMBER)
        return static_cast<const NumberNode*>(node)->nativeLiteral();
    std::string raw = node->getCExpr(e);
    return e.nativeRead(raw, VType::Float64);
}

std::string residualNameFromCall(const FunctionCallNode& call) {
    const auto& args = call.getArguments();
    if (args.empty() || !args[0])
        return {};
    if (args[0]->type() == NodeType::VARIABLE)
        return static_cast<const VariableNode*>(args[0].get())
            ->getOriginalName();
    if (args[0]->type() == NodeType::MEMBER_ACCESS)
        return static_cast<const MemberAccessNode*>(args[0].get())
            ->getFullPath();
    return {};
}

std::string mangleFromAdjoint(const std::string& adjointName) {
    const std::string prefix = "fn_";
    const std::string suffix = "_grad";
    if (adjointName.size() <= prefix.size() + suffix.size())
        return {};
    if (adjointName.compare(0, prefix.size(), prefix) != 0)
        return {};
    if (adjointName.compare(
            adjointName.size() - suffix.size(),
            suffix.size(),
            suffix
        ) != 0)
        return {};
    return adjointName.substr(
        prefix.size(),
        adjointName.size() - prefix.size() - suffix.size()
    );
}

} // namespace

bool isImplicitSolveName(const std::string& name) {
    return name == "newton_solve" || name == "implicit_solve";
}

std::string emitImplicitSolveCall(C_Emitter& e, const FunctionCallNode& call) {
    const auto& args = call.getArguments();
    const int line = call.lineNumber;

    if (args.size() < 2) {
        throw std::runtime_error(
            "Compile Error (VNE-127): newton_solve(F, x..., z0) needs at "
            "least a residual and an initial guess (line " +
            std::to_string(line) + ")."
        );
    }

    std::string residualName = residualNameFromCall(call);
    if (residualName.empty()) {
        throw std::runtime_error(
            "Compile Error (VNE-127): the first argument of newton_solve() "
            "must name a @differentiable residual function (line " +
            std::to_string(line) + ")."
        );
    }

    C_Emitter::ImplicitSolve* rec = e.findImplicitSolveByNode(&call);
    if (!rec) {
        throw std::runtime_error(
            "Compile Error (VNE-127): newton_solve() must be the direct "
            "right-hand side of a fresh ':=' declaration inside an "
            "@implicit region (line " +
            std::to_string(line) + ").\n"
            "  Example: z :: Float64 := newton_solve(F, x, z0);"
        );
    }
    if (rec->filled)
        return rec->stateCName;

    const C_Emitter::ADInfo* info = e.lookupDifferentiable(residualName);
    if (!info) {
        throw std::runtime_error(
            "Compile Error (VNE-127): newton_solve() residual '" +
            residualName +
            "' is not @differentiable or has not been seen yet (line " +
            std::to_string(line) + ")."
        );
    }
    if (info->custom) {
        throw std::runtime_error(
            "Compile Error (VNE-127): newton_solve() needs the compiler "
            "adjoint of '" +
            residualName +
            "'; a custom <fn>_vjp cannot expose dF/dz for the implicit "
            "backward pass (line " +
            std::to_string(line) + ")."
        );
    }

    const size_t residualArity = args.size() - 1;
    if (info->arity != residualArity) {
        throw std::runtime_error(
            "Compile Error (VNE-127): newton_solve() passes " +
            std::to_string(residualArity) + " values to residual '" +
            residualName + "', but the residual takes " +
            std::to_string(info->arity) +
            " parameters (x..., z) (line " + std::to_string(line) + ")."
        );
    }
    for (VType t : info->paramTypes) {
        if (t != VType::Float64) {
            throw std::runtime_error(
                "Compile Error (VNE-127): this @implicit slice supports "
                "scalar Float64 residues only; residual '" +
                residualName + "' has a non-Float64 parameter (line " +
                std::to_string(line) + ")."
            );
        }
    }

    // The parameter arguments come before z0. They must be plain variables
    // so `.grad` can be published on the source names.
    const size_t paramCount = args.size() - 2;
    std::vector<std::string> paramVars;
    paramVars.reserve(paramCount);
    for (size_t i = 0; i < paramCount; ++i) {
        const ASTNode* a = args[1 + i].get();
        if (!a || a->type() != NodeType::VARIABLE) {
            throw std::runtime_error(
                "Compile Error (VNE-127): newton_solve() parameter " +
                std::to_string(i) +
                " must be a plain variable so its .grad slot can be "
                "published (line " +
                std::to_string(line) + ")."
            );
        }
        paramVars.push_back(
            static_cast<const VariableNode*>(a)->getOriginalName()
        );
    }

    const std::string mangle = mangleFromAdjoint(info->adjointName);
    const std::string* native =
        mangle.empty() ? nullptr : e.lookupNativeVariant(mangle);
    if (!native || native->empty()) {
        throw std::runtime_error(
            "Compile Error (VNE-127): residual '" + residualName +
            "' has no native Float64 variant, so newton_solve() cannot "
            "evaluate it without a boxed tape (line " +
            std::to_string(line) +
            ").\n"
            "  Keep the residual signature Float64 -> Float64 and avoid "
            "defer/try in its body."
        );
    }

    e.emit(
        "// --- @implicit solve: " + rec->binding + " := newton_solve(" +
        residualName + ", ...) ---"
    );

    std::vector<std::string> paramTemps;
    paramTemps.reserve(paramCount);
    for (size_t i = 0; i < paramCount; ++i) {
        std::string val = implicitDoubleExpr(e, args[1 + i].get());
        std::string t = e.newTemp("imp_x" + std::to_string(i));
        e.emit("double " + t + " = " + val + ";");
        e.declareLocal(t, CType::fromVType(VType::Float64));
        paramTemps.push_back(t);
    }

    std::string z0 = implicitDoubleExpr(e, args.back().get());
    std::string state = e.newTemp("imp_z");
    e.emit("double " + state + " = " + z0 + ";");
    e.declareLocal(state, CType::fromVType(VType::Float64));

    std::string res = e.newTemp("imp_res");
    std::string ok = e.newTemp("imp_ok");
    std::string it = e.newTemp("imp_it");
    e.emit("double " + res + " = 0.0;");
    e.emit("int " + ok + " = 0;");

    std::vector<std::string> fwdArgs = paramTemps;
    fwdArgs.push_back(state);

    const int maxIters = e.currentImplicitMaxIters();
    const std::string nativeFn = "fn_" + *native;
    e.emit(
        "for (int " + it + " = 0; " + it + " < " + std::to_string(maxIters) +
        "; ++" + it + ") {"
    );
    e.emit(
        "    " + res + " = " + nativeFn + "(" + joinComma(fwdArgs) + ");"
    );
    e.emit(
        "    if (fabs(" + res + ") <= 1e-12) { " + ok + " = 1; break; }"
    );

    std::vector<std::string> dTemps;
    dTemps.reserve(residualArity);
    for (size_t i = 0; i < residualArity; ++i) {
        std::string d = (i + 1 == residualArity)
                            ? e.newTemp("imp_dz")
                            : e.newTemp("imp_d" + std::to_string(i));
        e.emit("    double " + d + " = 0.0;");
        dTemps.push_back(d);
    }
    const std::string& dz = dTemps.back();

    std::vector<std::string> adjArgs = fwdArgs;
    adjArgs.push_back("1.0");
    for (const auto& d : dTemps)
        adjArgs.push_back("&" + d);
    e.emit("    " + info->adjointName + "(" + joinComma(adjArgs) + ");");
    e.emit(
        "    if (fabs(" + dz + ") < 1e-14) { " + ok + " = -1; break; }"
    );
    e.emit("    " + state + " -= " + res + " / " + dz + ";");
    e.emit("}");

    const std::string regionName = e.currentImplicitRegionName();
    e.emit(
        "if (" + ok + " == 0) { vyne_panic_implicit(\"" + regionName + "\", " +
        std::to_string(line) + ", \"did not converge in " +
        std::to_string(maxIters) + " Newton iterations\"); }"
    );
    e.emit(
        "if (" + ok + " < 0) { vyne_panic_implicit(\"" + regionName + "\", " +
        std::to_string(line) +
        ", \"has a singular residual Jacobian (dF/dz == 0)\"); }"
    );

    rec->residualName = std::move(residualName);
    rec->adjointName = info->adjointName;
    rec->nativeName = nativeFn;
    rec->stateCName = state;
    rec->paramVars = std::move(paramVars);
    rec->paramCNames = std::move(paramTemps);
    rec->line = line;
    rec->filled = true;

    return state;
}

std::string
emitImplicitBackwardCall(C_Emitter& e, const FunctionCallNode& call) {
    const auto& args = call.getArguments();
    const int line = call.lineNumber;

    if (args.empty() || args.size() > 2) {
        throw std::runtime_error(
            "Compile Error (VNE-128): backward(root[, seed]) takes one or "
            "two arguments inside an @implicit region (line " +
            std::to_string(line) + ")."
        );
    }
    if (args[0]->type() != NodeType::VARIABLE) {
        throw std::runtime_error(
            "Compile Error (VNE-128): backward() expects the variable that "
            "received a newton_solve() root (line " +
            std::to_string(line) + ")."
        );
    }

    auto* var = static_cast<const VariableNode*>(args[0].get());
    const C_Emitter::ImplicitSolve* rec =
        e.lookupImplicitSolve(var->getOriginalName());
    if (!rec || !rec->filled) {
        throw std::runtime_error(
            "Compile Error (VNE-128): '" + var->getOriginalName() +
            "' was not bound to a newton_solve() call inside this "
            "@implicit region (line " +
            std::to_string(line) + ")."
        );
    }

    std::string seed = e.newTemp("imp_seed");
    e.emit(
        "double " + seed + " = " +
        (args.size() == 2 ? implicitDoubleExpr(e, args[1].get()) : "1.0") +
        ";"
    );
    e.declareLocal(seed, CType::fromVType(VType::Float64));

    const size_t residualArity = rec->paramCNames.size() + 1;
    std::vector<std::string> dTemps;
    dTemps.reserve(residualArity);
    for (size_t i = 0; i < residualArity; ++i) {
        std::string d = (i + 1 == residualArity)
                            ? e.newTemp("imp_bwd_dz")
                            : e.newTemp("imp_bwd_d" + std::to_string(i));
        e.emit("double " + d + " = 0.0;");
        e.declareLocal(d, CType::fromVType(VType::Float64));
        dTemps.push_back(d);
    }
    const std::string& dz = dTemps.back();

    std::vector<std::string> fwdArgs = rec->paramCNames;
    fwdArgs.push_back(rec->stateCName);
    std::vector<std::string> adjArgs = fwdArgs;
    adjArgs.push_back("1.0");
    for (const auto& d : dTemps)
        adjArgs.push_back("&" + d);
    e.emit(
        "// --- @implicit IFT backward: " + rec->binding + " -> parameters ---"
    );
    e.emit(rec->adjointName + "(" + joinComma(adjArgs) + ");");
    e.emit(
        "if (fabs(" + dz + ") < 1e-14) { vyne_panic_implicit(\"" +
        e.currentImplicitRegionName() + "\", " + std::to_string(line) +
        ", \"has a singular residual Jacobian (dF/dz == 0)\"); }"
    );

    for (size_t i = 0; i < rec->paramVars.size(); ++i) {
        std::string g = e.newTemp(
            "imp_grad_" + sanitizeName(rec->paramVars[i])
        );
        e.emit(
            "double " + g + " = -(" + seed + " * " + dTemps[i] + ") / " + dz +
            ";"
        );
        e.declareLocal(g, CType::fromVType(VType::Float64));
        e.registerGradientAccess(rec->paramVars[i], g);
    }

    // A downstream chain can read the seed as the root's own gradient.
    e.registerGradientAccess(rec->binding, seed);

    return "vyne_null()";
}

} // namespace vyne_ad

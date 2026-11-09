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
};

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
    std::vector<LocalDef> locals;
    const ASTNode* ret = nullptr;
    std::string why;
    if (analyzeBody(fn, locals, ret, why)) {
        return true;
    }
    if (reason)
        *reason = why;
    return false;
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
        s += "double " + valueName(gradName, p.name);
    }
    if (!first)
        s += ", ";
    s += "double __d_out";

    for (const auto& p : fn.getParameters())
        s += ", double* __d_" + sanitizeName(p.name);

    s += ")";
    return s;
}

void emitAdjoint(
    C_Emitter& e,
    const FunctionNode& fn,
    const std::string& mangledName
) {
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

std::string emitBackwardCall(C_Emitter& e, const FunctionCallNode& call) {
    const auto& args = call.getArguments();
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

} // namespace vyne_ad

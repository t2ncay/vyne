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
#include <stdexcept>

// Binary, unary and postfix operations; operands may emit prerequisite
// statements. Keep expressions that emit statements in evaluation order; see
// README.md.
// ============================================================
// BINARY / UNARY / POSTFIX
// ============================================================

std::string BinOpNode::getCExpr(C_Emitter& e) const {
    // Materialize both operands first — their getCExpr() may emit statements.
    std::string l = leftNode->getCExpr(e);
    std::string r = rightNode->getCExpr(e);
    std::string temp = e.newTemp("bin");

    // Effective static operand types: node-reported first; when a reference
    // reports Unknown (plain variable reads do), fall back to the emitter's
    // native type table so unboxed locals still take the fast path.
    VType lt = leftNode->getStaticType();
    if (lt == VType::Unknown) {
        if (const CType* ct = e.exprNativeType(l))
            lt = ct->toVType();
    }
    VType rt = rightNode->getStaticType();
    if (rt == VType::Unknown) {
        if (const CType* ct = e.exprNativeType(r))
            rt = ct->toVType();
    }

    // Shape consistency check for `+` between two shaped arrays.
    // Same-shape scratch addition: lower to an element-wise loop.
    if (op == VTokenType::Add) {
        const CType* lct = e.lookupAnyType(l);
        const CType* rct = e.lookupAnyType(r);
        if (lct && rct && lct->hasShape() && rct->hasShape() &&
            lct->sameShape(*rct) && !lct->args.empty()) {

            VType elem = lct->args[0].toVType();
            const char* ct = (elem == VType::Float64) ? "double" : "int64_t";
            int64_t n = lct->numElements();

            std::string dst = e.newTemp("arrsum");
            std::string k = e.newTemp("k");

            e.emit(
                std::string(ct) + " " + dst + "[" + std::to_string(n) + "];"
            );
            e.emit(
                "for (int64_t " + k + " = 0; " + k + " < " + std::to_string(n) +
                "; ++" + k + ") {"
            );
            e.emit(
                "    " + dst + "[" + k + "] = " + l + "[" + k + "] + " + r +
                "[" + k + "];"
            );
            e.emit("}");

            CType outCT = *lct;
            e.declareNativeTemp(dst, outCT);
            return dst;
        }
    }

    // Read an operand as a native value of static type `st`:
    //  - numeric literal → raw literal
    //  - registered native expression of the same type → used directly
    //  - native of the other numeric kind → cast
    //  - boxed VyneValue → union member read
    auto operand = [&](const ASTNode* node,
                       const std::string& expr,
                       VType st) -> std::string {
        if (node->type() == NodeType::NUMBER && st != VType::Unknown) {
            auto* num = static_cast<const NumberNode*>(node);
            if ((st == VType::Int64 && num->getStaticType() == VType::Int64) ||
                st == VType::Float64) {
                return num->nativeLiteral();
            }
        }
        if (st == VType::Int64 || st == VType::Float64) {
            if (const CType* ct = e.exprNativeType(expr)) {
                if (ct->toVType() == st)
                    return expr;
                if (ct->kind == CType::Kind::Int64 && st == VType::Float64)
                    return "(double)(" + expr + ")";
            }
            return "(" + expr + ")." +
                   (st == VType::Float64 ? "as.f64" : "as.i64");
        }
        return expr;
    };

    // =========================================================
    // FAST PATH: Int64 op Int64 → native int64_t result
    // =========================================================
    if (lt == VType::Int64 && rt == VType::Int64) {
        std::string lv = operand(leftNode.get(), l, VType::Int64);
        std::string rv = operand(rightNode.get(), r, VType::Int64);
        switch (op) {
        case VTokenType::Add:
            e.emit("int64_t " + temp + " = " + lv + " + " + rv + ";");
            e.declareNativeTemp(temp, CType::fromVType(VType::Int64));
            return temp;
        case VTokenType::Substract:
            e.emit("int64_t " + temp + " = " + lv + " - " + rv + ";");
            e.declareNativeTemp(temp, CType::fromVType(VType::Int64));
            return temp;
        case VTokenType::Multiply:
            e.emit("int64_t " + temp + " = " + lv + " * " + rv + ";");
            e.declareNativeTemp(temp, CType::fromVType(VType::Int64));
            return temp;
        case VTokenType::Division:
        case VTokenType::Floor_Divide:
            e.emit(
                "if (" + rv +
                " == 0) { fprintf(stderr, \"Runtime error: Division by "
                "zero!\\n\"); exit(1); }"
            );
            e.emit("int64_t " + temp + " = " + lv + " / " + rv + ";");
            e.declareNativeTemp(temp, CType::fromVType(VType::Int64));
            return temp;
        case VTokenType::Modulo:
            e.emit(
                "if (" + rv +
                " == 0) { fprintf(stderr, \"Runtime error: Modulo by "
                "zero!\\n\"); exit(1); }"
            );
            e.emit("int64_t " + temp + " = " + lv + " % " + rv + ";");
            e.declareNativeTemp(temp, CType::fromVType(VType::Int64));
            return temp;
        case VTokenType::Power:
            e.emit(
                "VyneValue " + temp + " = vyne_float(pow((double)" + lv +
                ", (double)" + rv + "));"
            );
            return temp;
        case VTokenType::Double_Equals:
            e.emit(
                "VyneValue " + temp + " = vyne_bool(" + lv + " == " + rv + ");"
            );
            return temp;
        case VTokenType::Not_Equal:
            e.emit(
                "VyneValue " + temp + " = vyne_bool(" + lv + " != " + rv + ");"
            );
            return temp;
        case VTokenType::Greater:
            e.emit(
                "VyneValue " + temp + " = vyne_bool(" + lv + " > " + rv + ");"
            );
            return temp;
        case VTokenType::Smaller:
            e.emit(
                "VyneValue " + temp + " = vyne_bool(" + lv + " < " + rv + ");"
            );
            return temp;
        case VTokenType::Greater_Or_Equal:
            e.emit(
                "VyneValue " + temp + " = vyne_bool(" + lv + " >= " + rv + ");"
            );
            return temp;
        case VTokenType::Smaller_Or_Equal:
            e.emit(
                "VyneValue " + temp + " = vyne_bool(" + lv + " <= " + rv + ");"
            );
            return temp;
        case VTokenType::And:
            e.emit(
                "VyneValue " + temp + " = vyne_bool((" + lv + " != 0) && (" +
                rv + " != 0));"
            );
            return temp;
        case VTokenType::Or:
            e.emit(
                "VyneValue " + temp + " = vyne_bool((" + lv + " != 0) || (" +
                rv + " != 0));"
            );
            return temp;
        case VTokenType::Bitwise_And:
            e.emit(
                "VyneValue " + temp + " = vyne_int(" + lv + " & " + rv + ");"
            );
            return temp;
        case VTokenType::Bitwise_Or:
            e.emit(
                "VyneValue " + temp + " = vyne_int(" + lv + " | " + rv + ");"
            );
            return temp;
        case VTokenType::Bitwise_Xor:
            e.emit(
                "VyneValue " + temp + " = vyne_int(" + lv + " ^ " + rv + ");"
            );
            return temp;
        case VTokenType::Bitwise_Sll:
            e.emit(
                "VyneValue " + temp + " = vyne_int(" + lv + " << " + rv + ");"
            );
            return temp;
        case VTokenType::Bitwise_Srl:
            e.emit(
                "VyneValue " + temp + " = vyne_int(" + lv + " >> " + rv + ");"
            );
            return temp;
        default:
            break; // fall through to slow path
        }
    }

    // f3rhd : you may change this later based on the semantic rules, for now i
    // assume no bitwise operations if they include any floating numbers
    if (lt == VType::Float64 || rt == VType::Float64) {
        switch (op) {
        case VTokenType::Bitwise_And:
        case VTokenType::Bitwise_Not:
        case VTokenType::Bitwise_Xor:
        case VTokenType::Bitwise_Or:
        case VTokenType::Bitwise_Sll:
        case VTokenType::Bitwise_Srl:
            throw std::runtime_error(
                "Compile Error: Bitwise operators should only be used with "
                "integer values  "
                "(line " +
                std::to_string(lineNumber) + ")."
            );
        default:;
        }
    }
    // =========================================================
    // FAST PATH: Float64 op Float64 → native double result
    // =========================================================
    if (lt == VType::Float64 && rt == VType::Float64) {
        std::string lv = operand(leftNode.get(), l, VType::Float64);
        std::string rv = operand(rightNode.get(), r, VType::Float64);
        switch (op) {
        case VTokenType::Add:
            e.emit("double " + temp + " = " + lv + " + " + rv + ";");
            e.declareNativeTemp(temp, CType::fromVType(VType::Float64));
            return temp;
        case VTokenType::Substract:
            e.emit("double " + temp + " = " + lv + " - " + rv + ";");
            e.declareNativeTemp(temp, CType::fromVType(VType::Float64));
            return temp;
        case VTokenType::Multiply:
            e.emit("double " + temp + " = " + lv + " * " + rv + ";");
            e.declareNativeTemp(temp, CType::fromVType(VType::Float64));
            return temp;
        case VTokenType::Division:
            e.emit(
                "if (" + rv +
                " == 0.0) { fprintf(stderr, \"Runtime error: Division by "
                "zero!\\n\"); exit(1); }"
            );
            e.emit("double " + temp + " = " + lv + " / " + rv + ";");
            e.declareNativeTemp(temp, CType::fromVType(VType::Float64));
            return temp;
        case VTokenType::Modulo:
            e.emit(
                "if (" + rv +
                " == 0.0) { fprintf(stderr, \"Runtime error: Modulo by "
                "zero!\\n\"); exit(1); }"
            );
            e.emit("double " + temp + " = fmod(" + lv + ", " + rv + ");");
            e.declareNativeTemp(temp, CType::fromVType(VType::Float64));
            return temp;
        case VTokenType::Power:
            e.emit("double " + temp + " = pow(" + lv + ", " + rv + ");");
            e.declareNativeTemp(temp, CType::fromVType(VType::Float64));
            return temp;
        case VTokenType::Double_Equals:
            e.emit(
                "VyneValue " + temp + " = vyne_bool(" + lv + " == " + rv + ");"
            );
            return temp;
        case VTokenType::Not_Equal:
            e.emit(
                "VyneValue " + temp + " = vyne_bool(" + lv + " != " + rv + ");"
            );
            return temp;
        case VTokenType::Greater:
            e.emit(
                "VyneValue " + temp + " = vyne_bool(" + lv + " > " + rv + ");"
            );
            return temp;
        case VTokenType::Smaller:
            e.emit(
                "VyneValue " + temp + " = vyne_bool(" + lv + " < " + rv + ");"
            );
            return temp;
        case VTokenType::Greater_Or_Equal:
            e.emit(
                "VyneValue " + temp + " = vyne_bool(" + lv + " >= " + rv + ");"
            );
            return temp;
        case VTokenType::Smaller_Or_Equal:
            e.emit(
                "VyneValue " + temp + " = vyne_bool(" + lv + " <= " + rv + ");"
            );
            return temp;
        default:
            break; // fall through to slow path
        }
    }

    {
        bool ltNum = (lt == VType::Int64 || lt == VType::Float64);
        bool rtNum = (rt == VType::Int64 || rt == VType::Float64);
        if (ltNum && rtNum) {
            std::string lv = operand(leftNode.get(), l, VType::Float64);
            std::string rv = operand(rightNode.get(), r, VType::Float64);
            switch (op) {
            case VTokenType::Add:
                e.emit("double " + temp + " = " + lv + " + " + rv + ";");
                e.declareNativeTemp(temp, CType::fromVType(VType::Float64));
                return temp;
            case VTokenType::Substract:
                e.emit("double " + temp + " = " + lv + " - " + rv + ";");
                e.declareNativeTemp(temp, CType::fromVType(VType::Float64));
                return temp;
            case VTokenType::Multiply:
                e.emit("double " + temp + " = " + lv + " * " + rv + ";");
                e.declareNativeTemp(temp, CType::fromVType(VType::Float64));
                return temp;
            case VTokenType::Division:
                e.emit(
                    "if (" + rv +
                    " == 0.0) { fprintf(stderr, \"Runtime error: Division by "
                    "zero!\\n\"); exit(1); }"
                );
                e.emit("double " + temp + " = " + lv + " / " + rv + ";");
                e.declareNativeTemp(temp, CType::fromVType(VType::Float64));
                return temp;
            case VTokenType::Modulo:
                e.emit(
                    "if (" + rv +
                    " == 0.0) { fprintf(stderr, \"Runtime error: Modulo by "
                    "zero!\\n\"); exit(1); }"
                );
                e.emit("double " + temp + " = fmod(" + lv + ", " + rv + ");");
                e.declareNativeTemp(temp, CType::fromVType(VType::Float64));
                return temp;
            case VTokenType::Power:
                e.emit("double " + temp + " = pow(" + lv + ", " + rv + ");");
                e.declareNativeTemp(temp, CType::fromVType(VType::Float64));
                return temp;
            case VTokenType::Double_Equals:
                e.emit(
                    "VyneValue " + temp + " = vyne_bool(" + lv + " == " + rv +
                    ");"
                );
                return temp;
            case VTokenType::Not_Equal:
                e.emit(
                    "VyneValue " + temp + " = vyne_bool(" + lv + " != " + rv +
                    ");"
                );
                return temp;
            case VTokenType::Greater:
                e.emit(
                    "VyneValue " + temp + " = vyne_bool(" + lv + " > " + rv +
                    ");"
                );
                return temp;
            case VTokenType::Smaller:
                e.emit(
                    "VyneValue " + temp + " = vyne_bool(" + lv + " < " + rv +
                    ");"
                );
                return temp;
            case VTokenType::Greater_Or_Equal:
                e.emit(
                    "VyneValue " + temp + " = vyne_bool(" + lv + " >= " + rv +
                    ");"
                );
                return temp;
            case VTokenType::Smaller_Or_Equal:
                e.emit(
                    "VyneValue " + temp + " = vyne_bool(" + lv + " <= " + rv +
                    ");"
                );
                return temp;
            default:
                break; // AND / OR / Floor_Divide → slow path
            }
        }
    }

    // =========================================================
    // SLOW PATH: dynamic dispatch through vyne_binop()
    // =========================================================
    int opCode = static_cast<int>(op);
    switch (op) {
    case VTokenType::Add:
        opCode = 29;
        break;
    case VTokenType::Substract:
        opCode = 30;
        break;
    case VTokenType::Multiply:
        opCode = 31;
        break;
    case VTokenType::Division:
        opCode = 32;
        break;
    case VTokenType::Modulo:
        opCode = 36;
        break;
    case VTokenType::Power:
        opCode = 37;
        break;
    case VTokenType::Double_Equals:
        opCode = 43;
        break;
    case VTokenType::Not_Equal:
        opCode = 44;
        break;
    case VTokenType::Greater:
        opCode = 45;
        break;
    case VTokenType::Smaller:
        opCode = 46;
        break;
    case VTokenType::Greater_Or_Equal:
        opCode = 47;
        break;
    case VTokenType::Smaller_Or_Equal:
        opCode = 48;
        break;
    case VTokenType::And:
        opCode = 49;
        break;
    case VTokenType::Or:
        opCode = 50;
        break;
    case VTokenType::Floor_Divide:
        opCode = 51;
        break;
    case VTokenType::Bitwise_Not:
        opCode = 52;
        break;
    case VTokenType::Bitwise_And:
        opCode = 53;
        break;
    case VTokenType::Bitwise_Or:
        opCode = 54;
        break;
    case VTokenType::Bitwise_Xor:
        opCode = 55;
        break;
    case VTokenType::Bitwise_Sll:
        opCode = 56;
        break;
    case VTokenType::Bitwise_Srl:
        opCode = 57;
        break;
    default:
        break;
    }

    e.emit(
        "VyneValue " + temp + " = vyne_binop(" + e.boxAny(l) + ", " +
        e.boxAny(r) + ", " + std::to_string(opCode) + ");"
    );
    return temp;
}

void BinOpNode::compile(C_Emitter& e) const {
    getCExpr(e);
}

std::string UnaryNode::getCExpr(C_Emitter& e) const {
    if (op == VTokenType::Addresser) {
        throw std::runtime_error(
            "Compile Error: '&' (address-of) is not supported by the C backend "
            "(line " +
            std::to_string(lineNumber) + "). Use the interpreter instead."
        );
    }
    /*
        f3rhd:
        normally this conditional should be checked but due to type info loss it
       will alert false positives and terminate the compiler fn main() { x ::
       Int64 = 32;

            y :: Int64 = ~((~((x >> 3) << 3) + 1)) | 64; # y = 95

            z :: Int64 = (y ^ 15) & 112;                # z = 80
            if z == 80 {
                out ("Test passed");
            }
            else {
                out("Test failed");
            }
        }
        main();
        X variable's type in the second line evaluates to unknown for some
       reason. and because of that in the condition below becomes true and
       throws a compiler error. Uncomment those once type problem is fixed
    */
    if (op == VTokenType::Bitwise_Not &&
        right->getStaticType() != VType::Int64) {
        throw std::runtime_error(
            "Compile Error: Bitwise operators should only be used with integer "
            "values  "
            "(line " +
            std::to_string(lineNumber) + ")."
        );
    }
    std::string val = e.boxAny(right->getCExpr(e));
    std::string temp = e.newTemp("un");
    int opCode = static_cast<int>(op);

    if (op == VTokenType::Exclamatory)
        opCode = 44;
    else if (op == VTokenType::Substract)
        opCode = 30;
    else if (op == VTokenType::Bitwise_Not)
        opCode = 52; // enum value of bitwise not in codegen/operators.h

    e.emit(
        "VyneValue " + temp + " = vyne_unary(" + val + ", " +
        std::to_string(opCode) + ");"
    );
    return temp;
}

void UnaryNode::compile(C_Emitter& e) const {
    getCExpr(e);
}

std::string PostFixNode::getCExpr(C_Emitter& e) const {
    std::string temp = e.newTemp("post");

    // x.field++ / x.field-- : struct fields are reached via vyne_struct_get,
    // which returns by value, so we cannot do `....as.i64++` directly.
    // Read, bump, write back.
    if (left->type() == NodeType::MEMBER_ACCESS) {
        auto* memNode = static_cast<MemberAccessNode*>(left.get());
        std::string recv = memNode->getReceiver()->getCExpr(e);
        uint32_t fid = StringPool::intern(memNode->getMemberName());
        std::string fname = memNode->getMemberName();

        e.emit(
            "VyneValue " + temp + " = vyne_struct_get(" + recv + ", " +
            std::to_string(fid) + ");"
        );

        std::string newV = e.newTemp("postn");
        int opc = (op == VTokenType::Double_Increment) ? 29 : 30; // ADD / SUB
        e.emit(
            "VyneValue " + newV + " = vyne_binop(" + temp + ", vyne_int(1), " +
            std::to_string(opc) + ");"
        );

        e.emit(
            "vyne_struct_set(" + recv + ", " + std::to_string(fid) + ", \"" +
            fname + "\", " + newV + ");"
        );
        return temp;
    }

    // Plain variable lvalue: the existing in-place update is fine.
    std::string var = left->getCExpr(e);
    const CType* nativeT = e.exprNativeType(var);
    if (nativeT && nativeT->isPrimitive()) {
        // M1: native int64_t/double local — box the old value, bump in place.
        e.emit("VyneValue " + temp + " = " + nativeT->box(var) + ";");
        if (op == VTokenType::Double_Increment) {
            e.emit(var + "++;");
        } else {
            e.emit(var + "--;");
        }
        return temp;
    }
    e.emit("VyneValue " + temp + " = " + var + ";");
    if (op == VTokenType::Double_Increment) {
        e.emit("if (" + var + ".type == V_INT64)   " + var + ".as.i64++;");
        e.emit("else if (" + var + ".type == V_FLOAT64) " + var + ".as.f64++;");
    } else {
        e.emit("if (" + var + ".type == V_INT64)   " + var + ".as.i64--;");
        e.emit("else if (" + var + ".type == V_FLOAT64) " + var + ".as.f64--;");
    }
    return temp;
}

void PostFixNode::compile(C_Emitter& e) const {
    getCExpr(e);
}

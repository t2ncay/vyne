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

// If/while and return/break/continue; preserves deferred cleanup control flow.
// Keep expressions that emit statements in evaluation order; see README.md.
// ============================================================
// CONTROL FLOW
// ============================================================

void IfNode::compile(C_Emitter& e) const {
    std::string cond = emitNativeCondition(e, condition.get());
    e.emitBlockOpen("if (" + cond + ") {");
    if (body)
        body->compile(e);
    e.emitBlockClose();
    if (elseBody) {
        e.emitBlockOpen("else {");
        elseBody->compile(e);
        e.emitBlockClose();
    }
}

std::string IfNode::getCExpr(C_Emitter& e) const {
    std::string temp = e.newTemp("ifres");
    e.emit("VyneValue " + temp + " = vyne_null();");

    std::string cond = emitNativeCondition(e, condition.get());
    e.emitBlockOpen("if (" + cond + ") {");
    if (body) {
        std::string bv = e.boxAny(body->getCExpr(e));
        e.emit(temp + " = " + bv + ";");
    }
    e.emitBlockClose();

    if (elseBody) {
        e.emitBlockOpen("else {");
        std::string ev = e.boxAny(elseBody->getCExpr(e));
        e.emit(temp + " = " + ev + ";");
        e.emitBlockClose();
    }

    return temp;
}

void WhileNode::compile(C_Emitter& e) const {
    e.emitBlockOpen("while (1) {");
    std::string cond = emitNativeCondition(e, condition.get());
    e.emit("if (!(" + cond + ")) break;");
    if (body)
        body->compile(e);
    e.emitBlockClose();
}

std::string WhileNode::getCExpr(C_Emitter& e) const {
    compile(e);
    return "vyne_null()";
}

void ReturnNode::compile(C_Emitter& e) const {
    if (e.getNativeReturnType().isPrimitive()) {
        VType retVT = e.getNativeReturnType().toVType();
        std::string raw = expression ? expression->getCExpr(e) : "vyne_null()";
        std::string native = coerceToNative(e, expression.get(), raw, retVT);
        if (e.hasRegion()) {
            e.emit(
                "vmem_runtime_pop_checkpoints(" +
                std::to_string(e.getRegionStack().size()) + ");"
            );
        } else {
            e.emitRegionUnwind(); // no-op if stack is empty anyway
        }
        e.emit("return " + native + ";");
        return;
    }

    // --- Native Struct return (via out-params) -----------------------
    {
        const CType& retCt = e.getNativeReturnType();
        const auto& outParams = e.getStructReturnOutParams();
        if (retCt.kind == CType::Kind::Struct && !outParams.empty()) {
            const auto* layout = e.getInterfaceStructLayout(retCt.mangledName);
            if (!layout) {
                throw std::runtime_error(
                    "Native variant returns Struct '" + retCt.mangledName +
                    "' but no field layout is registered."
                );
            }

            std::string boxed =
                e.boxAny(expression ? expression->getCExpr(e) : "vyne_null()");
            std::string slot = e.newTemp("ret_struct");
            e.emit("VyneValue " + slot + " = " + boxed + ";");

            for (size_t i = 0; i < layout->size(); ++i) {
                const auto& fd = (*layout)[i];
                const std::string& op = outParams[i];
                std::string fieldVal = "vyne_struct_get(" + slot + ", " +
                                       std::to_string(fd.id) + ")";

                if (fd.type.kind == CType::Kind::Int64) {
                    std::string fb = e.newTemp("fb");
                    e.emit("VyneValue " + fb + " = " + fieldVal + ";");
                    e.emit(
                        "*" + op + " = (" + fb + ".type == V_INT64) ? " + fb +
                        ".as.i64 : (int64_t)" + fb + ".as.f64;"
                    );
                } else if (fd.type.kind == CType::Kind::Float64) {
                    std::string fb = e.newTemp("fb");
                    e.emit("VyneValue " + fb + " = " + fieldVal + ";");
                    e.emit(
                        "*" + op + " = (" + fb + ".type == V_FLOAT64) ? " + fb +
                        ".as.f64 : (double)" + fb + ".as.i64;"
                    );
                } else if (
                    fd.type.kind == CType::Kind::Array && !fd.type.args.empty()
                ) {
                    bool isF64 = (fd.type.args[0].toVType() == VType::Float64);
                    std::string conv = isF64 ? "vyne_value_to_array_f64"
                                             : "vyne_value_to_array_i64";
                    e.emit("*" + op + " = " + conv + "(" + fieldVal + ");");
                }
            }

            if (e.hasRegion()) {
                e.emit(
                    "vmem_runtime_pop_checkpoints(" +
                    std::to_string(e.getRegionStack().size()) + ");"
                );
            } else {
                e.emitRegionUnwind();
            }
            e.emit("return;");
            return;
        }
    }

    // --- Native Array<T> return -------------------------------------
    // A native variant declared to return Array<Float64> or Array<Int64>
    // emits a C function whose return type is VyneArray_f64 / VyneArray_i64.
    // The body, however, was lowered by AssignmentNode::compile, which does
    // not yet know that the enclosing function is a native-array producer —
    // it produces a boxed VyneValue. Convert at the return boundary using
    // the same runtime helper MemberAccessNode uses to unbox Array<Float64>
    // fields on a struct. If the expression already has a typed-array
    // CType in the emitter's table, pass it through unchanged.
    {
        const CType& retCt = e.getNativeReturnType();
        if (retCt.kind == CType::Kind::Array && !retCt.args.empty()) {
            VType elem = retCt.args[0].toVType();

            // Empty-array literal in a native array producer: emit the
            // typed container directly. The native function's return
            // type is already VyneArray_f64 / VyneArray_i64, so the
            // box-and-unbox round trip the generic path would take is
            // pure waste. Check this before evaluating the RHS, because
            // ArrayNode::getCExpr on `[]` returns a boxed empty container.
            if (expression && expression->type() == NodeType::ARRAY) {
                auto* arr = static_cast<const ArrayNode*>(expression.get());
                if (arr->getElements().empty() &&
                    (elem == VType::Float64 || elem == VType::Int64)) {
                    std::string ctor = (elem == VType::Float64)
                                           ? "vyne_array_f64_create"
                                           : "vyne_array_i64_create";
                    std::string native = e.newTemp("ret_arr");
                    e.emit(
                        CType::arrayContainerName(elem) + " " + native + " = " +
                        ctor + "(0);"
                    );
                    e.emitRegionUnwind();
                    e.emit("return " + native + ";");
                    return;
                }
            }

            std::string raw =
                expression ? expression->getCExpr(e) : "vyne_array_create(0)";

            const CType* got = e.lookupType(raw);
            std::string native;
            if (got && got->kind == CType::Kind::Array && !got->args.empty() &&
                got->args[0].toVType() == elem) {
                // Already a typed container — use it directly.
                native = raw;
            } else {
                // Boxed VyneValue — unbox at the ABI boundary.
                const char* fn = (elem == VType::Float64)
                                     ? "vyne_value_to_array_f64"
                                     : "vyne_value_to_array_i64";
                native = std::string(fn) + "(" + e.boxAny(raw) + ")";
            }

            e.emitRegionUnwind();
            e.emit("return " + native + ";");
            return;
        }
    }
    // --- end native Array<T> return ---------------------------------
    std::string expr;

    // --- Empty-array return in a function declared to return Array<T> ---
    // `return [];` in `fn f() -> Array<Float64>` should produce a typed
    // empty array, not a boxed one — matching the semantics of every
    // other construction path that already specialises `[]` to the
    // declared element type.
    if (expression && expression->type() == NodeType::ARRAY) {
        auto* arr = static_cast<const ArrayNode*>(expression.get());
        if (arr->getElements().empty()) {
            const CType& retCt = e.getCurrentFunctionReturnType();
            if (retCt.kind == CType::Kind::Array && !retCt.args.empty()) {
                VType elem = retCt.args[0].toVType();
                if (elem == VType::Float64 || elem == VType::Int64) {
                    std::string arrName = e.newTemp("ret_arr");
                    std::string ctor = (elem == VType::Float64)
                                           ? "vyne_array_f64_create"
                                           : "vyne_array_i64_create";
                    e.emit(
                        CType::arrayContainerName(elem) + " " + arrName +
                        " = " + ctor + "(0);"
                    );

                    std::string boxFn = (elem == VType::Float64)
                                            ? "vyne_array_f64_to_value"
                                            : "vyne_array_i64_to_value";
                    expr = e.newTemp("ret_boxed");
                    e.emit(
                        "VyneValue " + expr + " = " + boxFn + "(&" + arrName +
                        ");"
                    );
                }
            }
        }
    }

    if (expr.empty()) {
        expr = expression ? e.boxAny(expression->getCExpr(e)) : "vyne_null()";
    }

    // Every region we're lexically inside at the point of this return.
    // A `return` exits all of them; the question is whether we can safely
    // rewind the arena on the way out.
    size_t nRegions = e.getRegionStack().size();

    // Only primitives survive a rewind: Int64 / Float64 / Bool / Null are
    // copied by value. Array / Map / String / Struct are references into
    // the arena and would dangle.
    //
    // VType::Unknown means the static type isn't provable — treat it as
    // unsafe, matching the "box on uncertainty" rule elsewhere in this file.
    VType retType = expression ? expression->getStaticType() : VType::Null;
    bool primitiveSafe =
        (retType == VType::Int64 || retType == VType::Float64 ||
         retType == VType::Bool || retType == VType::Null);

    auto emitRegionCleanup = [&]() {
        if (nRegions == 0)
            return;
        if (primitiveSafe) {
            e.emitRegionUnwind();
        } else {
            e.emit(
                "vmem_runtime_pop_checkpoints(" + std::to_string(nRegions) +
                ");"
            );
        }
    };

    if (e.hasTryCleanup() && e.hasReturnVars()) {
        e.emit(e.getReturnVar() + " = " + expr + ";");
        e.emit(e.getReturningVar() + " = 1;");
        emitRegionCleanup();
        e.emit("goto " + e.currentTryCleanup() + ";");
    } else if (e.hasDeferContext() && e.hasReturnVars()) {
        e.emit(e.getReturnVar() + " = " + expr + ";");
        emitRegionCleanup();
        e.emit("goto " + e.getDeferCleanupLabel() + ";");
    } else {
        if (nRegions > 0 && primitiveSafe) {
            // Capture the boxed RHS in a local before unwinding, so the
            // rewind cannot free anything the RHS still references.
            std::string slot = e.newTemp("ret_val");
            e.emit("VyneValue " + slot + " = " + expr + ";");
            e.emitRegionUnwind();
            e.emit("return " + slot + ";");
        } else {
            emitRegionCleanup();
            e.emit("return " + expr + ";");
        }
    }
}

std::string ReturnNode::getCExpr(C_Emitter& e) const {
    compile(e);
    return "vyne_null()";
}

void BreakNode::compile(C_Emitter& e) const {
    if (e.hasTryCleanup()) {
        throw std::runtime_error(
            "Compile Error: 'break' inside try/catch/finally is not supported "
            "by "
            "the C backend (line " +
            std::to_string(lineNumber) +
            "). "
            "Use a flag variable and break outside the try."
        );
    }
    e.emitRegionUnwind();
    e.emit("break;");
}
std::string BreakNode::getCExpr(C_Emitter& e) const {
    return "vyne_null()";
}

void ContinueNode::compile(C_Emitter& e) const {
    if (e.hasTryCleanup()) {
        throw std::runtime_error(
            "Compile Error: 'continue' inside try/catch/finally is not "
            "supported by "
            "the C backend (line " +
            std::to_string(lineNumber) + ")."
        );
    }
    e.emitRegionUnwind();
    e.emit("continue;");
}
std::string ContinueNode::getCExpr(C_Emitter& e) const {
    return "vyne_null()";
}

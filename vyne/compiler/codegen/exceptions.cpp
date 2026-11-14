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

// Throw, try/catch, and finally lowering.
// Keep expressions that emit statements in evaluation order; see README.md.
// ============================================================
// TRY/CATCH/THROW/FINALLY (stubs for Compile)
// ============================================================

void ThrowNode::compile(C_Emitter& e) const {
    std::string expr = expression ? e.boxAny(expression->getCExpr(e))
                                  : "vyne_null()";
    e.emit("vyne_throw(" + expr + ");");
}

std::string ThrowNode::getCExpr(C_Emitter& e) const {
    return "vyne_null()";
}

void FinallyNode::compile(C_Emitter& e) const {
    if (body) body->compile(e);
}

std::string FinallyNode::getCExpr(C_Emitter& e) const {
    return "vyne_null()";
}

void TryCatchNode::compile(C_Emitter& e) const {
    std::string mark        = e.newTemp("try_mark");
    std::string cmark       = e.newTemp("catch_mark");
    std::string frameLive   = e.newTemp("frame_live");
    std::string cframeLive  = e.newTemp("cframe_live");
    std::string caught      = e.newTemp("caught");
    std::string errVar      = e.newTemp("try_err");
    std::string cleanup     = e.newTemp("try_cleanup");

    e.emit("VyneValue " + errVar + " = vyne_null();");
    e.emit("int " + caught + " = 0;");
    e.emit("int " + frameLive + " = 1;");
    e.emit("int " + cframeLive + " = 0;");
    e.emit("int " + mark + " = vyne_try_push();");

    // ---- try body ----
    e.emitBlockOpen("if (vyne_setjmp(g_exc_stack[" + mark + "].buf) == 0) {");
    e.pushTryCleanup(cleanup);
    if (tryBody) tryBody->compile(e);
    e.popTryCleanup();
    e.emit("goto " + cleanup + ";");
    e.emitBlockClose();

    // ---- landed from a throw in the try body ----
    // vyne_throw already popped the frame before the longjmp.
    e.emit(frameLive + " = 0;");
    e.emit(caught + " = 1;");
    e.emit(errVar + " = g_exc_value;");

    // ---- catch body in its own frame ----
    if (catchBody) {
        e.emit("int " + cmark + " = vyne_try_push();");
        e.emit(cframeLive + " = 1;");
        e.emitBlockOpen("if (vyne_setjmp(g_exc_stack[" + cmark + "].buf) == 0) {");

        // Bind the catch variable in the current function scope
        std::string prefix = e.getActiveFunctionPrefix();
        std::string catchSanitized = catchVarName;
        std::replace(catchSanitized.begin(), catchSanitized.end(), '.', '_');
        std::string cVar = prefix.empty()
            ? ("v_" + catchSanitized)
            : ("v_" + prefix + "_" + catchSanitized);
        e.registerDeclaration(cVar);
        e.emit("VyneValue " + cVar + " = " + errVar + ";");

        e.pushTryCleanup(cleanup);
        catchBody->compile(e);
        e.popTryCleanup();

        e.emit("goto " + cleanup + ";");
        e.emitBlockClose();

        // ---- landed from a throw in the catch body ----
        // vyne_throw already popped the catch frame.
        e.emit(cframeLive + " = 0;");
        e.emit(errVar + " = g_exc_value;");
    }

    // ---- cleanup label: run finally, decide what to do next ----
    e.dedent();
    e.emit(cleanup + ":");
    e.indent();

    // Pop whichever frame is still live (skip if throw already popped it).
    e.emit("if (" + frameLive + ") vyne_try_pop();");
    e.emit("if (" + cframeLive + ") { vyne_try_pop(); " + caught + " = 0; }");

    if (finallyBody) finallyBody->compile(e);

    // Pending return takes priority — if the user wrote `return` inside the
    // try or catch, the finally already ran above, so bubble it up now.
    if (e.hasReturnVars()) {
        e.emit("if (" + e.getReturningVar() + ") {");
        if (e.hasTryCleanup()) {
            e.emit("    goto " + e.currentTryCleanup() + ";");
        } else if (e.hasDeferContext()) {
            e.emit("    goto " + e.getDeferCleanupLabel() + ";");
        } else {
            e.emit("    return " + e.getReturnVar() + ";");
        }
        e.emit("}");
    }

    // If the try (or catch) threw and nothing cleared the flag, re-throw.
    e.emit("if (" + caught + ") vyne_throw(" + errVar + ");");
}

std::string TryCatchNode::getCExpr(C_Emitter& e) const {
    compile(e);
    return "vyne_null()";
}

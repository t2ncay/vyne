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

// Language built-in call lowering.
// Keep expressions that emit statements in evaluation order; see README.md.
// ============================================================
// BUILT-INS
// ============================================================

std::string BuiltInCallNode::getCExpr(C_Emitter& e) const {
    if (funcName == "out") {
        for (const auto& arg : arguments) {
            e.emit("vyne_out(" + e.boxAny(arg->getCExpr(e)) + ");");
        }
        return "vyne_null()";
    }
    if (funcName == "string") {
        if (arguments.empty()) return "vyne_string(\"\")";
        std::string temp = e.newTemp("str");
        e.emit("VyneValue " + temp + " = vyne_to_string(" +
               e.boxAny(arguments[0]->getCExpr(e)) + ");");
        return temp;
    }
    if (funcName == "int64") {
        std::string arg = arguments.empty() ? "vyne_null()"
                                            : e.boxAny(arguments[0]->getCExpr(e));
        return "vyne_to_int(" + arg + ")";
    }
    if (funcName == "float64") {
        if (arguments.empty()) return "vyne_float(0.0)";
        return "vyne_to_float(" + e.boxAny(arguments[0]->getCExpr(e)) + ")";
    }
    if (funcName == "float32") {
        if (arguments.empty()) return "vyne_float32(0.0f)";
        return "vyne_to_float32(" + e.boxAny(arguments[0]->getCExpr(e)) + ")";
    }
    if (funcName == "sizeof") {
        if (arguments.empty()) return "vyne_int(0)";
        std::string arg = e.boxAny(arguments[0]->getCExpr(e));
        std::string temp = e.newTemp("sz");
        e.emit("VyneValue " + temp + " = vyne_int(vyne_get_sizeof(" + arg + "));");
        return temp;
    }
    if (funcName == "type") {
        if (arguments.empty()) return "vyne_string(\"null\")";
        std::string temp = e.newTemp("type");
        e.emit("VyneValue " + temp + " = vyne_string(vyne_get_type_name(" +
               e.boxAny(arguments[0]->getCExpr(e)) + "));");
        return temp;
    }
    if (funcName == "free") {
        if (arguments.empty()) return "vyne_null()";
        // Free is a no-op in the C runtime since we use arena allocation
        e.emit("// free() called on: " + e.boxAny(arguments[0]->getCExpr(e)));
        return "vyne_null()";
    }
    if (funcName == "exit") {
        if (!arguments.empty()) {
            e.emit("exit((int)" + e.boxAny(arguments[0]->getCExpr(e)) + ".as.i64);");
        } else {
            e.emit("exit(0);");
        }
        return "vyne_null()";
    }
    if (funcName == "sequence") {
        if (arguments.size() < 2) return "vyne_array_create(0)";
        std::string start = e.boxAny(arguments[0]->getCExpr(e));
        std::string end   = e.boxAny(arguments[1]->getCExpr(e));
        std::string temp  = e.newTemp("seq");
        std::string iv    = e.newTemp("i");
        std::string nTmp  = e.newTemp("seq_n");

        e.emit("int64_t " + nTmp + " = (" + end + ").as.i64 - (" + start + ").as.i64;");
        e.emit("if (" + nTmp + " < 0) " + nTmp + " = 0;");
        e.emit("VyneValue " + temp + " = vyne_array_create((int)" + nTmp + ");");
        e.emitBlockOpen("for (int64_t " + iv + " = 0; " + iv + " < " + nTmp + "; " + iv + "++) {");
        e.emit("vyne_array_set(" + temp + ", vyne_int(" + iv + "), "
               "vyne_int((" + start + ").as.i64 + " + iv + "));");
        e.emitBlockClose();
        return temp;
    }
    if (funcName == "map") {
        return "vyne_map_create()";
    }

    e.emit("/* unknown built-in: " + funcName + " */");
    return "vyne_null()";
}

void BuiltInCallNode::compile(C_Emitter& e) const { getCExpr(e); }


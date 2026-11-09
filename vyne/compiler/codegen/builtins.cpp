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

static void ensureVmemIncludeForDrop(C_Emitter& e) {
    std::string base = FileUtils::getExeDir();
    std::filesystem::path moduleBase =
        std::filesystem::path(base) / "vyne" / "runtime" / "modules";
    e.addInclude((moduleBase / "vmem.h").string());
}

// Resolve the C name of an lvalue argument using the same mangling rule as
// VariableNode::getCExpr and RegionCommitNode::compile. Kept local so the
// three sites can evolve independently (they already differ in how they
// report errors).
static std::string resolveLvalueName(
    C_Emitter& e,
    const std::string& originalName
) {
    std::string sanitized = originalName;
    std::replace(sanitized.begin(), sanitized.end(), '.', '_');

    std::string prefix = e.getActiveFunctionPrefix();
    if (!prefix.empty()) {
        std::string localName = "v_" + prefix + "_" + sanitized;
        return e.isLocalDeclared(localName) ? localName : ("v_" + sanitized);
    }
    return "v_" + sanitized;
}

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
    if (funcName == "drop") {
        if (arguments.size() != 1) {
            throw std::runtime_error(
                "Compile Error (VNE-083): drop(x) takes exactly one variable "
                "argument, got " +
                std::to_string(arguments.size()) + " (line " +
                std::to_string(lineNumber) + ")."
            );
        }
        auto* var = dynamic_cast<VariableNode*>(arguments[0].get());
        if (!var) {
            throw std::runtime_error(
                "Compile Error (VNE-083): drop(x) requires an lvalue "
                "variable, not an expression (line " +
                std::to_string(lineNumber) + ").\n"
                "  Drop is a LIFO tail reclamation; it needs the variable "
                "whose allocation should be released."
            );
        }

        std::string cVar = resolveLvalueName(e, var->getOriginalName());
        const CType* ct = e.lookupType(cVar);
        if (!ct || ct->kind != CType::Kind::Array || ct->args.empty()) {
            throw std::runtime_error(
                "Compile Error (VNE-083): drop(x) currently supports typed "
                "Array<Float64>, Array<Float32>, and Array<Int64> variables "
                "(line " +
                std::to_string(lineNumber) + ").\n"
                "  The container must be a native typed array so its LIFO "
                "payload can be released without a per-element scan."
            );
        }
        if (ct->hasShape()) {
            throw std::runtime_error(
                "Compile Error (VNE-084): drop() on a stack-allocated "
                "scratch array is impossible (line " +
                std::to_string(lineNumber) +
                ").\n"
                "  Scratch arrays are C stack storage, not arena "
                "allocations."
            );
        }

        VType elem = ct->args[0].toVType();
        const char* fn = nullptr;
        if (elem == VType::Float64)
            fn = "vmem_runtime_drop_array_f64";
        else if (elem == VType::Float32)
            fn = "vmem_runtime_drop_array_f32";
        else if (elem == VType::Int64)
            fn = "vmem_runtime_drop_array_i64";
        else {
            throw std::runtime_error(
                "Compile Error (VNE-083): drop(x) supports only Float64, "
                "Float32, and Int64 array elements (line " +
                std::to_string(lineNumber) + ")."
            );
        }

        ensureVmemIncludeForDrop(e);
        std::string temp = e.newTemp("drop_ok");
        e.emit(
            "VyneValue " + temp + " = " + fn + "(&" + cVar + ");"
        );
        return temp;
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


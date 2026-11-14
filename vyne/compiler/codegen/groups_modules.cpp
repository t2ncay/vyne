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

// Group and module statements and deployment includes.
// Keep expressions that emit statements in evaluation order; see README.md.
// ============================================================
// GROUP
// ============================================================

void GroupNode::compile(C_Emitter& e) const {
    e.registerGroup(groupName);
    e.pushGlobalContext();
    e.emit("// --- Group: " + groupName + " ---");

    for (const auto& stmt : statements) {
        if (!stmt)
            continue;
        if (stmt->type() == NodeType::ASSIGNMENT) {
            auto* assign = static_cast<AssignmentNode*>(stmt.get());
            std::string mangled =
                "v_" + groupName + "_" + assign->getOriginalName();
            std::replace(mangled.begin(), mangled.end(), '.', '_');

            CType declared = CType::fromVType(assign->getExpectedType());

            if (declared.isPrimitive()) {
                e.declareGlobal(mangled, declared);
                e.emitGlobalDecl(
                    declared.cTypeName() + " " + mangled + " = 0;"
                );
            } else {
                e.registerDeclaration(mangled);
                e.emitGlobalDecl("VyneValue " + mangled + ";");
            }

            e.popGlobalContext();
            std::string val = assign->getRHS()->getCExpr(e);
            if (declared.isPrimitive()) {
                std::string init = coerceToNative(
                    e,
                    assign->getRHS(),
                    val,
                    declared.toVType()
                );
                e.emit(mangled + " = " + init + ";");
            } else {
                e.emit(mangled + " = " + e.boxAny(val) + ";");
            }
            e.pushGlobalContext();
        } else if (stmt->type() == NodeType::FUNCTION) {
            auto* fn = static_cast<FunctionNode*>(stmt.get());

            // Register this group member's return type under every
            // spelling the escape check might query with. Group functions
            // were previously never registered — only top-level ones in
            // ProgramNode::compile were — so every `x = group.fn(...)`
            // inside a region looked like an untyped, non-primitive RHS
            // and tripped VNE-070. `lossN = vlin.cross_entropy(...)` is
            // the case that exposed it.
            CType retCt = CType::fromVType(fn->getReturnType());
            if (fn->getReturnType() == VType::Array &&
                fn->getReturnArrayElemType() != VType::Unknown) {
                retCt.args.push_back(
                    CType::fromVType(fn->getReturnArrayElemType())
                );
            }

            std::string orig = fn->getOriginalName();
            std::string mangled = orig;
            std::replace(mangled.begin(), mangled.end(), '.', '_');

            std::string groupMangled = groupName + "_" + mangled;
            std::replace(groupMangled.begin(), groupMangled.end(), '.', '_');

            std::string groupDotted = groupName + "." + orig;

            e.registerFunctionReturnType(orig, retCt);
            e.registerFunctionReturnType(mangled, retCt);
            e.registerFunctionReturnType(groupMangled, retCt);
            e.registerFunctionReturnType(groupDotted, retCt);

            e.popGlobalContext();
            stmt->compile(e);
            e.pushGlobalContext();
        } else if (stmt->type() == NodeType::INTERFACE) {
            e.popGlobalContext();
            e.setGroupPrefix(groupName);
            stmt->compile(e);
            e.clearGroupPrefix();
            e.pushGlobalContext();
        }
    }
    e.popGlobalContext();
}

std::string GroupNode::getCExpr(C_Emitter& e) const {
    compile(e);
    return "vyne_null()";
}

// ============================================================
// MODULE
// ============================================================

// `module foo;` — no more name-based behaviour. The parser has
// already set `currentModuleName`, which is all the compiler needs
// from this statement during parsing. At codegen time we register
// the name as a user group so `foo.bar(...)` routes through the
// group-call path; nothing else is done here. If the user wants a
// native module, they must say so explicitly with `use native`.
void ModuleNode::compile(C_Emitter& e) const {
    e.registerGroup(originalName);
}

std::string ModuleNode::getCExpr(C_Emitter& e) const {
    return "vyne_null()";
}

// ============================================================
// NATIVE MODULE — CODEGEN
// ------------------------------------------------------------
// `use native vmath;` lowers to:
//   • pulling the runtime header <vmath.h> into the generated C
//   • registering the bound name with the emitter as a group, so
//     `vmath.sqrt(...)` reaches the group-call branch of
//     MethodCallNode::getCExpr
//
// The group branch checks `findNative(recvPath, methodName)`
// before falling through to the boxed call, so registering the
// name as a group enables dispatch. Native aliases also retain their canonical
// module name for symbol lookup and automatic runtime-header inclusion.
// Aliases are registered as additional groups pointing at the
// same underlying header. Emitting the same `#include` twice is
// harmless (includeSet deduplicates by path).
// ============================================================
void NativeModuleNode::compile(C_Emitter& e) const {
    e.registerNativeModuleAlias(alias, moduleName);
    // The `arduino` pseudo-module has no standalone `arduino.h` under
    // vyne/runtime/modules. Its runtime lives in vyne/runtime/arduino/
    // and is included by C_Emitter::finalize() whenever the Arduino
    // target is selected (`#include "vyne_arduino.h"`), which in turn
    // pulls in <Arduino.h> through mcu_types.h and mcu_hardware.h.
    // The only thing this node has to do is register the group so
    // `arduino.pin_mode(...)` reaches the native-method dispatch.
    if (moduleName == "arduino") {
        e.registerGroup("arduino");
        if (!alias.empty()) {
            e.registerGroup(alias);
        }
        return;
    }

    std::string base = FileUtils::getExeDir();
    std::filesystem::path moduleBase =
        std::filesystem::path(base) / "vyne" / "runtime" / "modules";

    e.addInclude((moduleBase / (moduleName + ".h")).string());

    e.registerGroup(moduleName);
    if (!alias.empty()) {
        e.registerGroup(alias);
    }
}

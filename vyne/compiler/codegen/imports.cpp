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
#include "../../utils/file_utils.h" 

// Imported module compilation and namespacing.
// Keep expressions that emit statements in evaluation order; see README.md.
// ============================================================
// IMPORT
// ============================================================

void ImportNode::compile(C_Emitter& e) const {
    std::filesystem::path finalPath;
    std::string cleanPath = filePath;
    if (!cleanPath.empty() && (cleanPath[0] == '/' || cleanPath[0] == '\\'))
        cleanPath.erase(0, 1);

    if (isExtern) {
        finalPath = FileUtils::getExternalPath(cleanPath);
    } else {
        finalPath = std::filesystem::path(e.getSourceDir()) / cleanPath;
    }

    try {
        finalPath = std::filesystem::weakly_canonical(finalPath);
    } catch (const std::filesystem::filesystem_error& ex) {
        Vyne::DiagnosticEngine::setCurrentFile(filePath);
        throw std::runtime_error(
            "VNE-005: could not resolve import (line " +
            std::to_string(lineNumber) + "): '" + cleanPath +
            "' — " + ex.what());
    }

    if (!std::filesystem::exists(finalPath) || std::filesystem::is_directory(finalPath)) {
        Vyne::DiagnosticEngine::setCurrentFile(filePath);
        throw std::runtime_error(
            "VNE-005: unresolved import (line " + std::to_string(lineNumber) +
            "): '" + cleanPath + "' not found at " + finalPath.string() +
            "\n  searched from source dir: " + e.getSourceDir());
    }

    if (e.isAlreadyImported(finalPath.string()))
        return;
    e.markImported(finalPath.string());

    const std::string& source = FileUtils::readFile(finalPath.string());
    auto tokens = tokenize(source);

    SymbolContainer parseEnv;
    Parser parser(std::move(tokens));
    std::unique_ptr<ProgramNode> externalAst = parser.parseProgram(parseEnv);
    if (!externalAst)
        throw std::runtime_error("Import Error: failed to parse '" + cleanPath + "'");

    std::string prevDir = e.getSourceDir();
    e.setSourceDir(finalPath.parent_path().string());

    for (const auto& stmt : externalAst->statements) {
        if (!stmt) continue;
        if (stmt->type() == NodeType::INTERFACE) {
            auto* iface = static_cast<InterfaceNode*>(stmt.get());
            e.registerInterface(iface->getInterfaceName());
            if (!iface->getModuleName().empty()) {
                e.registerInterface(iface->getModuleName() + "." + iface->getInterfaceName());
            }
        }
        if (stmt->type() == NodeType::GROUP) {
            auto* grp = static_cast<GroupNode*>(stmt.get());
            e.registerGroup(grp->getGroupName());
        }
        if (stmt->type() == NodeType::MODULE) {
            auto* mod = static_cast<ModuleNode*>(stmt.get());
            e.registerGroup(mod->getOriginalName());
        }
    }

    if (alias.empty()) {
        externalAst->compile(e);
    } else {
        e.registerGroup(alias);
        e.pushGlobalContext();
        e.emit("// --- import as " + alias + " ---");

        for (const auto& stmt : externalAst->statements) {
            if (!stmt) continue;
            if (stmt->type() == NodeType::FUNCTION) {
                auto* fn = static_cast<FunctionNode*>(stmt.get());

                // Register the aliased function's return type so the
                // region escape check (assignments.cpp:checkRegionEscape)
                // can prove calls to it are primitive-returning.
                //
                // This branch of ImportNode::compile handles the
                // `use lib "..."` form, which the parser turns into an
                // ImportNode with a non-empty alias ("vlin" for
                // "vlin/vlin.vy"). It routes through
                // FunctionNode::compileAs, which — unlike
                // FunctionNode::compile — does not touch the return-
                // type table. So `x = alias.fn(...)` inside a region
                // saw an unregistered callee, could not prove a
                // primitive return, and tripped VNE-070 even for pure
                // scalar functions like `vlin.cross_entropy`.
                //
                // Register under every spelling the escape check
                // queries with: bare, alias-underscore, alias-dotted.
                {
                    CType retCt = CType::fromVType(fn->getReturnType());
                    if (fn->getReturnType() == VType::Array &&
                        fn->getReturnArrayElemType() != VType::Unknown) {
                        retCt.args.push_back(
                            CType::fromVType(fn->getReturnArrayElemType()));
                    }

                    std::string fnName = fn->getOriginalName();
                    std::string mangled = alias + "_" + fnName;
                    std::replace(mangled.begin(), mangled.end(), '.', '_');

                    e.registerFunctionReturnType(fnName,               retCt);
                    e.registerFunctionReturnType(mangled,              retCt);
                    e.registerFunctionReturnType(alias + "." + fnName, retCt);
                }

                e.popGlobalContext();
                fn->compileAs(e, alias + "_" + fn->getOriginalName());
                e.pushGlobalContext();
            } else if (stmt->type() == NodeType::ASSIGNMENT) {
                auto* assign = static_cast<AssignmentNode*>(stmt.get());
                std::string mangled = "v_" + alias + "_" + assign->getOriginalName();
                std::replace(mangled.begin(), mangled.end(), '.', '_');

                CType declared = CType::fromVType(assign->getExpectedType());

                if (declared.isPrimitive()) {
                    e.declareGlobal(mangled, declared);
                    e.emitGlobalDecl(declared.cTypeName() + " " + mangled + " = 0;");
                } else {
                    e.registerDeclaration(mangled);
                    e.emitGlobalDecl("VyneValue " + mangled + ";");
                }

                e.popGlobalContext();
                std::string val = assign->getRHS()->getCExpr(e);
                if (declared.isPrimitive()) {
                    std::string init = coerceToNative(
                        e, assign->getRHS(), val, declared.toVType());
                    e.emit(mangled + " = " + init + ";");
                } else {
                    e.emit(mangled + " = " + e.boxAny(val) + ";");
                }
                e.pushGlobalContext();
            } else {
                e.popGlobalContext();
                stmt->compile(e);
                e.pushGlobalContext();
            }
        }
        e.popGlobalContext();
    }

    std::string targetNamespace = alias.empty() ? finalPath.stem().string() : alias;
    std::string modVirtualVar = "v_" + targetNamespace;
    std::replace(modVirtualVar.begin(), modVirtualVar.end(), '.', '_');

    if (e.getGlobalVars().count(modVirtualVar) == 0) {
        e.registerDeclaration(modVirtualVar);
        e.emitGlobalDecl("VyneValue " + modVirtualVar + ";");
    }

    e.pushMainContext();
    e.emit("// Virtual Module registration for " + targetNamespace);
    e.emitBlockOpen("{");
    std::string tempS = e.newTemp("mod_s");
    e.emit("VyneStruct* " + tempS + " = (VyneStruct*)arena_alloc(sizeof(VyneStruct));");
    e.emit(tempS + "->type_name = \"" + targetNamespace + "\";");
    e.emit(tempS + "->field_count = 0;");
    e.emit(tempS + "->fields = NULL;");
    e.emit(tempS + "->methods = NULL;");
    e.emit(tempS + "->method_count = 0;");
    e.emit(tempS + "->field_cache[0] = -1;");
    e.emit(tempS + "->field_cache[1] = -1;");
    e.emit(tempS + "->field_cache[2] = -1;");
    e.emit(tempS + "->field_cache[3] = -1;");
    e.emit(modVirtualVar + ".type = V_STRUCT; " + modVirtualVar + ".as.strct = " + tempS + ";");

    for (const auto& stmt : externalAst->statements) {
        if (stmt && stmt->type() == NodeType::FUNCTION) {
            auto* fn = static_cast<FunctionNode*>(stmt.get());
            std::string fnOrigName = fn->getOriginalName();
            std::string fnRealCName = "fn_" + (alias.empty() ? fnOrigName : (alias + "_" + fnOrigName));
            std::replace(fnRealCName.begin(), fnRealCName.end(), '.', '_');

            std::string wrapperName = "wrap_" + targetNamespace + "_" + fnOrigName;
            std::replace(wrapperName.begin(), wrapperName.end(), '.', '_');

            e.pushFunctionContext();
            e.emitGlobalDecl("VyneValue " + wrapperName + "(int arg_count, VyneValue* args);");
            e.emitBlockOpen("VyneValue " + wrapperName + "(int arg_count, VyneValue* args) {");
            e.emit("if (arg_count > 0) {");
            e.emit("    // args[0] 'self'");
            e.emit("    return " + fnRealCName + "(arg_count - 1, args + 1);");
            e.emit("}");
            e.emit("return " + fnRealCName + "(arg_count, args);");
            e.emitBlockClose();
            e.popFunctionContext();

            e.emit("vyne_register_method(\"" + targetNamespace + "\", \"" +
                   fnOrigName + "\", " + wrapperName + ");");
        }
    }
    e.emitBlockClose();
    e.popMainContext();

    e.setSourceDir(prevDir);
}

std::string ImportNode::getCExpr(C_Emitter& e) const {
    compile(e);
    return "vyne_null()";
}

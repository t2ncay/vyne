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
#include <cctype>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "../../runtime/diagnostics.h"
#include "../ast/ast.h"
#include "../ast/ast_helpers.h"
#include "../lexer/lexer.h"
#include "../types.h"

struct SymbolInfo {
    VType type;
    bool isExplicit;
    bool used;
    int line;
    std::string name;
};

class Parser {
  private:
    std::vector<Token> tokens;
    size_t pos = 0;
    std::vector<std::unordered_map<uint32_t, SymbolInfo>> scopeStack;
    inline static std::unordered_set<std::string> declaredTypes;
    std::string currentGroupName;
    std::string currentModuleName;
    std::vector<std::string> groupPath;
    std::string sourceDir;

    // Cache: canonical path → the declared-types set that parsing
    // that file produced (including everything it transitively pulled in).
    // Siblings consult this instead of re-parsing shared deps, and unlike
    // a visited-set they still receive the harvested names.
    std::shared_ptr<
        std::unordered_map<std::string, std::unordered_set<std::string>>>
        importedTypeCache = std::make_shared<
            std::unordered_map<std::string, std::unordered_set<std::string>>>();

    // Cycle guard: canonical paths we are currently in the middle of
    // parsing. Prevents `a → b → a` from recursing forever.
    std::shared_ptr<std::unordered_set<std::string>> parsingInProgress =
        std::make_shared<std::unordered_set<std::string>>();

    void pushScope() {
        scopeStack.push_back({});
    }
    void popScope() {
        scopeStack.pop_back();
    }
    void defineSymbol(
        uint32_t id,
        VType type,
        bool explicitType,
        int line,
        const std::string& name
    ) {
        if (scopeStack.empty()) {
            pushScope();
        }

        if (!Vyne::isQuietMode()) {
            if (scopeStack.back().count(id)) {
                Vyne::warn(
                    "Variable '" + name + "' is already defined in this scope",
                    line
                );
            }

            for (size_t i = 0; i < scopeStack.size() - 1; ++i) {
                if (scopeStack[i].count(id)) {
                    Vyne::warn(
                        "Variable '" + name + "' shadows outer variable",
                        line
                    );
                    break;
                }
            }
        }

        scopeStack.back()[id] = {type, explicitType, false, line, name};
    }

    void defineScopedSymbol(
        const std::vector<std::string>& scope,
        uint32_t id,
        VType type,
        bool explicitType
    ) {
        std::string fullName = "";
        for (const auto& s : scope)
            fullName += s + ".";

        std::string originalName = StringPool::instance().get(id);
        uint32_t scopedId =
            StringPool::instance().intern(fullName + originalName);

        if (!scopeStack.empty()) {
            scopeStack.front()[scopedId] = {type, explicitType};
        }
    }

    SymbolInfo* lookupSymbol(uint32_t id) {
        for (auto it = scopeStack.rbegin(); it != scopeStack.rend(); ++it) {
            if (it->count(id))
                return &((*it)[id]);
        }
        return nullptr;
    }

    bool isDeclaredAsStruct(const std::string& name) const {
        return declaredTypes.find(name) != declaredTypes.end();
    }

    bool tryParseTypeArgs(std::vector<std::string>& out);
    VType resolveArrayElementType(const std::string& typePath);

    std::vector<std::string> parsePolicyArgs();

    // Recursively parse `finalPath` and merge every interface name it
    // declares (and transitively pulls in) into this parser's
    // `declaredTypes`. Results are cached per canonical path so sibling
    // imports still see each other's types.
    void collectTypesFromImport(const std::filesystem::path& finalPath);

    // --- Literal Workers ---
    std::unique_ptr<ASTNode> parseStringLiteral();
    std::unique_ptr<ASTNode> parseNumberLiteral();
    std::unique_ptr<ASTNode> parseBooleanLiteral();
    std::unique_ptr<ASTNode> parseArrayLiteral();
    std::unique_ptr<ASTNode> parseMapLiteral();
    std::unique_ptr<ASTNode> parseGroupingExpr();
    std::unique_ptr<ASTNode> parseIdentifierExpr();
    std::unique_ptr<ASTNode> parseNullLiteral();

    // --- Statement Workers ---
    std::unique_ptr<ASTNode> parseBlock();
    std::unique_ptr<ASTNode> parseReturnStatement();
    std::unique_ptr<ASTNode> parseIfStatement();
    std::unique_ptr<ASTNode> parseWhileLoop();
    std::unique_ptr<ASTNode> parseForLoop();
    std::unique_ptr<ASTNode> parseAssignment();
    std::unique_ptr<ASTNode> parseVariableAssignment(
        std::unique_ptr<ASTNode> lhs,
        int line,
        bool isConst
    );
    std::unique_ptr<ASTNode> parseGroupDefinition();
    std::unique_ptr<ASTNode> parseModuleStatement();
    std::unique_ptr<ASTNode> parseDismissStatement();
    std::unique_ptr<ASTNode> parseLoopControl();
    std::unique_ptr<ASTNode> parseStatement();
    std::unique_ptr<ASTNode> parseRuleset();
    std::unique_ptr<ASTNode> parseInterpolatedString();
    std::unique_ptr<ASTNode> parseDeferStatement();
    std::unique_ptr<ASTNode> parseInExpression();
    std::unique_ptr<ASTNode> parseNullCoalesce();
    std::unique_ptr<ASTNode> parsePipeline();
    std::unique_ptr<ASTNode> parseEnum();
    std::unique_ptr<ASTNode> parseTryCatch();
    std::unique_ptr<ASTNode> parseThrowStatement();
    std::unique_ptr<ASTNode> parseRulesetBlock(int line);
    std::unique_ptr<ASTNode> parseRegionStatement();
    std::unique_ptr<ASTNode> parseScratchDeclaration();

    void applyRulesetValue(const Token& ruleName, int64_t value, int line);
    void applyRulesetValue(
        const Token& ruleName,
        const std::string& value,
        int line
    );
    void applyRulesetArrayValue(
        const Token& ruleName,
        std::unique_ptr<ASTNode> values,
        int line
    );
    void applyRulesetFlag(const Token& ruleName, int line);

  public:
    // --- Navigation ---
    const Token& peekToken();
    const Token& getNextToken();
    const Token& lookAhead(int distance);
    const Token& consume(VTokenType expected);
    void consumeSemicolon();
    bool isAtEnd();
    VType resolveType(std::string_view typeName);
    std::string parseTypePath();

    Parser(std::vector<Token>&& t)
        : tokens(std::move(t)) {};
    void setSourceDir(const std::string& dir) {
        sourceDir = dir;
    }

    std::unique_ptr<ASTNode> parseFunctionDefinition();
    std::unique_ptr<ASTNode> parseBuiltInCall();
    std::unique_ptr<ASTNode> parseFactor();
    std::unique_ptr<ASTNode> parseTerm();
    std::unique_ptr<ASTNode> parsePostfix();
    std::unique_ptr<ASTNode> parseUnary();
    std::unique_ptr<ASTNode> parseAdditive();
    std::unique_ptr<ASTNode> parseShift();
    std::unique_ptr<ASTNode> parseRelational();
    std::unique_ptr<ASTNode> parseEquality();
    std::unique_ptr<ASTNode> parseBitwiseAnd();
    std::unique_ptr<ASTNode> parseBitwiseXor();
    std::unique_ptr<ASTNode> parseBitwiseOr();
    std::unique_ptr<ASTNode> parseLogicalAnd();
    std::unique_ptr<ASTNode> parseLogicalOr();
    std::unique_ptr<ASTNode> parseRange();
    std::unique_ptr<ASTNode> parseTernary();
    std::unique_ptr<ASTNode> parseExpression();
    std::unique_ptr<ASTNode> parseImportModule();
    std::unique_ptr<ASTNode> parseDeployModule();
    std::unique_ptr<ASTNode> parseInterfaceDefinition();
    std::unique_ptr<ProgramNode> parseProgram(SymbolContainer& env);

    void checkUnusedVariables(const SymbolContainer& env) {
        if (Vyne::isQuietMode())
            return;

        for (const auto& scope : scopeStack) {
            for (const auto& [id, info] : scope) {
                if (!info.used && !env.wasUsed(id)) {
                    Vyne::warn(
                        "Unused variable '" + info.name + "'",
                        info.line
                    );
                }
            }
        }
    }
};

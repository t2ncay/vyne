#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <cctype>
#include <memory>
#include <map>
#include <unordered_set>

#include "../lexer/lexer.h"
#include "../ast/ast.h"
#include "../types.h"
#include "../ast/ast_helpers.h"

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
	std::unordered_set<std::string> declaredTypes;
	std::string currentGroupName;
	std::string currentModuleName;
	std::vector<std::string> groupPath;

	void pushScope() { scopeStack.push_back({}); }
	void popScope()  { scopeStack.pop_back(); }
	void defineSymbol(uint32_t id, VType type, bool explicitType, int line, const std::string& name) {
		if(scopeStack.empty()) {
			pushScope();
		}
		
		if (!Vyne::isQuietMode()) {
			if (scopeStack.back().count(id)) {
				Vyne::warn("Variable '" + name + "' is already defined in this scope", line);
			}
			
			for (size_t i = 0; i < scopeStack.size() - 1; ++i) {
				if (scopeStack[i].count(id)) {
					Vyne::warn("Variable '" + name + "' shadows outer variable", line);
					break;
				}
			}
		}
		
		scopeStack.back()[id] = {type, explicitType, false, line, name};
	}

	void defineScopedSymbol(const std::vector<std::string>& scope, uint32_t id, VType type, bool explicitType) {
		std::string fullName = "";
		for (const auto& s : scope) fullName += s + ".";
		
		std::string originalName = StringPool::instance().get(id);
		uint32_t scopedId = StringPool::instance().intern(fullName + originalName);

		if(!scopeStack.empty()) {
			scopeStack.front()[scopedId] = {type, explicitType};
		}
	}

	SymbolInfo* lookupSymbol(uint32_t id) {
		for (auto it = scopeStack.rbegin(); it != scopeStack.rend(); ++it) {
			if (it->count(id)) return &((*it)[id]);
		}
		return nullptr;
	}

	bool isDeclaredAsStruct(const std::string& name) const {
        return declaredTypes.find(name) != declaredTypes.end();
    }

	void checkUnusedVariables(const SymbolContainer& env) {
		if (Vyne::isQuietMode()) return;
		
		for (const auto& scope : scopeStack) {
			for (const auto& [id, info] : scope) {
				if (!env.wasUsed(id)) {
					Vyne::warn("Unused variable '" + info.name + "'", info.line);
				}
			}
		}
	}

	// --- Literal Workers ---
	std::unique_ptr<ASTNode> parseStringLiteral();
    std::unique_ptr<ASTNode> parseNumberLiteral();
    std::unique_ptr<ASTNode> parseBooleanLiteral();
    std::unique_ptr<ASTNode> parseArrayLiteral();
    std::unique_ptr<ASTNode> parseGroupingExpr();
    std::unique_ptr<ASTNode> parseIdentifierExpr();

	// --- Statement Workers ---
	std::unique_ptr<ASTNode> parseBlock();
	std::unique_ptr<ASTNode> parseReturnStatement();
	std::unique_ptr<ASTNode> parseIfStatement();
	std::unique_ptr<ASTNode> parseWhileLoop();
	std::unique_ptr<ASTNode> parseForLoop();
	std::unique_ptr<ASTNode> parseAssignment();
	std::unique_ptr<ASTNode> parseGroupDefinition();
	std::unique_ptr<ASTNode> parseModuleStatement();
	std::unique_ptr<ASTNode> parseDismissStatement();
	std::unique_ptr<ASTNode> parseLoopControl();
	std::unique_ptr<ASTNode> parseStatement();
	std::unique_ptr<ASTNode> parseRuleset();

public:
	// --- Navigation ---
    Token peekToken();
	Token getNextToken();
    Token lookAhead(int distance);
    Token consume(VTokenType expected);
    void  consumeSemicolon();
	bool isAtEnd();
	VType resolveType(std::string_view typeName);
	std::string parseTypePath();

	Parser(std::vector<Token>&& t) : tokens(std::move(t)) {};

	std::unique_ptr<ASTNode>     parseFunctionDefinition();
	std::unique_ptr<ASTNode>     parseBuiltInCall();
	std::unique_ptr<ASTNode>     parseFactor();
	std::unique_ptr<ASTNode>     parseTerm();
	std::unique_ptr<ASTNode>     parsePostfix();
	std::unique_ptr<ASTNode>     parseUnary();
	std::unique_ptr<ASTNode>     parseAdditive();
	std::unique_ptr<ASTNode>     parseRelational();
	std::unique_ptr<ASTNode>     parseEquality();
	std::unique_ptr<ASTNode>     parseLogicalAnd();
	std::unique_ptr<ASTNode>     parseLogicalOr();
	std::unique_ptr<ASTNode>     parseRange();
	std::unique_ptr<ASTNode>     parseExpression();
	std::unique_ptr<ASTNode>     parseImportModule();
	std::unique_ptr<ASTNode>     parseDeployModule();
	std::unique_ptr<ASTNode>     parseInterfaceDefinition();
	std::unique_ptr<ProgramNode> parseProgram(SymbolContainer& env);
};
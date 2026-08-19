#include "parser.h"
#include "../ast/value.h"

#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define CYAN    "\033[36m"
#define BOLD    "\033[1m"

class ASTNode;
class ProgramNode;
class StringPool;

Token Parser::getNextToken() {
    if (pos < tokens.size()) {
        return tokens[pos++];
    }
    return Token();
}

Token Parser::peekToken() {
    if (pos < tokens.size()) {
        return tokens[pos];
    }
    return Token();
}

Token Parser::lookAhead(int distance) {
    if (pos + distance < tokens.size()) {
        return tokens[pos + distance];
    }
    return Token();
}

Token Parser::consume(VTokenType expected) {
    Token t = peekToken();
    if (t.type == expected) {
        return tokens[pos++];
    }
    throw std::runtime_error("Error: Unexpected token type! Expected " +
        VTokenTypeToString(expected) + ", but got " +
        VTokenTypeToString(peekToken().type) + " instead [ line " + std::to_string(t.line) + " ]");
}

bool Parser::isAtEnd() {
    return peekToken().type == VTokenType::End;
}

VType Parser::resolveType(std::string_view typeName) {
    std::string name(typeName);

    if (name.find('.') == std::string::npos && 
        name != "Int64" && name != "Float64" && name != "String" && 
        name != "Array" && name != "Bool" && name != "null") {
        
        bool isGroupName = false;
        for (const auto& t : declaredTypes) {
            if (t.find(name + ".") == 0) {
                isGroupName = true;
                break;
            }
        }
        
        if (isGroupName) {
            throw std::runtime_error("Type Error: '" + name + "' is a group/module name, not a type. Did you mean '" + name + ".Something'?");
        }
    }
        
    
    VType primitive = stringToVType(name);
    if (primitive != VType::Unknown) {
        return primitive;
    }

    if (declaredTypes.count(name)) {
        return VType::Struct;
    }

    size_t dotPos = name.find('.');
    if (dotPos != std::string::npos) {
        if (!currentModuleName.empty()) {
            std::string withModule = currentModuleName + "." + name;
            if (declaredTypes.count(withModule)) {
                return VType::Struct;
            }
        }
        
        std::string lastPart = name.substr(name.find_last_of('.') + 1);
        if (declaredTypes.count(lastPart)) {
            return VType::Struct;
        }
    }

    if (declaredTypes.count(name)) {
        return VType::Struct;
    }

    if (!currentModuleName.empty()) {
        std::string contextualName = currentModuleName + "." + name;
        if (declaredTypes.count(contextualName)) {
            return VType::Struct;
        }
    }

    throw std::runtime_error("Type Error: '" + name + "' is not a defined type.");
}

std::string Parser::parseTypePath() {
    std::string path;
    path.reserve(32);
    
    path += consume(VTokenType::Identifier).name;
    while (peekToken().type == VTokenType::Dot) {
        consume(VTokenType::Dot);
        path.reserve(path.size() + 1 + 16);
        path += '.';
        path += consume(VTokenType::Identifier).name;
    }
    return path;
}

std::unique_ptr<ASTNode> Parser::parseDeployModule() {
    int line = peekToken().line;
    consume(VTokenType::Deploy);

    Token modTok = consume(VTokenType::Identifier);
    std::string moduleName = modTok.name;

    consumeSemicolon();

    auto node = std::make_unique<DeployNode>(moduleName);
    node->lineNumber = line;
    return node;
}

std::unique_ptr<ASTNode> Parser::parseImportModule() {
    int line = peekToken().line;
    consume(VTokenType::Use);

    bool isExtern = false;
    if (peekToken().type == VTokenType::Extern) {
        consume(VTokenType::Extern);
        isExtern = true;
    }

    Token pathTok = consume(VTokenType::String);
    std::string filePath = std::get<std::string>(pathTok.literal);

    std::string cleanPath = filePath;
    if (!cleanPath.empty() && (cleanPath[0] == '/' || cleanPath[0] == '\\')) {
        cleanPath.erase(0, 1);
    }

    std::filesystem::path finalPath;
    if (isExtern) {
        finalPath = std::filesystem::path(FileUtils::exeDir) / "vyne" / "modules" / "external" / cleanPath;
    } else {
        finalPath = std::filesystem::current_path() / cleanPath;
    }

    if (std::filesystem::exists(finalPath)) {
        std::string source = FileUtils::readFile(finalPath.string());
        auto externTokens = tokenize(source);

        for (size_t i = 0; i < externTokens.size(); ++i) {
            if (externTokens[i].type == VTokenType::Interface) {
                if (i + 1 < externTokens.size()) {
                    std::string interfaceName = externTokens[i+1].name;
                    if (i + 3 < externTokens.size() && externTokens[i+2].type == VTokenType::Extends) {
                        std::string moduleName = externTokens[i+3].name;
                        declaredTypes.insert(moduleName + "." + interfaceName);
                    }
                    declaredTypes.insert(interfaceName);
                }
            }
        }
    }

    std::string alias = "";
    if (peekToken().type == VTokenType::As) {
        consume(VTokenType::As);
        alias = consume(VTokenType::Identifier).name;
    }

    if (peekToken().type == VTokenType::Semicolon) {
        consume(VTokenType::Semicolon);
    }
    auto node = std::make_unique<ImportNode>(filePath, isExtern, alias);
    node->lineNumber = line;
    return node;
}

std::unique_ptr<ASTNode> Parser::parseInterfaceDefinition() {
    int line = peekToken().line;
    consume(VTokenType::Interface);

    Token interfaceIdentifier = consume(VTokenType::Identifier);
    std::string interfaceName = interfaceIdentifier.name;

    std::string namespacePart = "";
    if (peekToken().type == VTokenType::Extends) {
        consume(VTokenType::Extends);
        namespacePart = consume(VTokenType::Identifier).name;
    } 
    else if (!groupPath.empty()) {
        for (size_t i = 0; i < groupPath.size(); ++i) {
            if (i > 0) namespacePart += ".";
            namespacePart += groupPath[i];
        }
    }
    else if (!currentModuleName.empty()) {
        namespacePart = currentModuleName;
    }

    if (!namespacePart.empty()) {
        std::string fullPath = namespacePart + "." + interfaceName;
        declaredTypes.insert(fullPath);
        
        if (!currentModuleName.empty()) {
            std::string modulePath = currentModuleName + "." + interfaceName;
            declaredTypes.insert(modulePath);
        }
        
        if (!groupPath.empty()) {
            std::string groupRelativePath = groupPath.back() + "." + interfaceName;
            declaredTypes.insert(groupRelativePath);
        }
        
        if (!currentModuleName.empty() && !groupPath.empty()) {
            std::string moduleGroupPath = currentModuleName + "." + groupPath.back() + "." + interfaceName;
            declaredTypes.insert(moduleGroupPath);
        }
    }
    
    declaredTypes.insert(interfaceName);

    consume(VTokenType::Left_CB);
    std::vector<InterfaceMember> members;
    std::vector<std::shared_ptr<ASTNode>> methods;

    while (peekToken().type != VTokenType::Right_CB && !isAtEnd()) {
        if (lookAhead(1).type == VTokenType::Left_Parenthese) {
            Token methodName = consume(VTokenType::Identifier);
            consume(VTokenType::Left_Parenthese);
            
            std::vector<Parameter> params;
            if (peekToken().type != VTokenType::Right_Parenthese) {
                do {
                    if (!params.empty() && peekToken().type == VTokenType::Comma) {
                        consume(VTokenType::Comma);
                    }
                    
                    Token paramTok = consume(VTokenType::Identifier);
                    uint32_t pId = StringPool::instance().intern(paramTok.name);
                    VType pType = VType::Unknown;
                    bool isReference = false;

                    if (peekToken().type == VTokenType::Referencer) {
                        consume(VTokenType::Referencer);
                        isReference = true;
                    }
                    
                    if (peekToken().type == VTokenType::Extends) {
                        consume(VTokenType::Extends);
                        std::string typePath = parseTypePath();
                        pType = resolveType(typePath);

                        if (peekToken().type == VTokenType::Referencer) {
                            consume(VTokenType::Referencer);
                            isReference = true;
                        }
                    }
                    
                    params.emplace_back(pId, paramTok.name, pType, isReference);
                } while (peekToken().type == VTokenType::Comma);
            }
            consume(VTokenType::Right_Parenthese);
            
            VType retType = VType::Unknown;
            if (peekToken().type == VTokenType::Arrow) {
                consume(VTokenType::Arrow);
                Token typeTok = consume(VTokenType::Identifier);
                retType = resolveType(typeTok.name);
            }
            
            consume(VTokenType::Left_CB);
            std::vector<std::shared_ptr<ASTNode>> body;
            while (peekToken().type != VTokenType::Right_CB && peekToken().type != VTokenType::End) {
                body.emplace_back(parseStatement());
            }
            consume(VTokenType::Right_CB);
            
            auto methodNode = std::make_shared<FunctionNode>(
                currentModuleName,
                StringPool::instance().intern(methodName.name),
                methodName.name,
                std::move(params),
                std::move(body),
                retType
            );
            methods.push_back(methodNode);
        } else {
            Token memberName = consume(VTokenType::Identifier);
            consume(VTokenType::Extends);
            
            std::string typePath = parseTypePath();
            VType memberType = resolveType(typePath);

            if (peekToken().type == VTokenType::Referencer) {
                consume(VTokenType::Referencer);
                memberType = VType::Reference;
            }
            
            members.emplace_back(memberName.name, memberType, 0);
        }
        
        if (peekToken().type == VTokenType::Comma || peekToken().type == VTokenType::Semicolon) {
            consume(peekToken().type);
        }
    }

    consume(VTokenType::Right_CB);
    
    auto node = std::make_unique<InterfaceNode>(interfaceName, std::move(members), std::move(methods));
    node->lineNumber = line;
    node->setModuleName(currentModuleName);
    return node;
}

void Parser::consumeSemicolon() {
    Token t = peekToken();
    if (t.type == VTokenType::Semicolon) {
        consume(VTokenType::Semicolon);
    } else if (t.type != VTokenType::End && t.type != VTokenType::Right_CB) {
        throw std::runtime_error("Runtime/Compilation Error: Expected ';' at end of statement on line " 
            + std::to_string(t.line) + 
            ", but got '" + t.name + "' instead.");
    }
}

std::unique_ptr<ProgramNode> Parser::parseProgram(SymbolContainer& env) {
    std::vector<std::shared_ptr<ASTNode>> statements;

    while (peekToken().type != VTokenType::End) {
        statements.emplace_back(parseStatement());
    }

    return std::make_unique<ProgramNode>(std::move(statements));
}

std::unique_ptr<ASTNode> Parser::parseStatement() {
    Token current = peekToken();
    
    switch (current.type) {
        case VTokenType::Function:   return parseFunctionDefinition();
        case VTokenType::Left_CB:    return parseBlock();
        case VTokenType::Return:     return parseReturnStatement();
        case VTokenType::If:         return parseIfStatement();
        case VTokenType::While:      return parseWhileLoop();
        case VTokenType::Group:      return parseGroupDefinition();
        case VTokenType::Break:      
        case VTokenType::Continue:   return parseLoopControl(); 
        case VTokenType::Module:     return parseModuleStatement();
        case VTokenType::Dismiss:    return parseDismissStatement();
        case VTokenType::Use:        return parseImportModule();
        case VTokenType::Deploy:     return parseDeployModule();
        case VTokenType::Interface:  return parseInterfaceDefinition();
        case VTokenType::Ruleset:    return parseRuleset();
        case VTokenType::Enum:       return parseEnum();
        case VTokenType::Defer:      return parseDeferStatement();
        case VTokenType::Identifier:
        case VTokenType::Const: {
            int checkPos = 0;
            bool hasConst = (peekToken().type == VTokenType::Const);
            if (hasConst) checkPos++;

            if (lookAhead(checkPos).type == VTokenType::Identifier) {
                checkPos++;

                while (lookAhead(checkPos).type == VTokenType::Dot || 
                    lookAhead(checkPos).type == VTokenType::Left_Bracket) {
                    if (lookAhead(checkPos).type == VTokenType::Left_Bracket) {
                        int depth = 1; checkPos++;
                        while (depth > 0 && lookAhead(checkPos).type != VTokenType::End) {
                            if (lookAhead(checkPos).type == VTokenType::Left_Bracket) depth++;
                            if (lookAhead(checkPos).type == VTokenType::Right_Bracket) depth--;
                            checkPos++;
                        }
                    } else {
                        checkPos += 2;
                    }
                }

                VTokenType next = lookAhead(checkPos).type;
                if (next == VTokenType::Extends || next == VTokenType::Equals) {
                    return parseAssignment();
                }
            }

            if (hasConst) {
                throw std::runtime_error("Syntax Error: 'const' can only be used in assignments at line " + std::to_string(peekToken().line));
            }

            auto expr = parseExpression();
            consumeSemicolon();
            return expr;
        }
        default: {
            auto expr = parseExpression();
            consumeSemicolon(); 
            return expr;
        }
    }
}

std::unique_ptr<ASTNode> Parser::parseExpression() {
    return parseTernary();
}

std::unique_ptr<ASTNode> Parser::parseTernary() {
    auto expr = parseRange();

    if (peekToken().type == VTokenType::Question) {
        consume(VTokenType::Question);
        auto truePart = parseExpression();
        consume(VTokenType::Colon);
        auto falsePart = parseTernary();
        
        return std::make_unique<TernaryNode>(std::move(expr), std::move(truePart), std::move(falsePart));
    }

    return expr;
}

std::unique_ptr<ASTNode> Parser::parseRange() {
    auto left = parseLogicalOr();
    
    while (peekToken().type == VTokenType::Double_Dot) {
        Token opToken = getNextToken();
        auto right = parseLogicalOr();
        left = std::make_unique<RangeNode>(std::move(left), std::move(right));
    }
    return left;
}

std::unique_ptr<ASTNode> Parser::parseLogicalOr() {
    auto left = parseLogicalAnd();
    while (peekToken().type == VTokenType::Or) {
        Token opToken = getNextToken();
        auto right = parseLogicalAnd();
        left = std::make_unique<BinOpNode>(VTokenType::Or, std::move(left), std::move(right));
    }
    return left;
}

std::unique_ptr<ASTNode> Parser::parseLogicalAnd() {
    auto left = parseEquality();
    while (peekToken().type == VTokenType::And) {
        Token opToken = getNextToken();
        auto right = parseEquality();
        left = std::make_unique<BinOpNode>(VTokenType::And, std::move(left), std::move(right));
    }
    return left;
}

std::unique_ptr<ASTNode> Parser::parseEquality() {
    auto left = parseRelational();
    while (peekToken().type == VTokenType::Double_Equals || peekToken().type == VTokenType::Not_Equal) {
        Token opToken = getNextToken();
        auto right = parseRelational();
        left = std::make_unique<BinOpNode>(opToken.type, std::move(left), std::move(right));
    }
    return left;
}

std::unique_ptr<ASTNode> Parser::parseRelational() {
    auto left = parseAdditive();
    while (peekToken().type == VTokenType::Greater || peekToken().type == VTokenType::Smaller || 
           peekToken().type == VTokenType::Greater_Or_Equal || peekToken().type == VTokenType::Smaller_Or_Equal) {
        Token opToken = getNextToken();
        auto right = parseAdditive();
        left = std::make_unique<BinOpNode>(opToken.type, std::move(left), std::move(right));
    }
    return left;
}

std::unique_ptr<ASTNode> Parser::parseAdditive() {
    auto left = parseTerm();
    while (peekToken().type == VTokenType::Add || peekToken().type == VTokenType::Substract || peekToken().type == VTokenType::Floor_Divide || peekToken().type == VTokenType::Modulo) {
        Token opToken = getNextToken();
        auto right = parseTerm();
        left = std::make_unique<BinOpNode>(opToken.type, std::move(left), std::move(right));
    }
    return left;
}

std::unique_ptr<ASTNode> Parser::parseTerm() {
    auto left = parseUnary();
    while (peekToken().type == VTokenType::Multiply || peekToken().type == VTokenType::Division || peekToken().type == VTokenType::Power) {
        Token opToken = getNextToken();
        auto right = parseUnary();
        auto node = std::make_unique<BinOpNode>(opToken.type, std::move(left), std::move(right));
        node->lineNumber = opToken.line;
        left = std::move(node);
    }
    return left;
}

std::unique_ptr<ASTNode> Parser::parseUnary() {
    if (peekToken().type == VTokenType::Exclamatory || 
        peekToken().type == VTokenType::Substract   ||
        peekToken().type == VTokenType::Addresser) {
        Token opToken = getNextToken();
        
        auto right = parseUnary(); 

        auto node = std::make_unique<UnaryNode>(opToken.type, std::move(right));
        node->lineNumber = opToken.line;
        return node;
    }
    return parsePostfix();
}

std::unique_ptr<ASTNode> Parser::parsePostfix() {
    auto left = parseFactor();
    while (peekToken().type == VTokenType::Double_Increment || peekToken().type == VTokenType::Double_Decrement) {
        Token opToken = getNextToken();
        auto node = std::make_unique<PostFixNode>(opToken.type, std::move(left));
        node->lineNumber = opToken.line;
        left = std::move(node);
    }
    return left;
}

std::unique_ptr<ASTNode> Parser::parseFactor() {
    Token current = peekToken(); 
    switch (current.type) {
        case VTokenType::String:                  return parseStringLiteral();
        case VTokenType::Int64:                   return parseNumberLiteral();
        case VTokenType::Float64:                 return parseNumberLiteral();
        case VTokenType::True:
        case VTokenType::False:                   return parseBooleanLiteral();
        case VTokenType::Null:                    return parseNullLiteral();
        case VTokenType::Identifier:              return parseIdentifierExpr();
        case VTokenType::Left_Bracket:            return parseArrayLiteral();
        case VTokenType::Left_Parenthese:         return parseGroupingExpr();
        case VTokenType::BuiltIn:                 return parseBuiltInCall();
        case VTokenType::Through:                 return parseForLoop();
        case VTokenType::InterpolatedString:
            return parseInterpolatedString();
        case VTokenType::Left_CB: {               // ADDED DISAMBIGUATION
            if (lookAhead(1).type == VTokenType::Right_CB || 
                (lookAhead(1).type == VTokenType::String && lookAhead(2).type == VTokenType::Colon) ||
                (lookAhead(1).type == VTokenType::Identifier && lookAhead(2).type == VTokenType::Colon)) {
                return parseMapLiteral();
            }
            return parseBlock();
        }
        default:
            throw std::runtime_error("Unexpected token in factor: " + current.name + "[ line " + std::to_string(current.line) + " ]");
    }
}

std::unique_ptr<ASTNode> Parser::parseStringLiteral() {
    Token current = peekToken(); 
    int line = current.line;

    consume(VTokenType::String);

    std::string strVal = std::get<std::string>(current.literal);
    
    auto node = std::make_unique<StringNode>(strVal);
    node->lineNumber = line;

    return node;
}

std::unique_ptr<ASTNode> Parser::parseNumberLiteral() {
    Token current = peekToken(); 
    int line = current.line;

    if (current.type == VTokenType::Int64) {
        consume(VTokenType::Int64);
        int64_t intVal = std::get<int64_t>(current.literal);
        auto node = std::make_unique<NumberNode>(Value(intVal)); 
        node->lineNumber = line;
        return node;
    } 
    else if (current.type == VTokenType::Float64) {
        consume(VTokenType::Float64);
        double val = std::get<double>(current.literal);
        auto node = std::make_unique<NumberNode>(Value(val));
        node->lineNumber = line;
        return node;
    }

    throw std::runtime_error("Expected numeric literal at line " + std::to_string(line));
}

std::unique_ptr<ASTNode> Parser::parseBooleanLiteral() {
    Token tok = peekToken();
    bool value = (tok.type == VTokenType::True);
    consume(tok.type);
    
    auto node = std::make_unique<BooleanNode>(value);
    node->lineNumber = tok.line;
    return node;
}

std::unique_ptr<ASTNode> Parser::parseArrayLiteral() {
    Token tok = peekToken(); 
    int line = tok.line;

    consume(VTokenType::Left_Bracket);
    
    std::vector<std::unique_ptr<ASTNode>> elements;
    
    if (peekToken().type != VTokenType::Right_Bracket) {
        elements.emplace_back(parseExpression());
        
        while (peekToken().type == VTokenType::Comma) {
            consume(VTokenType::Comma);
            elements.emplace_back(parseExpression());
        }
    }
    
    consume(VTokenType::Right_Bracket);

    auto node = std::make_unique<ArrayNode>(std::move(elements));
    node->lineNumber = line;
    return node;
}

std::unique_ptr<ASTNode> Parser::parseMapLiteral() {
    int line = peekToken().line;
    consume(VTokenType::Left_CB);
    
    std::vector<std::pair<std::unique_ptr<ASTNode>, std::unique_ptr<ASTNode>>> pairs;
    
    if (peekToken().type != VTokenType::Right_CB) {
        do {
            if (peekToken().type == VTokenType::Comma) {
                consume(VTokenType::Comma);
            }
            if (peekToken().type == VTokenType::Right_CB) break;
            
            auto keyExpr = parseExpression();
            consume(VTokenType::Colon);
            auto valExpr = parseExpression();
            
            pairs.emplace_back(std::move(keyExpr), std::move(valExpr));
        } while (peekToken().type == VTokenType::Comma);
    }
    
    consume(VTokenType::Right_CB);

    auto node = std::make_unique<MapNode>(std::move(pairs));
    node->lineNumber = line;
    return node;
}

std::unique_ptr<ASTNode> Parser::parseNullLiteral() {
    int line = peekToken().line;
    consume(VTokenType::Null);
    auto node = std::make_unique<NullNode>();
    node->lineNumber = line;
    return node;
}

std::unique_ptr<ASTNode> Parser::parseGroupingExpr() {
    consume(VTokenType::Left_Parenthese);
    auto node = parseExpression();
    consume(VTokenType::Right_Parenthese);
    return node;
}

std::unique_ptr<ASTNode> Parser::parseFunctionDefinition() {
    Token funcTok = consume(VTokenType::Function);
    int line = funcTok.line;
    
    std::string targetModule = "";
    uint32_t funcId;
    std::string funcName;

    if (peekToken().type == VTokenType::Extends) {
        consume(VTokenType::Extends); 
        targetModule = consume(VTokenType::Identifier).name;
        
        Token actualFuncTok = consume(VTokenType::Identifier);
        funcName = actualFuncTok.name;
    } 
    else {
        Token firstTok = consume(VTokenType::Identifier);
        
        if (peekToken().type == VTokenType::Extends) {
            consume(VTokenType::Extends);

            targetModule = firstTok.name;

            Token actualFuncTok = consume(VTokenType::Identifier);
            funcName = actualFuncTok.name;

        } else {
            funcName = firstTok.name;
        }
    }
       
    if(targetModule == "vcore" || targetModule == "vglib"){
        throw std::runtime_error("Permission Error : Cannot inject function '" + funcName + "' to built-in module " + targetModule + " at line " + std::to_string(line));
    }

    funcId = StringPool::instance().intern(funcName);

    pushScope();

    consume(VTokenType::Left_Parenthese);
    std::vector<Parameter> params;

    if (peekToken().type != VTokenType::Right_Parenthese) {
        do {
            if (!params.empty() && peekToken().type == VTokenType::Comma) {
                consume(VTokenType::Comma);
            }

            Token paramTok = consume(VTokenType::Identifier);
            uint32_t pId = StringPool::instance().intern(paramTok.name);
            VType pType = VType::Unknown;

            bool isReference = false;

            if (peekToken().type == VTokenType::Referencer) {
                consume(VTokenType::Referencer);
                isReference = true;
            }

            if (peekToken().type == VTokenType::Extends) {
                consume(VTokenType::Extends);

                std::string typePath = parseTypePath();
                pType = resolveType(typePath);
                                
                if (peekToken().type == VTokenType::Referencer) {
                    consume(VTokenType::Referencer);
                    isReference = true;
                }
            }

            params.emplace_back(pId, paramTok.name, pType, isReference);

        } while (peekToken().type == VTokenType::Comma); 
    }
    consume(VTokenType::Right_Parenthese);
    
    VType retType = VType::Unknown;
    std::string typeName = "null";

    if (peekToken().type == VTokenType::Arrow) {
        consume(VTokenType::Arrow);
        std::string typePath = parseTypePath();
        retType = resolveType(typePath);
    }

    consume(VTokenType::Left_CB);
    std::vector<std::shared_ptr<ASTNode>> body;
    while (peekToken().type != VTokenType::Right_CB && peekToken().type != VTokenType::End) {
        body.emplace_back(parseStatement());
    }
    consume(VTokenType::Right_CB);

    popScope();

    auto node = std::make_unique<FunctionNode>(targetModule, funcId, funcName, std::move(params), std::move(body), retType);
    node->lineNumber = line;
    return node;
}

std::unique_ptr<ASTNode> Parser::parseBuiltInCall() {
    Token tok = consume(VTokenType::BuiltIn);
    int line = tok.line;
    
    consume(VTokenType::Left_Parenthese);
    
    std::vector<std::unique_ptr<ASTNode>> args;
    if (peekToken().type != VTokenType::Right_Parenthese) {
        do {
            if (peekToken().type == VTokenType::Comma) consume(VTokenType::Comma);
            args.emplace_back(parseExpression());
        } while (peekToken().type == VTokenType::Comma);
    }
    
    consume(VTokenType::Right_Parenthese);
    
    auto node = std::make_unique<BuiltInCallNode>(tok.name, std::move(args));
    node->lineNumber = line;
    return node;
}

std::unique_ptr<ASTNode> Parser::parseIdentifierExpr() {
    Token tok = consume(VTokenType::Identifier);
    int line = tok.line;
    bool isRefVar = false;

    if (tok.type == VTokenType::Return   || 
        tok.type == VTokenType::Function || 
        tok.type == VTokenType::BuiltIn) {
        throw std::runtime_error("Syntax Error: Unexpected keyword '" + tok.name + "'");
    }

    std::string lastName = tok.name;
    uint32_t currentId = StringPool::instance().intern(lastName);

    VType explicitType = VType::Unknown;
    if (peekToken().type == VTokenType::Extends) {
        consume(VTokenType::Extends);
        Token startTypeTok = peekToken();
        explicitType = resolveType(parseTypePath());

        if (peekToken().type == VTokenType::Referencer) {
            consume(VTokenType::Referencer);
            isRefVar = true;
            explicitType = VType::Reference;
        }

        if (explicitType == VType::Unknown) {
            throw std::runtime_error("Type Error : Unexpected type " + std::string(startTypeTok.name) + " [ line " + std::to_string(line) + " ]");
        }

        defineSymbol(currentId, explicitType, true, line, lastName);
    }
    std::vector<std::string> scope;
    std::unique_ptr<ASTNode> node;

    if (peekToken().type == VTokenType::Left_Parenthese) {
        consume(VTokenType::Left_Parenthese);
        std::vector<std::unique_ptr<ASTNode>> args;
        if (peekToken().type != VTokenType::Right_Parenthese) {
            do {
                if (peekToken().type == VTokenType::Comma) consume(VTokenType::Comma);
                args.emplace_back(parseExpression());
            } while (peekToken().type == VTokenType::Comma);
        }
        consume(VTokenType::Right_Parenthese);
        node = std::make_unique<FunctionCallNode>(currentId, lastName, std::move(args));
    } else {
        node = std::make_unique<VariableNode>(currentId, tok.name, explicitType, std::vector<std::string>{}, isRefVar);
    }

    while (peekToken().type == VTokenType::Dot || peekToken().type == VTokenType::Left_Bracket) {
        if (peekToken().type == VTokenType::Dot) {
            consume(VTokenType::Dot);
            Token member = consume(VTokenType::Identifier);

            if (peekToken().type == VTokenType::Left_Parenthese) {
                consume(VTokenType::Left_Parenthese);
                std::vector<std::unique_ptr<ASTNode>> args;
                if (peekToken().type != VTokenType::Right_Parenthese) {
                    do {
                        if (peekToken().type == VTokenType::Comma) consume(VTokenType::Comma);
                        args.emplace_back(parseExpression());
                    } while (peekToken().type == VTokenType::Comma);
                }
                consume(VTokenType::Right_Parenthese);
                node = std::make_unique<MethodCallNode>(std::move(node), member.name, std::move(args));
            } 
            else {
                auto memberNode = std::make_unique<MemberAccessNode>(std::move(node), member.name);
                memberNode->lineNumber = member.line;
                node = std::move(memberNode);
            }
        }
        else if (peekToken().type == VTokenType::Left_Bracket) {
            consume(VTokenType::Left_Bracket);
            auto indexExpr = parseExpression();
            consume(VTokenType::Right_Bracket);
            
            node = std::make_unique<IndexAccessNode>(std::move(node), std::move(indexExpr));
            node->lineNumber = line;
        }
    }

    node->lineNumber = line;
    return node;
}

std::unique_ptr<ASTNode> Parser::parseBlock() {
    int line = peekToken().line;
    consume(VTokenType::Left_CB);
    
    std::vector<std::shared_ptr<ASTNode>> statements;
    while (peekToken().type != VTokenType::Right_CB && peekToken().type != VTokenType::End) {
        statements.emplace_back(parseStatement());
    }
    
    consume(VTokenType::Right_CB);
    auto node = std::make_unique<BlockNode>(std::move(statements));
    node->lineNumber = line;
    return node;
}

std::unique_ptr<ASTNode> Parser::parseIfStatement() {
    int line = peekToken().line;
    consume(VTokenType::If);
    
    auto condition = parseExpression();
    
    auto body = parseStatement();

    std::unique_ptr<ASTNode> elseBody = nullptr;
    
    if (peekToken().type == VTokenType::Else) {
        consume(VTokenType::Else);
        
        if (peekToken().type == VTokenType::If) {
            elseBody = parseIfStatement(); 
        } else {
            elseBody = parseStatement();
        }
    }

    auto node = std::make_unique<IfNode>(std::move(condition), std::move(body), std::move(elseBody));
    node->lineNumber = line;
    return node;
}

std::unique_ptr<ASTNode> Parser::parseWhileLoop() {
    int line = peekToken().line;
    consume(VTokenType::While);
    
    auto condition = parseExpression();
    
    auto body = parseStatement();

    auto node = std::make_unique<WhileNode>(std::move(condition), std::move(body));
    node->lineNumber = line;
    return node;
}

std::unique_ptr<ASTNode> Parser::parseForLoop() {
    int line = peekToken().line;
    consume(VTokenType::Through);
    
    std::string iteratorName = "_";

    if (peekToken().type == VTokenType::Identifier && lookAhead(1).type == VTokenType::Extends) {
        iteratorName = consume(VTokenType::Identifier).name;
        consume(VTokenType::Extends);
    }

    auto iterable = parseExpression(); 

    if (peekToken().type == VTokenType::Arrow) {
        consume(VTokenType::Arrow);
    }

    std::string modeStr = consume(VTokenType::LoopMode).name;
    std::unique_ptr<ASTNode> body;
    
    if (peekToken().type == VTokenType::Left_CB) {
        body = parseBlock();
    } else {
        body = std::make_unique<VariableNode>(StringPool::instance().intern(iteratorName), iteratorName);
    }
    

    auto node = std::make_unique<ForNode>(std::move(iterable), std::move(body), iteratorName, ForNode::getForMode(modeStr));
    node->lineNumber = line;
    return node;
}

std::unique_ptr<ASTNode> Parser::parseAssignment() {
    int line = peekToken().line;
    bool isConst = false;

    if (peekToken().type == VTokenType::Const) {
        consume(VTokenType::Const);
        isConst = true;
    }

    auto lhs = parsePostfix();

    if (peekToken().type == VTokenType::Equals) {
        if (lhs->type() == NodeType::MEMBER_ACCESS) {
            auto* mem = static_cast<MemberAccessNode*>(lhs.get());
            consume(VTokenType::Equals);
            auto rhs = parseExpression();
            consumeSemicolon();

            auto node = std::make_unique<MemberAssignmentNode>(
                mem->takeReceiver(), 
                mem->getMemberName(),
                std::move(rhs)
            );
            node->lineNumber = line;
            return node;
        }
        
        if (lhs->type() == NodeType::INDEX_ACCESS) {
            auto* idx = static_cast<IndexAccessNode*>(lhs.get());
            consume(VTokenType::Equals);
            auto rhs = parseExpression();
            consumeSemicolon();
            
            auto base = idx->takeBase();
            auto index = idx->takeIndex();
            
            auto node = std::make_unique<IndexAssignmentNode>(
                std::move(base),
                std::move(index),
                std::move(rhs),
                isConst
            );
            node->lineNumber = line;
            return node;
        }
    }

    if (lhs->type() == NodeType::VARIABLE) [[likely]] {
        auto* var = static_cast<VariableNode*>(lhs.get());
        uint32_t varId = var->getNameId();
        std::string originalName = var->getOriginalName();
        
        VType varType = var->getStaticType();
        bool isReference = var->isRefVar();
        std::string customTypeName = "";
        
        bool hasTypeDecl = (peekToken().type == VTokenType::Extends) && (varType == VType::Unknown);
        
        if (hasTypeDecl) {
            consume(VTokenType::Extends);
            customTypeName = parseTypePath();
            varType = resolveType(customTypeName);

            if (peekToken().type == VTokenType::Referencer) {
                consume(VTokenType::Referencer);
                isReference = true;
            }
        } else if (varType == VType::Unknown) {
            if (Vyne::isTypeStrict()) {
                throw std::runtime_error(
                    "Type Error: Variable '" + originalName + 
                    "' requires explicit type declaration in strict mode. " +
                    "Use ':: <type>' or enable dynamic typing with `ruleset` declarations " +
                    "[ line " + std::to_string(line) + " ]"
                );
            } else {
                if (!Vyne::isQuietMode()) {
                    Vyne::warn("Implicitly typing '" + originalName + "' (dynamic mode)", line);
                }
            }
        }

        std::unique_ptr<ASTNode> rhs = nullptr;
        if (peekToken().type == VTokenType::Equals) {
            consume(VTokenType::Equals);
            rhs = parseExpression();
        } else {
            if (isReference) {
                rhs = std::make_unique<NullNode>();
            } else {
                switch (varType) {
                    case VType::String:  rhs = std::make_unique<StringNode>(""); break;
                    case VType::Int64:   rhs = std::make_unique<NumberNode>(Value((int64_t)0)); break;
                    case VType::Float64: rhs = std::make_unique<NumberNode>(Value(0.0)); break;
                    case VType::Bool:    rhs = std::make_unique<BooleanNode>(false); break;
                    case VType::Array:   rhs = std::make_unique<ArrayNode>(std::vector<std::unique_ptr<ASTNode>>()); break;
                    case VType::Struct:  rhs = std::make_unique<NullNode>(customTypeName); break;
                    default:             
                        if (!Vyne::isTypeStrict()) {
                            rhs = std::make_unique<NullNode>(); 
                        } else {
                            throw std::runtime_error(
                                "Type Error: Cannot default initialize '" + originalName + 
                                "' without type declaration in strict mode [ line " + 
                                std::to_string(line) + " ]"
                            );
                        }
                        break;
                }
            }
        }

        consumeSemicolon();
        
        if (varType != VType::Unknown || !Vyne::isTypeStrict()) {
            defineSymbol(varId, varType, varType != VType::Unknown, line, originalName);
        }

        auto node = std::make_unique<AssignmentNode>(
            varId, originalName, std::move(rhs), 
            isConst, isReference, varType, std::vector<std::string>{}
        );
        node->lineNumber = line;
        return node;
    }
    
    return lhs;
}

std::unique_ptr<ASTNode> Parser::parseEnum() {
    int line = peekToken().line;
    consume(VTokenType::Enum);

    Token enumTok = consume(VTokenType::Identifier);
    std::string enumName = enumTok.name;

    consume(VTokenType::Left_CB);

    std::unordered_map<std::string, uint32_t> members;
    uint32_t counter = 0;

    while (peekToken().type != VTokenType::Right_CB && !isAtEnd()) {
        Token memberTok = consume(VTokenType::Identifier);
        std::string memberName = memberTok.name;

        if (peekToken().type == VTokenType::Equals) {
            consume(VTokenType::Equals);
            Token valTok = consume(VTokenType::Int64);
            counter = static_cast<uint32_t>(std::get<int64_t>(valTok.literal));
        }

        members[memberName] = counter;
        counter++;

        if (peekToken().type == VTokenType::Comma || peekToken().type == VTokenType::Semicolon) {
            consume(peekToken().type);
        }
    }

    consume(VTokenType::Right_CB); // '}' uduruq
    consumeSemicolon();            // Optional ';' sığortası

    // Elan olunmuş tip kimi qeydiyyata alırıq ki, gələcəkdə static_type check-dən keçsin
    declaredTypes.insert(enumName);

    auto node = std::make_unique<EnumNode>(enumName, std::move(members));
    node->lineNumber = line;
    return node;
}

std::unique_ptr<ASTNode> Parser::parseGroupDefinition() {
    int line = peekToken().line;
    consume(VTokenType::Group);
    
    std::string targetModule = "";
    std::string groupName;

    if (peekToken().type == VTokenType::Extends) { 
        consume(VTokenType::Extends);
        targetModule = consume(VTokenType::Identifier).name;
        groupName = consume(VTokenType::Identifier).name;
    } 
    else {
        Token first = consume(VTokenType::Identifier);
        
        if (peekToken().type == VTokenType::Extends) { 
            consume(VTokenType::Extends);
            targetModule = consume(VTokenType::Identifier).name;
            groupName = first.name;
        } 
        else {
            groupName = first.name;
        }
    }
    
    if(targetModule == "vcore" || targetModule == "vglib"){
        throw std::runtime_error("Permission Error: Cannot inject group to " + targetModule + " [ line " + std::to_string(line) + " ]");
    }

    std::string oldGroupName = currentGroupName;
    std::vector<std::string> oldGroupPath = groupPath;
    
    currentGroupName = groupName;
    groupPath.push_back(groupName);
    
    consume(VTokenType::Left_CB);

    std::vector<std::unique_ptr<ASTNode>> statements;

    while (peekToken().type != VTokenType::Right_CB && peekToken().type != VTokenType::End) {
        if (peekToken().type == VTokenType::Function) {
            throw std::runtime_error("Syntax Error: Cannot define a function inside group '" + 
                groupName + "' at line " + std::to_string(peekToken().line));
        }
        statements.emplace_back(parseStatement());
    }
    
    consume(VTokenType::Right_CB);
    consumeSemicolon();

    // Restore old path
    currentGroupName = oldGroupName;
    groupPath = oldGroupPath;

    auto node = std::make_unique<GroupNode>(groupName, std::move(statements), targetModule);
    node->lineNumber = line;
    return node;
}

std::unique_ptr<ASTNode> Parser::parseReturnStatement() {
    int line = peekToken().line;
    consume(VTokenType::Return);
    
    std::unique_ptr<ASTNode> expr = nullptr;

    if (peekToken().type == VTokenType::Semicolon) {
        expr = std::make_unique<NullNode>();
    } else {
        expr = parseExpression();
    }
    
    consumeSemicolon();
    
    auto node = std::make_unique<ReturnNode>(std::move(expr));
    node->lineNumber = line;
    return node;
}

std::unique_ptr<ASTNode> Parser::parseLoopControl() {
    Token tok = getNextToken();
    consumeSemicolon();
    
    std::unique_ptr<ASTNode> node;
    if (tok.type == VTokenType::Break) {
        node = std::make_unique<BreakNode>();
    } else {
        node = std::make_unique<ContinueNode>();
    }
    
    node->lineNumber = tok.line;
    return node;
}

std::unique_ptr<ASTNode> Parser::parseModuleStatement() {
    int line = peekToken().line;
    consume(VTokenType::Module);
    
    Token nameToken = consume(VTokenType::Identifier);
    currentModuleName = nameToken.name;
    uint32_t mId = StringPool::instance().intern(nameToken.name);
    consumeSemicolon();
    
    auto node = std::make_unique<ModuleNode>(mId, nameToken.name);
    node->lineNumber = line;
    return node;
}

std::unique_ptr<ASTNode> Parser::parseDismissStatement() {
    int line = peekToken().line;
    consume(VTokenType::Dismiss);
    
    Token nameToken = consume(VTokenType::Identifier);
    uint32_t mId = StringPool::instance().intern(nameToken.name);
    consumeSemicolon();
    
    auto node = std::make_unique<DismissNode>(mId, nameToken.name);
    node->lineNumber = line;
    return node;
}

std::unique_ptr<ASTNode> Parser::parseRuleset() {
    int line = peekToken().line;
    consume(VTokenType::Ruleset);
    
    if (peekToken().type == VTokenType::Left_CB) {
        return parseRulesetBlock(line);
    }
    
    Token rulesetType = consume(VTokenType::Identifier);
    
    if (peekToken().type == VTokenType::Semicolon) {
        consume(VTokenType::Semicolon);
    }
    
    if (rulesetType.type == VTokenType::Warnings) {
        Vyne::setQuietMode(false);
    } else if (rulesetType.type == VTokenType::Dynamic_Casting) {
        Vyne::setTypeStrictMode(false);
    } else if (rulesetType.type == VTokenType::Memory_Limit) {
        Vyne::setMemoryLimitEnabled(true);
    }
    
    auto node = std::make_unique<NullNode>();
    node->lineNumber = line;
    return node;
}

std::unique_ptr<ASTNode> Parser::parseRulesetBlock(int line) {
    consume(VTokenType::Left_CB);
    
    while (peekToken().type != VTokenType::Right_CB && !isAtEnd()) {
        // İdentifier gözləmirik, çünki bunlar artıq keyword-dür
        Token ruleName = getNextToken(); 
        
        if (peekToken().type == VTokenType::Equals) {
            consume(VTokenType::Equals);
            
            Token valueToken = consume(VTokenType::Int64); 

            double value = 0;
            if (std::holds_alternative<int64_t>(valueToken.literal)) 
                value = static_cast<double>(std::get<int64_t>(valueToken.literal));
            else if (std::holds_alternative<double>(valueToken.literal))
                value = std::get<double>(valueToken.literal);

            if (ruleName.type == VTokenType::Memory_Limit) { 
                Vyne::setMemoryLimitEnabled(true);
                Vyne::setMemoryLimit(static_cast<size_t>(value));
                if (!Vyne::isQuietMode())
                    std::cout << "[RULES] Memory limit set to: " << value << " bytes\n";
            } else {
                throw std::runtime_error("Rule '" + ruleName.name + "' requires a value, but is unknown.");
            }
        } 
        else {
            if (ruleName.type == VTokenType::Warnings) {
                Vyne::setQuietMode(false);
            } else if (ruleName.type == VTokenType::Dynamic_Casting) {
                Vyne::setTypeStrictMode(false);
            } else {
                throw std::runtime_error("Unknown ruleset flag: " + ruleName.name);
            }
        }

        if (peekToken().type == VTokenType::Comma || peekToken().type == VTokenType::Semicolon) {
            getNextToken();
        }
    }
    
    consume(VTokenType::Right_CB);
    consumeSemicolon();
    
    auto node = std::make_unique<NullNode>();
    node->lineNumber = line;
    return node;
}
std::unique_ptr<ASTNode> Parser::parseInterpolatedString() {
    Token tok = getNextToken();
    auto parts = std::get<std::vector<std::pair<std::string, bool>>>(tok.literal);
    
    auto node = std::make_unique<InterpolatedStringNode>();
    node->lineNumber = tok.line;
    
    for (const auto& [part, isExpr] : parts) {
        if (isExpr) {
            auto exprTokens = tokenize(part);
            Parser exprParser(std::move(exprTokens));
            auto expr = exprParser.parseExpression();
            
            if (!expr) {
                throw std::runtime_error(
                    "Failed to parse expression: " + part +
                    " at line " + std::to_string(tok.line)
                );
            }
            
            node->addExpressionPart(std::move(expr));
        } else {
            node->addStringPart(part);
        }
    }
    
    return node;
}

std::unique_ptr<ASTNode> Parser::parseDeferStatement() {
    int line = peekToken().line;
    consume(VTokenType::Defer);
    
    auto body = parseStatement();
    
    auto node = std::make_unique<DeferNode>(std::move(body));
    node->lineNumber = line;
    return node;
}
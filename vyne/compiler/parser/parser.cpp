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

// TODO ADD DOUBLE INCREMENT SYNTAX

Token Parser::getNextToken() {
    if (pos < tokens.size()) {
        return tokens[pos++];
    }
    return Token(VTokenType::End, 0, 0, "");
}

Token Parser::peekToken() {
    if (pos < tokens.size()) {
        return tokens[pos];
    }
    return Token(VTokenType::End, 0, 0, "");
}

Token Parser::lookAhead(int distance) {
    if (pos + distance < tokens.size()) {
        return tokens[pos + distance];
    }
    return Token(VTokenType::End, 0, 0, "");
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
    std::string filePath = pathTok.name;

    std::string alias = "";
    if (peekToken().type == VTokenType::As) {
        consume(VTokenType::As);
        alias = consume(VTokenType::Identifier).name;
    }

    consumeSemicolon();

    auto node = std::make_unique<ImportNode>(filePath, isExtern, alias);
    node->lineNumber = line;
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

std::unique_ptr<ProgramNode> Parser::parseProgram() {
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
    return parseRange();
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
        case VTokenType::Identifier:              return parseIdentifierExpr();
        case VTokenType::Left_Bracket:            return parseArrayLiteral();
        case VTokenType::Left_Parenthese:         return parseGroupingExpr();
        case VTokenType::BuiltIn:                 return parseBuiltInCall();
        case VTokenType::Through:                 return parseForLoop();
        default:
            throw std::runtime_error("Unexpected token in factor: " + current.name + "[ line " + std::to_string(current.line) + " ]");
    }
}

std::unique_ptr<ASTNode> Parser::parseStringLiteral() {
    Token current = peekToken(); 
    int line = current.line;

    consume(VTokenType::String);

    auto node = std::make_unique<StringNode>(current.name);
    node->lineNumber = line;

    return node;
}

std::unique_ptr<ASTNode> Parser::parseNumberLiteral() {
    Token current = peekToken(); 
    int line = current.line;

    if (current.type == VTokenType::Int64) {
        consume(VTokenType::Int64);
        int64_t intVal = static_cast<int64_t>(current.value); 
        auto node = std::make_unique<NumberNode>(Value(intVal)); 
        node->lineNumber = line;
        return node;
    } 
    else if (current.type == VTokenType::Float64) {
        consume(VTokenType::Float64);
        auto node = std::make_unique<NumberNode>(Value(current.value));
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

    consume(VTokenType::Left_Parenthese);
    std::vector<uint32_t> params;
    if (peekToken().type != VTokenType::Right_Parenthese) {
        params.emplace_back(StringPool::instance().intern(consume(VTokenType::Identifier).name));
        while (peekToken().type == VTokenType::Comma) {
            consume(VTokenType::Comma);
            params.emplace_back(StringPool::instance().intern(consume(VTokenType::Identifier).name));
        }
    }
    consume(VTokenType::Right_Parenthese);

    consume(VTokenType::Left_CB);
    std::vector<std::shared_ptr<ASTNode>> body;
    while (peekToken().type != VTokenType::Right_CB && peekToken().type != VTokenType::End) {
        body.emplace_back(parseStatement());
    }
    consume(VTokenType::Right_CB);

    auto node = std::make_unique<FunctionNode>(targetModule, funcId, funcName, std::move(params), std::move(body));
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
        Token typeTok = consume(VTokenType::Identifier);

        explicitType = stringToVType(typeTok.name);
        if (explicitType == VType::Unknown) {
            throw std::runtime_error("Type Error : Unexpected type " + std::string(typeTok.name) + " [ line " + std::to_string(line) + " ]");
        }

        defineSymbol(currentId, explicitType, true);
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
        node = std::make_unique<VariableNode>(currentId, tok.name, explicitType);
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
            } else {
                scope.emplace_back(lastName);
                lastName = member.name;
                uint32_t memberId = StringPool::instance().intern(member.name);
                node = std::make_unique<VariableNode>(memberId, lastName, explicitType, scope);
            }
        } 
        else if (peekToken().type == VTokenType::Left_Bracket) {
            consume(VTokenType::Left_Bracket);
            auto indexExpr = parseExpression();
            consume(VTokenType::Right_Bracket);
            
            uint32_t lastId = StringPool::instance().intern(lastName);
            node = std::make_unique<IndexAccessNode>(lastId, lastName, scope, std::move(indexExpr));
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

    Token nameTok = consume(VTokenType::Identifier);
    uint32_t varId = StringPool::instance().intern(nameTok.name);
    std::string originalName = nameTok.name;
    
    VType varType = VType::Unknown;
    if (peekToken().type == VTokenType::Extends) {
        consume(VTokenType::Extends);
        varType = stringToVType(consume(VTokenType::Identifier).name);
    }

    std::unique_ptr<ASTNode> rhs = nullptr;
    if (peekToken().type == VTokenType::Equals) {
        consume(VTokenType::Equals);
        rhs = parseExpression();
    } else {
        switch (varType) {
            case VType::String:  rhs = std::make_unique<StringNode>(""); break;
            case VType::Int64:   rhs = std::make_unique<NumberNode>(Value((int64_t)0)); break;
            case VType::Float64: rhs = std::make_unique<NumberNode>(Value(0.0)); break;
            case VType::Array:   rhs = std::make_unique<ArrayNode>(std::vector<std::unique_ptr<ASTNode>>()); break;
            default:             rhs = std::make_unique<NumberNode>(0); break; 
        }
    }

    consumeSemicolon();

    defineSymbol(varId, varType, varType != VType::Unknown);

    auto node = std::make_unique<AssignmentNode>(
        varId, originalName, std::move(rhs), isConst, varType, std::vector<std::string>{}
    );
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
            consume(peekToken().type);
            targetModule = first.name;
            groupName = consume(VTokenType::Identifier).name;
        } else {
            groupName = first.name;
        }
    }
    
    if(targetModule == "vcore" || targetModule == "vglib"){
        throw std::runtime_error("Permission Error: Cannot inject group to " + targetModule + " [ line " + std::to_string(line) + " ]");
    }

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

    auto node = std::make_unique<GroupNode>(groupName, std::move(statements), targetModule);
    node->lineNumber = line;
    return node;
}

std::unique_ptr<ASTNode> Parser::parseReturnStatement() {
    int line = peekToken().line;
    consume(VTokenType::Return);
    
    auto expr = parseExpression();
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

#include "emitter.h"
#include "../ast/ast.h"

Chunk compile(std::shared_ptr<ASTNode> root) {
    Chunk chunk;
    Emitter emitter(&chunk);
    if (root) root->compile(emitter);
    emitter.emitReturn(); 
    return chunk;
}

void ProgramNode::compile(Emitter& e) const {
    for (const auto& stmt : statements) {
        if (stmt) stmt->compile(e);
    }
}

void BlockNode::compile(Emitter& e) const {
    e.beginScope();
    for (const auto& stmt : statements) {
        if (stmt) stmt->compile(e);
    }
    e.endScope(); 
}

// --- Literallar (Sabitlər) ---

void NumberNode::compile(Emitter& e) const {
    e.emitConstant(value);
}

void StringNode::compile(Emitter& e) const {
    // String-i hələ də VM daxilində string kimi saxlaya bilərik, 
    // lakin adlar artıq ID-dir.
    e.emitConstant(Value(text));
}

void BooleanNode::compile(Emitter& e) const {
    e.emitConstant(Value(condition));
}

void NullNode::compile(Emitter& e) const {
    e.emitConstant(Value());
}

// --- Dəyişənlər və Assignment (ID-yə keçid) ---

void VariableNode::compile(Emitter& e) const {
    // Lokal dəyişənləri hələ də adla axtarırıq (kompilyasiya vaxtı), 
    // amma bytecode-da ancaq index qalır.
    int arg = e.resolveLocal(originalName);
    if (arg != -1) {
        e.emitBytes(OP_GET_LOCAL, (uint8_t)arg);
    } else {
        // Qlobal: String yerinə nameId-ni (double olaraq) sabit kimi əlavə edirik
        int nameIndex = e.currentChunk->addConstant(Value((double)nameId));
        e.emitBytes(OP_GET_GLOBAL, (uint8_t)nameIndex);
    }
}

void AssignmentNode::compile(Emitter& e) const {
    rhs->compile(e); 

    int arg = e.resolveLocal(originalName);
    if (arg != -1) {
        e.emitBytes(OP_SET_LOCAL, (uint8_t)arg);
    } else if (e.scopeDepth > 0) {
        e.addLocal(originalName);
        int localIdx = e.resolveLocal(originalName);
        e.emitBytes(OP_SET_LOCAL, (uint8_t)localIdx);
    } else {
        // Qlobal: identifierId (uint32_t) istifadə olunur
        int nameIndex = e.currentChunk->addConstant(Value((double)identifierId));
        e.emitBytes(OP_DEFINE_GLOBAL, (uint8_t)nameIndex);
    }
}

// --- Riyazi və Məntiqi Əməliyyatlar ---

void BinOpNode::compile(Emitter& e) const {
    if (op == VTokenType::And) {
        left->compile(e);
        int endJump = e.emitJump(OP_JUMP_IF_FALSE);
        e.emitByte(OP_POP);
        right->compile(e);
        e.patchJump(endJump);
    } else if (op == VTokenType::Or) {
        left->compile(e);
        int elseJump = e.emitJump(OP_JUMP_IF_FALSE);
        int endJump = e.emitJump(OP_JUMP);
        e.patchJump(elseJump);
        e.emitByte(OP_POP);
        right->compile(e);
        e.patchJump(endJump);
    } else {
        left->compile(e);
        right->compile(e);
        switch (op) {
            case VTokenType::Add:      e.emitByte(OP_ADD); break;
            case VTokenType::Substract:e.emitByte(OP_SUBTRACT); break;
            case VTokenType::Multiply: e.emitByte(OP_MULTIPLY); break;
            case VTokenType::Division: e.emitByte(OP_DIVIDE); break;
            case VTokenType::Double_Equals: e.emitByte(OP_EQUAL); break;
            case VTokenType::Greater:  e.emitByte(OP_GREATER); break;
            case VTokenType::Smaller:  e.emitByte(OP_SMALLER); break;
            case VTokenType::Modulo:   e.emitByte(OP_MODULO); break;
            default: break;
        }
    }
}

void PostFixNode::compile(Emitter& e) const {
    left->compile(e); 
    e.emitConstant(Value(1.0));
    e.emitByte(OP_ADD);

    if (auto* var = dynamic_cast<VariableNode*>(left.get())) {
        int arg = e.resolveLocal(var->getOriginalName());
        if (arg != -1) {
            e.emitBytes(OP_SET_LOCAL, (uint8_t)arg);
        } else {
            int nameIdx = e.currentChunk->addConstant(Value((double)var->getNameId()));
            e.emitBytes(OP_DEFINE_GLOBAL, (uint8_t)nameIdx);
        }
    }
    e.emitByte(OP_POP);
}

void UnaryNode::compile(Emitter& e) const {
    right->compile(e);
    switch (op) {
        case VTokenType::Exclamatory: e.emitByte(OP_NOT); break;
        case VTokenType::Substract:   e.emitByte(OP_NEGATE); break;
        default: break;
    }
}

// --- Funksiyalar (ID-yə keçid) ---

void FunctionNode::compile(Emitter& e) const {
    Chunk* funcChunk = new Chunk(); 
    Emitter funcEmitter(funcChunk);
    funcEmitter.scopeDepth = 1; 

    for (const auto& param : parameters) {
        funcEmitter.addLocal(param.name);
    }

    for (const auto& stmt : body) {
        if (stmt) stmt->compile(funcEmitter);
    }
    funcEmitter.emitReturn();

    auto funcData = std::make_shared<FunctionData>();
    funcData->params = this->parameters;
    funcData->bytecode = funcChunk; 
    funcData->isNative = false;
    funcData->expectedReturnType = VTypeToString(returnType);

    Value funcVal(funcData);
    int constIdx = e.currentChunk->addConstant(funcVal);
    e.emitBytes(OP_CONSTANT, (uint8_t)constIdx);

    // Funksiya adı ID olaraq saxlanılır
    int nameIdx = e.currentChunk->addConstant(Value((double)funcNameId));
    e.emitBytes(OP_DEFINE_GLOBAL, (uint8_t)nameIdx);
}

void FunctionCallNode::compile(Emitter& e) const {
    for (const auto& arg : arguments) {
        arg->compile(e);
    }

    // targetNameId artıq hazırdır
    int nameIndex = e.currentChunk->addConstant(Value((double)targetNameId));
    e.emitBytes(OP_GET_GLOBAL, (uint8_t)nameIndex);
    e.emitBytes(OP_CALL, (uint8_t)arguments.size());
}

// --- Control Flow ---

void IfNode::compile(Emitter& e) const {
    condition->compile(e);
    int thenJump = e.emitJump(OP_JUMP_IF_FALSE);
    e.emitByte(OP_POP); 

    body->compile(e);
    int elseJump = e.emitJump(OP_JUMP);

    e.patchJump(thenJump);
    e.emitByte(OP_POP);

    if (elseBody) elseBody->compile(e);
    e.patchJump(elseJump);
}

void WhileNode::compile(Emitter& e) const {
    int loopStart = e.currentChunk->code.size();
    condition->compile(e);
    
    int exitJump = e.emitJump(OP_JUMP_IF_FALSE);
    e.emitByte(OP_POP);

    body->compile(e);
    e.emitLoop(loopStart);

    e.patchJump(exitJump);
    e.emitByte(OP_POP);
}

// --- Massivlər və Property-lər ---

void ArrayNode::compile(Emitter& e) const {
    for (const auto& element : elements) {
        element->compile(e);
    }
    e.emitBytes(OP_ARRAY, (uint8_t)elements.size());
}

void IndexAccessNode::compile(Emitter& e) const {
    base->compile(e);
    index->compile(e);
    e.emitByte(OP_INDEX_GET);
}

void MemberAccessNode::compile(Emitter& e) const {
    receiver->compile(e);
    // memberName yerinə memberId (uint32_t) istifadə olunur
    int memberIdx = e.currentChunk->addConstant(Value((double)memberId));
    e.emitBytes(OP_GET_PROPERTY, (uint8_t)memberIdx);
}

void MemberAssignmentNode::compile(Emitter& e) const {
    receiver->compile(e); 
    rhs->compile(e);
    
    // memberName-i ID-yə çevirib sabitlərə əlavə et
    uint32_t mId = StringPool::instance().intern(memberName);
    int memberIdx = e.currentChunk->addConstant(Value((double)mId));
    e.emitBytes(OP_SET_PROPERTY, (uint8_t)memberIdx);
}

// --- Digərləri ---

void ReturnNode::compile(Emitter& e) const {
    if (expression) {
        expression->compile(e);
    } else {
        e.emitConstant(Value()); // NULL
    }
    e.emitReturn();
}

void BreakNode::compile(Emitter& e) const {
    e.emitJump(OP_JUMP); 
}

void ContinueNode::compile(Emitter& e) const {
    e.emitLoop(0); 
}

void GroupNode::compile(Emitter& e) const {
    for (const auto& stmt : statements) {
        if (stmt) stmt->compile(e);
    }
}

void ModuleNode::compile(Emitter& e) const {
    int nameIdx = e.currentChunk->addConstant(Value((double)moduleId));
    e.emitBytes(OP_CONSTANT, (uint8_t)nameIdx);
    e.emitBytes(OP_DEFINE_GLOBAL, (uint8_t)nameIdx);
}

void ImportNode::compile(Emitter& e) const {}

void DismissNode::compile(Emitter& e) const {
    int nameIdx = e.currentChunk->addConstant(Value((double)moduleId));
    e.emitBytes(OP_CONSTANT, (uint8_t)nameIdx);
}

void RangeNode::compile(Emitter& e) const {
    left->compile(e);
    right->compile(e);
    e.emitBytes(OP_ARRAY, 2); 
}

void IndexAssignmentNode::compile(Emitter& e) const {
    base->compile(e);   
    index->compile(e);  
    rhs->compile(e);    
    e.emitByte(OP_INDEX_SET); 
}

void InterfaceNode::compile(Emitter& e) const {
    auto funcData = std::make_shared<FunctionData>();
    funcData->isNative = true; 
    
    int constIdx = e.currentChunk->addConstant(Value(funcData));
    e.emitBytes(OP_CONSTANT, (uint8_t)constIdx);
    
    uint32_t iId = StringPool::instance().intern(interfaceName);
    int nameIdx = e.currentChunk->addConstant(Value((double)iId));
    e.emitBytes(OP_DEFINE_GLOBAL, (uint8_t)nameIdx);
}

void BuiltInCallNode::compile(Emitter& e) const {
    for (const auto& arg : arguments) {
        arg->compile(e);
    }
    
    if (funcName == "out") e.emitByte(OP_PRINT);
    else if (funcName == "type") e.emitByte(OP_TYPE);
    else {
        uint32_t fId = StringPool::instance().intern(funcName);
        int nameIdx = e.currentChunk->addConstant(Value((double)fId));
        e.emitBytes(OP_GET_GLOBAL, (uint8_t)nameIdx);
        e.emitBytes(OP_CALL, (uint8_t)arguments.size());
    }
}

void MethodCallNode::compile(Emitter& e) const {
    for (const auto& arg : arguments) {
        arg->compile(e);
    }
    receiver->compile(e);
    
    uint32_t mId = StringPool::instance().intern(methodName);
    int nameIdx = e.currentChunk->addConstant(Value((double)mId));
    e.emitBytes(OP_GET_PROPERTY, (uint8_t)nameIdx);
    e.emitBytes(OP_CALL, (uint8_t)arguments.size());
}

void ForNode::compile(Emitter& e) const {    
    iterable->compile(e);
    int loopStart = e.currentChunk->code.size();
    body->compile(e);
    e.emitLoop(loopStart);
}

void TernaryNode::compile(Emitter& e) const {
    condition->compile(e);

    int thenJump = e.emitJump(OP_JUMP_IF_FALSE);

    e.emitByte(OP_POP);
    trueExpr->compile(e);

    int elseJump = e.emitJump(OP_JUMP);

    e.patchJump(thenJump);
    e.emitByte(OP_POP);

    falseExpr->compile(e);

    e.patchJump(elseJump);
}
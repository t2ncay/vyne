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
    e.endScope(); // Burada avtomatik OP_POP-lar emit olunur (Emitter-də yazmışdıq)
}

// --- Literallar (Sabitlər) ---

void NumberNode::compile(Emitter& e) const {
    e.emitConstant(value);
}

void StringNode::compile(Emitter& e) const {
    e.emitConstant(Value(text));
}

void BooleanNode::compile(Emitter& e) const {
    e.emitConstant(Value(condition));
}

void NullNode::compile(Emitter& e) const {
    e.emitConstant(Value());
}

// --- Dəyişənlər və Assignment ---

void VariableNode::compile(Emitter& e) const {
    int arg = e.resolveLocal(originalName);
    if (arg != -1) {
        // Lokal: O(1) sürəti ilə stack-dən götür
        e.emitBytes(OP_GET_LOCAL, (uint8_t)arg);
    } else {
        // Qlobal: Map-dən axtar
        int nameIndex = e.currentChunk->addConstant(Value(originalName));
        e.emitBytes(OP_GET_GLOBAL, (uint8_t)nameIndex);
    }
}

void AssignmentNode::compile(Emitter& e) const {
    rhs->compile(e); // Dəyər stack-in başına çıxır

    int arg = e.resolveLocal(originalName);
    if (arg != -1) {
        e.emitBytes(OP_SET_LOCAL, (uint8_t)arg);
    } else if (e.scopeDepth > 0) {
        
        e.addLocal(originalName);
        int localIdx = e.resolveLocal(originalName);
        e.emitBytes(OP_SET_LOCAL, (uint8_t)localIdx);
    } else {
        // Qlobal dəyişən
        int nameIndex = e.currentChunk->addConstant(Value(originalName));
        e.emitBytes(OP_DEFINE_GLOBAL, (uint8_t)nameIndex);
    }
}

// --- Riyazi və Məntiqi Əməliyyatlar ---

void BinOpNode::compile(Emitter& e) const {
    // Qısa-qapanma (Short-circuit) məntiqi (And/Or üçün)
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
    // 1. Dəyişənin cari dəyərini stack-ə gətir (məs: i)
    left->compile(e); 

    // 2. Artırılacaq vahidi (1) stack-ə qoy
    e.emitConstant(Value(1));

    // 3. Topla
    e.emitByte(OP_ADD);

    // 4. Nəticəni eyni dəyişənə geri yaz
    if (auto* var = dynamic_cast<VariableNode*>(left.get())) {
        int arg = e.resolveLocal(var->getOriginalName());
        if (arg != -1) {
            e.emitBytes(OP_SET_LOCAL, (uint8_t)arg);
        } else {
            int nameIdx = e.currentChunk->addConstant(Value(var->getOriginalName()));
            e.emitBytes(OP_DEFINE_GLOBAL, (uint8_t)nameIdx);
        }
    }

    // 5. Kritik: i++ bir statement kimidir, stack-də artıq qalan dəyəri təmizlə
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

// --- Funksiyalar və Çağırışlar ---

void FunctionNode::compile(Emitter& e) const {
    // 1. Yeni funksiya üçün Chunk yarat
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
    funcData->bytecode = funcChunk; // <--- Artıq xəta verməyəcək
    funcData->isNative = false;
    funcData->expectedReturnType = VTypeToString(returnType);

    Value funcVal(funcData);

    int constIdx = e.currentChunk->addConstant(funcVal);
    e.emitBytes(OP_CONSTANT, (uint8_t)constIdx);

    int nameIdx = e.currentChunk->addConstant(Value(originalName));
    e.emitBytes(OP_DEFINE_GLOBAL, (uint8_t)nameIdx);
}

void FunctionCallNode::compile(Emitter& e) const {
    // Argumentləri stack-ə yığ
    for (const auto& arg : arguments) {
        arg->compile(e);
    }

    // Funksiya obyektini tap və çağır
    int nameIndex = e.currentChunk->addConstant(Value(originalName));
    e.emitBytes(OP_GET_GLOBAL, (uint8_t)nameIndex);
    e.emitBytes(OP_CALL, (uint8_t)arguments.size());
}

// --- Control Flow (If, While) ---

void IfNode::compile(Emitter& e) const {
    condition->compile(e);
    int thenJump = e.emitJump(OP_JUMP_IF_FALSE);
    e.emitByte(OP_POP); // Şərt nəticəsini təmizlə

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

// --- Massiv və Obyektlər ---

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
    int memberIdx = e.currentChunk->addConstant(Value(memberName));
    e.emitBytes(OP_GET_PROPERTY, (uint8_t)memberIdx);
}

// --- Digər (Break, Continue, Return) ---

void ReturnNode::compile(Emitter& e) const {
    if (expression) {
        expression->compile(e);
    } else {
        e.emitByte(OP_CONSTANT); // NULL qaytar
        e.emitByte(e.currentChunk->addConstant(Value()));
    }
    e.emitReturn();
}

void BreakNode::compile(Emitter& e) const {
    e.emitJump(OP_JUMP); 
}

void ContinueNode::compile(Emitter& e) const {
    // Loop-un başına qayıtmaq üçün
    e.emitLoop(0); // Emitter loopStart-ı idarə etməlidir
}

void GroupNode::compile(Emitter& e) const {
    // Group-lar əsasən ad sahəsidir (namespace). 
    // Daxilindəki bütün statement-ləri ardıcıl kompilyasiya edirik.
    for (const auto& stmt : statements) {
        if (stmt) stmt->compile(e);
    }
}

void ModuleNode::compile(Emitter& e) const {
    // VM-də bu modulun setup funksiyasını tetikləmək üçün OP_CONSTANT + OP_DEFINE_GLOBAL
    // Və ya birbaşa VM-in tanıdığı OP_MODULE_LOAD (əgər varsa) istifadə oluna bilər.
    int nameIdx = e.currentChunk->addConstant(Value(originalName));
    e.emitBytes(OP_CONSTANT, (uint8_t)nameIdx);
    e.emitBytes(OP_DEFINE_GLOBAL, (uint8_t)nameIdx);
}

void ImportNode::compile(Emitter& e) const {
    // Import zamanı hədəf fayl artıq parse olunub AST-yə çevrilməlidir.
    // VM səviyyəsində bu, həmin AST-nin bytecode-a çevrilib icra olunması deməkdir.
    // Hələlik boş saxlamaq olar və ya 'eval' kimi bir opcode emit edə bilərsən.
}

void DismissNode::compile(Emitter& e) const {
    // Bir modulu yaddaşdan silmək üçün VM-ə siqnal göndəririk.
    int nameIdx = e.currentChunk->addConstant(Value(originalName));
    e.emitBytes(OP_CONSTANT, (uint8_t)nameIdx);
    // e.emitByte(OP_DISMISS); // Əgər OP_DISMISS opcode-u yaratmısansa
}

// --- Massiv və Range Əməliyyatları ---

void RangeNode::compile(Emitter& e) const {
    // range(left, right) -> Vyne-da bu bir array yaradır.
    left->compile(e);
    right->compile(e);
    // VM-də bunu handle edəcək OP_RANGE əlavə edə bilərsən və ya array-ə çevirə bilərsən.
    // Hələlik array kimi simulyasiya edirik:
    e.emitBytes(OP_ARRAY, 2); 
}

void IndexAssignmentNode::compile(Emitter& e) const {
    // array[index] = value
    base->compile(e);   // Array obyektini stack-ə qoy
    index->compile(e);  // İndeksi stack-ə qoy
    rhs->compile(e);    // Dəyəri stack-ə qoy
    
    e.emitByte(OP_INDEX_SET); // VM: pop(value), pop(index), pop(array)
}

// --- Struct və Property Məntiqi ---

void MemberAssignmentNode::compile(Emitter& e) const {
    // receiver.memberName = rhs
    receiver->compile(e); 
    rhs->compile(e);
    
    int memberIdx = e.currentChunk->addConstant(Value(memberName));
    e.emitBytes(OP_SET_PROPERTY, (uint8_t)memberIdx);
}

void InterfaceNode::compile(Emitter& e) const {
    // Interface Vyne-da Struct yaradıcısı (constructor) kimidir.
    // Bu, əslində bir FunctionNode kimi davranır.
    auto funcData = std::make_shared<FunctionData>();
    funcData->isNative = true; // Struct yaradılması native bir prosesdir
    
    int constIdx = e.currentChunk->addConstant(Value(funcData));
    e.emitBytes(OP_CONSTANT, (uint8_t)constIdx);
    
    int nameIdx = e.currentChunk->addConstant(Value(interfaceName));
    e.emitBytes(OP_DEFINE_GLOBAL, (uint8_t)nameIdx);
}

// --- Çağırışlar və Dövrələr ---

void BuiltInCallNode::compile(Emitter& e) const {
    // out(), exit(), type() və s.
    for (const auto& arg : arguments) {
        arg->compile(e);
    }
    
    // VM-də hər built-in üçün ayrı opcode və ya vahid OP_BUILTIN istifadə et.
    if (funcName == "out") e.emitByte(OP_PRINT);
    else if (funcName == "type") e.emitByte(OP_TYPE);
    else {
        int nameIdx = e.currentChunk->addConstant(Value(funcName));
        e.emitBytes(OP_GET_GLOBAL, (uint8_t)nameIdx);
        e.emitBytes(OP_CALL, (uint8_t)arguments.size());
    }
}

void MethodCallNode::compile(Emitter& e) const {
    // receiver.methodName(args)
    
    // 1. Argumentləri stack-ə yığ
    for (const auto& arg : arguments) {
        arg->compile(e);
    }
    
    // 2. Receiver-i stack-ə qoy
    receiver->compile(e);
    
    // 3. Metodu tap (Get Property)
    int nameIdx = e.currentChunk->addConstant(Value(methodName));
    e.emitBytes(OP_GET_PROPERTY, (uint8_t)nameIdx);
    
    // 4. Çağır
    e.emitBytes(OP_CALL, (uint8_t)arguments.size());
}

void ForNode::compile(Emitter& e) const {    
    iterable->compile(e);
    
    // 2. Dövrənin başlanğıcı
    int loopStart = e.currentChunk->code.size();
    
    // 3. VM-də iterasiyanı idarə edəcək bir opcode lazımdır (məsələn OP_ITER_NEXT)
    // Hələlik while-a bənzər strukturla simulyasiya edə bilərsən.
    // Bu hissə Vyne-ın iterasiya protokolundan asılıdır.
    
    body->compile(e);
    
    e.emitLoop(loopStart);
}
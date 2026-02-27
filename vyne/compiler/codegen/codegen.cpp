#include "emitter.h"
#include "../ast/ast.h"

Chunk compile(std::shared_ptr<ASTNode> root) {
    Chunk chunk;
    Emitter emitter(&chunk);

    if (root) {
        root->compile(emitter);
    }

    

    emitter.emitReturn(); 
    return chunk;
}

void NumberNode::compile(Emitter& e) const {
    e.emitConstant(Value(value));
}

void StringNode::compile(Emitter& e) const {
    e.emitConstant(Value(text));
}

void BooleanNode::compile(Emitter& e) const {
    e.emitConstant(Value(condition));
}

void BinOpNode::compile(Emitter& e) const {
    left->compile(e);
    right->compile(e);

    switch (op) {
        case VTokenType::Add:   e.emitByte(OP_ADD); break;
        case VTokenType::Substract:  e.emitByte(OP_SUBTRACT); break;
        case VTokenType::Multiply:   e.emitByte(OP_MULTIPLY); break;
        case VTokenType::Division:  e.emitByte(OP_DIVIDE); break;
        case VTokenType::Double_Equals : e.emitByte(OP_EQUAL); break;
        case VTokenType::Greater : e.emitByte(OP_GREATER); break;
        case VTokenType::Smaller : e.emitByte(OP_SMALLER); break;
        default: break;
    }
}

void PostFixNode::compile(Emitter& e) const {}

void UnaryNode::compile(Emitter& e) const {}

void ProgramNode::compile(Emitter& e) const {
    for (const auto& stmt : statements) {
        if (stmt) stmt->compile(e);
    }
}

void GroupNode::compile(Emitter& e) const {
    for (const auto& stmt : statements) {
        if (stmt) stmt->compile(e);
    }
}

void VariableNode::compile(Emitter& e) const {
    int nameIndex = e.currentChunk->addConstant(Value(originalName));

    e.emitBytes(OP_GET_GLOBAL, static_cast<uint8_t>(nameIndex));
}

void AssignmentNode::compile(Emitter& e) const {
    rhs->compile(e);

    int nameIndex = e.currentChunk->addConstant(Value(originalName));

    e.emitBytes(OP_DEFINE_GLOBAL, (uint8_t)nameIndex);
}

void BuiltInCallNode::compile(Emitter& e) const {
    for (const auto& arg : arguments) {
        arg->compile(e);
    }

    if (funcName == "log") {
        e.emitByte(OP_PRINT);
    }
    else if (funcName == "type") {
        e.emitByte(OP_TYPE);
    }
}
void ArrayNode::compile(Emitter& e) const {
    for (const auto& element : elements) {
        element->compile(e);
    }

    e.emitByte(OP_ARRAY);
    e.emitByte(static_cast<uint8_t>(elements.size()));
}

void RangeNode::compile(Emitter& e) const {}
void IndexAccessNode::compile(Emitter& e) const {}
void FunctionNode::compile(Emitter& e) const {}
void FunctionCallNode::compile(Emitter& e) const {}
void MethodCallNode::compile(Emitter& e) const {}
void WhileNode::compile(Emitter& e) const {
    int loopStart = e.currentChunk->code.size();

    condition->compile(e);

    int exitJump = e.emitJump(OP_JUMP_IF_FALSE);

    body->compile(e);

    e.emitLoop(loopStart);

    e.patchJump(exitJump);
}

void ForNode::compile(Emitter& e) const {};

void BlockNode::compile(Emitter& e) const {
    for (const auto& stmt : statements) {
        if (stmt) stmt->compile(e);
    }
}
void ModuleNode::compile(Emitter& e) const {}
void ImportNode::compile(Emitter& e) const {}
void DismissNode::compile(Emitter& e) const {}
void IfNode::compile(Emitter& e) const {
    condition->compile(e); 
    int jumpAddress = e.emitJump(OP_JUMP_IF_FALSE);

    e.emitByte(OP_POP);
    if (body) body->compile(e);

    e.patchJump(jumpAddress);
}
void InterfaceNode::compile(Emitter& e) const {}
void MemberAccessNode::compile(Emitter& e) const {}
void BreakNode::compile(Emitter& e) const {}
void ContinueNode::compile(Emitter& e) const {}
void NullNode::compile(Emitter& e) const {}

void ReturnNode::compile(Emitter& e) const {
    if (expression) expression->compile(e);
    e.emitReturn();
}
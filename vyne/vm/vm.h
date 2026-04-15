#ifndef VYNE_VM_H
#define VYNE_VM_H

#include "../compiler/ast/ast.h"
#include "../compiler/codegen/chunk.h"
#include <vector>
#include <array>

enum InterpretResult {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
};

// Hər funksiya çağırışı üçün bir "Frame"
struct CallFrame {
    Chunk* chunk;
    uint8_t* ip;
    Value* slots; // Bu frame-in lokal dəyişənlərinin stack-də başladığı yer
};

class VM {
    static const int FRAMES_MAX = 64;
    static const int STACK_MAX = 1024;

    std::array<CallFrame, FRAMES_MAX> frames;
    int frameCount = 0;

    std::array<Value, STACK_MAX> stack;
    Value* stackTop;

    SymbolContainer& globals; 
    std::string currentGroup = "global";

public:
    VM(SymbolContainer& env) : globals(env) {
        resetStack();
    }

    InterpretResult interpret(Chunk& chunk);
    InterpretResult run();
    
    void resetStack() {
        stackTop = stack.data();
        frameCount = 0;
    }

    void push(Value value) {
        if (stackTop >= stack.data() + STACK_MAX) throw std::runtime_error("Stack Overflow");
        *stackTop = value;
        stackTop++;
    }

    Value pop() {
        stackTop--;
        return *stackTop;
    }

    Value peek(int distance) {
        return stackTop[-1 - distance];
    }

    Value handleDynamicProperty(Value& receiver, uint32_t nameId);
};

#endif
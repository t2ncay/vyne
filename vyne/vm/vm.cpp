#include "vm.h"
#include <iostream>
#include <algorithm>
#include <cmath>

InterpretResult VM::interpret(Chunk& c) {
    resetStack();
    CallFrame* frame = &frames[frameCount++];
    frame->chunk = &c;
    frame->ip = c.code.data();
    frame->slots = stack.data();
    
    // Lokal 0 adətən 'self' və ya qlobal context üçün rezerv olunur
    push(Value()); 

    return run();
}

InterpretResult VM::run() {
    CallFrame* frame = &frames[frameCount - 1];

    #define READ_BYTE() (*frame->ip++)

    #define READ_SHORT() \
        (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))

    #define READ_CONSTANT() (frame->chunk->constants[READ_BYTE()])

    #define READ_STRING() (READ_CONSTANT().stringId)

    #define BINARY_OP(op) \
        do { \
            Value b = pop(); \
            Value a = pop(); \
            if (a.getType() == Value::FLOAT64 || b.getType() == Value::FLOAT64) { \
                push(Value(a.asFloat() op b.asFloat())); \
            } else { \
                push(Value(a.asInt() op b.asInt())); \
            } \
        } while (false)

    for (;;) {
        uint8_t instruction = READ_BYTE();
        switch (instruction) {
            case OP_CONSTANT: push(READ_CONSTANT()); break;

            // --- Lokal və Qlobal Dəyişənlər ---
            case OP_GET_LOCAL: push(frame->slots[READ_BYTE()]); break;
            case OP_SET_LOCAL: frame->slots[READ_BYTE()] = peek(0); break;

            case OP_DEFINE_GLOBAL: {
                globals[currentGroup][READ_STRING()] = pop();
                break;
            }
            case OP_GET_GLOBAL: {
                uint32_t nameId = READ_STRING();
                auto it = globals[currentGroup].find(nameId);
                if (it != globals[currentGroup].end()) push(it->second);
                else if (globals["global"].count(nameId)) push(globals["global"][nameId]);
                else return INTERPRET_RUNTIME_ERROR;
                break;
            }
            case OP_SET_GLOBAL: {
                uint32_t nameId = READ_STRING();
                globals[currentGroup][nameId] = peek(0);
                break;
            }

            // --- Riyaziyyat və Məntiq ---
            case OP_ADD: {
                if (peek(0).getType() == Value::STRING && peek(1).getType() == Value::STRING) {
                    Value b = pop(); Value a = pop();
                    push(Value(a.asString() + b.asString()));
                } else BINARY_OP(+);
                break;
            }
            case OP_SUBTRACT: BINARY_OP(-); break;
            case OP_MULTIPLY: BINARY_OP(*); break;
            case OP_DIVIDE:   BINARY_OP(/); break;
            case OP_MODULO: {
                int64_t b = pop().asInt();
                int64_t a = pop().asInt();
                push(Value(a % b));
                break;
            }
            case OP_EQUAL:   push(Value(pop() == pop())); break;
            case OP_GREATER: BINARY_OP(>); break;
            case OP_SMALLER: BINARY_OP(<); break;
            case OP_NOT:     push(Value(!pop().isTruthy())); break;

            // --- Control Flow ---
            case OP_JUMP:          frame->ip += READ_SHORT(); break;
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_SHORT();
                if (!peek(0).isTruthy()) frame->ip += offset;
                break;
            }
            case OP_LOOP:          frame->ip -= READ_SHORT(); break;

            // --- Obyekt və Struct Məntiqi (Member Access) ---
            case OP_GET_PROPERTY: {
                Value receiver = pop();
                uint32_t nameId = READ_STRING();

                if (receiver.getType() == Value::STRUCT) {
                    auto strct = receiver.asStruct();
                    if (strct->fields.count(nameId)) push(strct->fields[nameId]);
                    else if (strct->methods.count(nameId)) push(strct->methods[nameId]);
                    else return INTERPRET_RUNTIME_ERROR;
                } else if (receiver.getType() == Value::MODULE) {
                    push(globals[receiver.asModule()][nameId]);
                } else {
                    // Dinamik metodlar (Array.length, String.size və s.)
                    push(handleDynamicProperty(receiver, nameId));
                }
                break;
            }

            case OP_SET_PROPERTY: {
                Value value = pop();
                Value receiver = pop();
                uint32_t nameId = READ_STRING();

                if (receiver.getType() == Value::STRUCT) {
                    receiver.asStruct()->fields[nameId] = value;
                    push(value); // Chaining üçün
                } else return INTERPRET_RUNTIME_ERROR;
                break;
            }

            // --- Funksiya Çağırışı (Native və Vyne) ---
            case OP_CALL: {
                int argCount = READ_BYTE();
                Value callee = peek(argCount);

                if (callee.getType() == Value::FUNCTION) {
                    auto funcData = callee.asFunction();
                    
                    if (funcData->isNative) {
                        // Native C++ funksiyasını çağır
                        std::vector<Value> args;
                        for (int i = 0; i < argCount; i++) args.push_back(pop());
                        std::reverse(args.begin(), args.end());
                        pop(); // Funksiyanın özünü çıxar
                        push(funcData->nativeFn(args));
                    } else {
                        // Yeni Vyne Frame yarat
                        if (frameCount >= FRAMES_MAX) return INTERPRET_RUNTIME_ERROR;
                        CallFrame* nextFrame = &frames[frameCount++];
                        nextFrame->chunk = funcData->bytecode;
                        nextFrame->ip = funcData->bytecode->code.data();
                        nextFrame->slots = stackTop - argCount - 1;
                        frame = nextFrame;
                    }
                } else return INTERPRET_RUNTIME_ERROR;
                break;
            }

            case OP_RETURN: {
                Value result = pop();
                frameCount--;
                if (frameCount == 0) return INTERPRET_OK;
                stackTop = frame->slots;
                push(result);
                frame = &frames[frameCount - 1];
                break;
            }

            // --- Massivlər ---
            case OP_ARRAY: {
                uint8_t count = READ_BYTE();
                std::vector<Value> elements(count);
                for (int i = count - 1; i >= 0; i--) elements[i] = pop();
                push(Value(std::move(elements)));
                break;
            }
            case OP_INDEX_GET: {
                Value index = pop();
                Value array = pop();
                push(array.asList()[index.asInt()]);
                break;
            }
            case OP_INDEX_SET: {
                Value val = pop();
                Value index = pop();
                Value array = pop();
                array.asList()[index.asInt()] = val;
                push(val);
                break;
            }

            case OP_PRINT: pop().print(std::cout); std::cout << "\n"; break;
            case OP_TYPE:  push(Value(pop().getTypeName())); break;
            case OP_POP:   pop(); break;
        }
    }
}

// Vyne üçün dinamik property-ləri idarə edən helper
Value VM::handleDynamicProperty(Value& receiver, uint32_t nameId) {
    std::string name = StringPool::instance().get(nameId);
    
    if (receiver.getType() == Value::ARRAY) {
        if (name == "size" || name == "length") return Value((int64_t)receiver.asList().size());
    }
    if (receiver.getType() == Value::STRING) {
        if (name == "size" || name == "length") return Value((int64_t)receiver.asString().size());
    }
    
    return Value(); // Null
}
#include "vm.h"
#include <iostream>

InterpretResult VM::interpret(Chunk& c) {
    this->chunk = &c;
    this->ip = chunk->code.data();
    return run();
}

InterpretResult VM::run() {
    #define READ_BYTE() (*ip++)
    #define READ_CONSTANT() (chunk->constants[READ_BYTE()])

    for (;;) {
        uint8_t instruction = READ_BYTE();
        switch (instruction) {
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                push(constant); 
                break;
            }
            case OP_ADD: {
                Value b = pop();
                Value a = pop();

                double result = a.asNumber() + b.asNumber();
                push(Value(result));
                break;
            }
            case OP_SUBTRACT : {
                Value b = pop();
                Value a = pop();

                double result = a.asNumber() - b.asNumber();
                push(Value(result));
                break;
            }
            case OP_MULTIPLY : {
                Value b = pop();
                Value a = pop();

                double result = a.asNumber() * b.asNumber();
                push(Value(result));
                break;
            }
            case OP_DIVIDE : {
                Value b = pop();
                Value a = pop();

                if (b.asNumber() == 0) {
                    std::cerr << "Runtime Error: Division by zero." << std::endl;
                    return INTERPRET_RUNTIME_ERROR;
                }

                double result = a.asNumber() / b.asNumber();
                push(Value(result));
                break;
            }

            case OP_DEFINE_GLOBAL: {
                Value nameValue = READ_CONSTANT();
                uint32_t nameId = std::get<uint32_t>(nameValue.data); 
                Value val = pop();
                this->globals[this->currentGroup][nameId] = val;
                break;
            }
            case OP_GET_GLOBAL: {
            Value nameValue = READ_CONSTANT();
            
            uint32_t nameId = std::get<uint32_t>(nameValue.data); 

            auto& table = globals[currentGroup];
            auto it = table.find(nameId);

            if (it != table.end()) {
                push(it->second);
            } else {
                std::string name = nameValue.asString(); 
                std::cerr << "Runtime Error: Undefined variable '" << name << "'.\n";
                return INTERPRET_RUNTIME_ERROR;
            }
            break;
        }
            case OP_JUMP_IF_FALSE : {
                uint16_t offset = static_cast<uint16_t>((ip[0] << 8) | ip[1]);
                ip += 2;

                Value condition = pop();
                if(!condition.isTruthy()){
                    ip += offset;
                }
                break;
            }
            case OP_EQUAL : {
                Value b = pop();
                Value a = pop();
                push(Value(a == b));
                break;
            }
            case OP_GREATER : {
                Value b = pop();
                Value a = pop();
                push(Value(std::get<double>(a.data) > std::get<double>(b.data)));
                break;
            }
            case OP_SMALLER : {
                Value b = pop();
                Value a = pop();
                push(Value(std::get<double>(a.data) < std::get<double>(b.data)));
                break;
            }
            case OP_POP : {
                stack.pop_back();
                break;
            }
            case OP_PRINT : {
                Value val = pop();
                val.print(std::cout);
                std::cout << "\n";
                break;
            }
            case OP_TYPE : {
                Value val = pop();
                push(Value(val.getTypeName()));
                break;
            }
            case OP_ARRAY : {
                uint8_t count = READ_BYTE();
                std::vector<Value> arrayElements;

                arrayElements.reserve(count);

                for (int i = 0; i < count; i++) {
                    arrayElements.push_back(pop());
                }

                std::reverse(arrayElements.begin(), arrayElements.end());

                push(Value(arrayElements));
                break;
            }
            case OP_LOOP: {
                uint16_t offset = (uint16_t)((ip[0] << 8) | ip[1]);
                ip += 2;
                ip -= offset;
                break;
            }
            case OP_RETURN: {
                Value finalResult;
                if (!stack.empty()) {
                    finalResult = pop();
                } else {
                    finalResult = Value();
                }

                std::cout << "Result: ";
                finalResult.print(std::cout);
                std::cout << std::endl;
                return INTERPRET_OK;
            }
        }
    }

    #undef READ_BYTE
    #undef READ_CONSTANT
}
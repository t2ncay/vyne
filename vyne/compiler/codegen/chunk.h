#ifndef VYNE_CHUNK_H
#define VYNE_CHUNK_H

#include <vector>
#include <cstdint>
#include "../ast/value.h"

enum OpCode : uint8_t {
    OP_CONSTANT,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_EQUAL,
    OP_GREATER,
    OP_SMALLER,
    OP_NOT,
    OP_NEGATE,
    OP_POP,
    OP_DEFINE_GLOBAL,
    OP_GET_GLOBAL,
    OP_SET_GLOBAL,
    OP_GET_LOCAL,    // Lokal dəyişənlər üçün lazımdır
    OP_SET_LOCAL,    // Lokal təyinat üçün lazımdır
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_LOOP,
    OP_CALL,
    OP_ARRAY,
    OP_INDEX_GET,    // Sənin xəta verdiyi sətir üçün bu mütləqdir
    OP_INDEX_SET,    // array[idx] = val üçün lazım olacaq
    OP_PRINT,
    OP_TYPE,
    OP_RETURN,
    OP_GET_PROPERTY,
    OP_SET_PROPERTY,
    OP_MODULO
};

struct Chunk {
    std::vector<uint8_t> code;
    std::vector<Value> constants;
    std::vector<int> lines;

    void write(uint8_t byte, int line) {
        code.push_back(byte);
        lines.push_back(line);
    }

    int addConstant(Value value) {
        constants.push_back(value);
        return constants.size() - 1;
    }
};

void disassembleChunk(const Chunk& chunk, const std::string& name);
int disassembleInstruction(const Chunk& chunk, int offset);

#endif
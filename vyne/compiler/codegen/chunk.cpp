#include "chunk.h"
#include <iostream>
#include <iomanip>
#include <cstdio>

// --- Helper funksiyalar (Kodun oxunaqlığını artırmaq üçün) ---

static int simpleInstruction(const char* name, int offset) {
    std::printf("%s\n", name);
    return offset + 1;
}

static int byteInstruction(const char* name, const Chunk& chunk, int offset) {
    uint8_t slot = chunk.code[offset + 1];
    std::printf("%-16s %4d\n", name, slot);
    return offset + 2;
}

static int constantInstruction(const char* name, const Chunk& chunk, int offset) {
    uint8_t constantIndex = chunk.code[offset + 1];
    std::printf("%-16s %4d '", name, constantIndex);
    chunk.constants[constantIndex].print(std::cout);
    std::printf("'\n");
    return offset + 2;
}

static int jumpInstruction(const char* name, int sign, const Chunk& chunk, int offset) {
    uint16_t jump = (uint16_t)(chunk.code[offset + 1] << 8);
    jump |= chunk.code[offset + 2];
    std::printf("%-16s %4d -> %04d\n", name, offset, offset + 3 + sign * jump);
    return offset + 3;
}

// --- Əsas Disassembler Məntiqi ---

int disassembleInstruction(const Chunk& chunk, int offset) {
    // Offset və sətir nömrəsi
    std::printf("%04d ", offset);
    if (offset > 0 && chunk.lines[offset] == chunk.lines[offset - 1]) {
        std::printf("   | ");
    } else {
        std::printf("%4d ", chunk.lines[offset]);
    }

    uint8_t instruction = chunk.code[offset];
    switch (instruction) {
        // Riyazi
        case OP_ADD:      return simpleInstruction("OP_ADD", offset);
        case OP_SUBTRACT: return simpleInstruction("OP_SUBTRACT", offset);
        case OP_MULTIPLY: return simpleInstruction("OP_MULTIPLY", offset);
        case OP_DIVIDE:   return simpleInstruction("OP_DIVIDE", offset);
        case OP_MODULO:   return simpleInstruction("OP_MODULO", offset);
        case OP_NEGATE:   return simpleInstruction("OP_NEGATE", offset);

        // Müqayisə və Məntiq
        case OP_EQUAL:    return simpleInstruction("OP_EQUAL", offset);
        case OP_GREATER:  return simpleInstruction("OP_GREATER", offset);
        case OP_SMALLER:  return simpleInstruction("OP_SMALLER", offset);
        case OP_NOT:      return simpleInstruction("OP_NOT", offset);
        
        // Sabitlər və Qloballar
        case OP_CONSTANT:      return constantInstruction("OP_CONSTANT", chunk, offset);
        case OP_DEFINE_GLOBAL: return constantInstruction("OP_DEFINE_GLOBAL", chunk, offset);
        case OP_GET_GLOBAL:    return constantInstruction("OP_GET_GLOBAL", chunk, offset);
        case OP_SET_GLOBAL:    return constantInstruction("OP_SET_GLOBAL", chunk, offset);

        // Lokallar (İndex ilə)
        case OP_GET_LOCAL:     return byteInstruction("OP_GET_LOCAL", chunk, offset);
        case OP_SET_LOCAL:     return byteInstruction("OP_SET_LOCAL", chunk, offset);

        // Control Flow (Jumps)
        case OP_JUMP:          return jumpInstruction("OP_JUMP", 1, chunk, offset);
        case OP_JUMP_IF_FALSE: return jumpInstruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
        case OP_LOOP:          return jumpInstruction("OP_LOOP", -1, chunk, offset);

        // Çağırışlar və Obyektlər
        case OP_CALL:          return byteInstruction("OP_CALL", chunk, offset);
        case OP_ARRAY:         return byteInstruction("OP_ARRAY", chunk, offset);
        case OP_INDEX_GET:     return simpleInstruction("OP_INDEX_GET", offset);
        case OP_INDEX_SET:     return simpleInstruction("OP_INDEX_SET", offset);
        case OP_GET_PROPERTY:  return constantInstruction("OP_GET_PROPERTY", chunk, offset);

        // Sistem
        case OP_PRINT:    return simpleInstruction("OP_PRINT", offset);
        case OP_TYPE:     return simpleInstruction("OP_TYPE", offset);
        case OP_POP:      return simpleInstruction("OP_POP", offset);
        case OP_RETURN:   return simpleInstruction("OP_RETURN", offset);

        default:
            std::printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}

void disassembleChunk(const Chunk& chunk, const std::string& name) {
    std::cout << "\n== " << name << " ==" << std::endl;

    for (int offset = 0; offset < (int)chunk.code.size(); ) {
        // Əgər bu bir funksiya sabitidirsə, onun daxilini də disassemble etmək olar
        offset = disassembleInstruction(chunk, offset);
    }
    std::cout << "== end of " << name << " ==\n" << std::endl;
}
#ifndef cvm_chunk_h
#define cvm_chunk_h

#include "common.h"
#include "value.h"
#include <vector>
#include <cstdint>
#include <string>

// Instruction Set Architecture (Opcodes)
enum OpCode {
    OP_CONSTANT,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_NEGATE,
    OP_NOT,
    OP_PRINT,
    OP_POP,
    OP_DEFINE_GLOBAL,
    OP_GET_GLOBAL,
    OP_SET_GLOBAL,
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    OP_CALL,
    OP_BUILD_ARRAY,
    OP_BUILD_MAP,
    OP_INDEX_GET,
    OP_INDEX_SET,
    OP_GET_UPVALUE,
    OP_SET_UPVALUE,
    OP_CLOSURE,
    OP_CLOSE_UPVALUE,
    OP_CLASS,
    OP_METHOD,
    OP_GET_PROPERTY,
    OP_SET_PROPERTY,
    OP_INHERIT,
    OP_GET_SUPER,
    OP_TRUE,
    OP_FALSE,
    OP_EQUAL,
    OP_GREATER,
    OP_LESS,
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_LOOP,
    OP_RETURN,
    OP_TRY,
    OP_END_TRY,
    OP_THROW,
    OP_IMPORT,
    OP_COLLECTION_SIZE, // READ_BYTE() slot → push collection.size
    OP_ITER_GET,        // READ_BYTE() collSlot, READ_BYTE() idxSlot → push element/key
    OP_JUMP_BREAK,      // Unconditional jump patched to loop exit (break)
    OP_JUMP_CONTINUE,   // Unconditional jump patched to loop start (continue)
};

class Chunk {
public:
    Chunk() = default;
    
    void writeChunk(uint8_t byte, int line);
    int addConstant(Value value);
    int addString(const std::string& str);
    void freeChunk();

    std::vector<uint8_t> code;
    std::vector<int> lines;
    ValueArray constants;
    std::vector<std::string> strings;
};

#endif

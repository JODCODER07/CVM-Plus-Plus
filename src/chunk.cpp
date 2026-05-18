#include "chunk.h"

void Chunk::writeChunk(uint8_t byte, int line) {
    code.push_back(byte);
    lines.push_back(line);
}

int Chunk::addConstant(Value value) {
    constants.writeValue(value);
    return constants.values.size() - 1;
}

int Chunk::addString(const std::string& str) {
    for (size_t i = 0; i < strings.size(); ++i) {
        if (strings[i] == str) return i;
    }
    strings.push_back(str);
    return strings.size() - 1;
}

void Chunk::freeChunk() {
    code.clear();
    lines.clear();
    constants.freeValueArray();
    strings.clear();
}

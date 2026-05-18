#include "serializer.h"
#include "vm.h"
#include <fstream>
#include <iostream>
#include <cstring>

// ─────────────────────────────────────────────
//  Primitive writers
// ─────────────────────────────────────────────

void Serializer::writeUint32(std::ostream& out, uint32_t v) {
    uint8_t bytes[4] = {
        (uint8_t)(v & 0xff),
        (uint8_t)((v >> 8) & 0xff),
        (uint8_t)((v >> 16) & 0xff),
        (uint8_t)((v >> 24) & 0xff)
    };
    out.write((char*)bytes, 4);
}

void Serializer::writeInt32(std::ostream& out, int32_t v) {
    writeUint32(out, (uint32_t)v);
}

void Serializer::writeDouble(std::ostream& out, double d) {
    uint8_t bytes[8];
    std::memcpy(bytes, &d, 8);
    out.write((char*)bytes, 8);
}

void Serializer::writeString(std::ostream& out, const std::string& s) {
    writeUint32(out, (uint32_t)s.size());
    out.write(s.data(), s.size());
}

// ─────────────────────────────────────────────
//  Primitive readers
// ─────────────────────────────────────────────

uint32_t Serializer::readUint32(std::istream& in) {
    uint8_t bytes[4];
    in.read((char*)bytes, 4);
    return (uint32_t)bytes[0]
         | ((uint32_t)bytes[1] << 8)
         | ((uint32_t)bytes[2] << 16)
         | ((uint32_t)bytes[3] << 24);
}

int32_t Serializer::readInt32(std::istream& in) {
    return (int32_t)readUint32(in);
}

double Serializer::readDouble(std::istream& in) {
    uint8_t bytes[8];
    in.read((char*)bytes, 8);
    double d;
    std::memcpy(&d, bytes, 8);
    return d;
}

std::string Serializer::readString(std::istream& in) {
    uint32_t len = readUint32(in);
    if (len == 0) return "";
    std::string s(len, '\0');
    in.read(&s[0], len);
    return s;
}

// ─────────────────────────────────────────────
//  Value serialization
// ─────────────────────────────────────────────

void Serializer::writeValue(std::ostream& out, const Value& value) {
    if (value.isNumber()) {
        out.put(CVMC_TAG_NUMBER);
        writeDouble(out, value.asNumber());
    } else if (value.isBool()) {
        out.put(CVMC_TAG_BOOLEAN);
        out.put(value.asBool() ? 1 : 0);
    } else if (value.isString()) {
        out.put(CVMC_TAG_STRING);
        writeString(out, value.asString());
    } else if (value.isFunction()) {
        out.put(CVMC_TAG_FUNCTION);
        auto fn = value.asFunction();
        writeUint32(out, (uint32_t)fn->arity);
        writeUint32(out, (uint32_t)fn->upvalueCount);
        writeString(out, fn->name);
        writeChunk(out, *fn->chunk);
    } else {
        // Closures/Classes/Instances are runtime objects – not serializable.
        // Emit a zero number placeholder. This should not occur for compiled scripts.
        std::cerr << "[CVMC] Warning: non-serializable value type encountered. Writing nil placeholder.\n";
        out.put(CVMC_TAG_NUMBER);
        writeDouble(out, 0.0);
    }
}

bool Serializer::readValue(std::istream& in, Value& value, VM& vm) {
    int tag = in.get();
    if (in.fail()) return false;

    switch ((uint8_t)tag) {
        case CVMC_TAG_NUMBER: {
            value = Value(readDouble(in));
            break;
        }
        case CVMC_TAG_BOOLEAN: {
            int b = in.get();
            value = Value((bool)(b != 0));
            break;
        }
        case CVMC_TAG_STRING: {
            value = Value(readString(in));
            break;
        }
        case CVMC_TAG_FUNCTION: {
            auto fn = vm.allocateObject<ObjFunction>();
            fn->arity = (int)readUint32(in);
            fn->upvalueCount = (int)readUint32(in);
            fn->name = readString(in);
            if (!readChunk(in, *fn->chunk, vm)) return false;
            value = Value(fn);
            break;
        }
        default:
            std::cerr << "[CVMC] Error: Unknown value tag 0x" << std::hex << (int)tag << std::dec << "\n";
            return false;
    }
    return true;
}

// ─────────────────────────────────────────────
//  Chunk serialization
// ─────────────────────────────────────────────

void Serializer::writeChunk(std::ostream& out, const Chunk& chunk) {
    // Code bytes
    writeUint32(out, (uint32_t)chunk.code.size());
    out.write((const char*)chunk.code.data(), chunk.code.size());

    // Line numbers (same count as code)
    writeUint32(out, (uint32_t)chunk.lines.size());
    for (int line : chunk.lines) {
        writeInt32(out, line);
    }

    // Interned strings pool
    writeUint32(out, (uint32_t)chunk.strings.size());
    for (const auto& s : chunk.strings) {
        writeString(out, s);
    }

    // Constants pool
    writeUint32(out, (uint32_t)chunk.constants.values.size());
    for (const auto& val : chunk.constants.values) {
        writeValue(out, val);
    }
}

bool Serializer::readChunk(std::istream& in, Chunk& chunk, VM& vm) {
    // Code bytes
    uint32_t codeCount = readUint32(in);
    chunk.code.resize(codeCount);
    in.read((char*)chunk.code.data(), codeCount);
    if (in.fail() && codeCount > 0) return false;

    // Line numbers
    uint32_t linesCount = readUint32(in);
    chunk.lines.resize(linesCount);
    for (uint32_t i = 0; i < linesCount; i++) {
        chunk.lines[i] = readInt32(in);
    }

    // Interned strings
    uint32_t strCount = readUint32(in);
    chunk.strings.resize(strCount);
    for (uint32_t i = 0; i < strCount; i++) {
        chunk.strings[i] = readString(in);
    }

    // Constants
    uint32_t constCount = readUint32(in);
    for (uint32_t i = 0; i < constCount; i++) {
        Value v;
        if (!readValue(in, v, vm)) return false;
        chunk.constants.writeValue(v);
    }

    return !in.fail();
}

// ─────────────────────────────────────────────
//  Public API
// ─────────────────────────────────────────────

bool Serializer::writeFile(const std::string& path, const Chunk& chunk) {
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) {
        std::cerr << "[CVMC] Error: Cannot open file for writing: " << path << "\n";
        return false;
    }

    // Write header: magic + version
    out.write(CVMC_MAGIC, 4);
    out.put(CVMC_VERSION_MAJOR);
    out.put(CVMC_VERSION_MINOR);

    // Write the top-level script chunk
    writeChunk(out, chunk);

    return out.good();
}

bool Serializer::readFile(const std::string& path, Chunk& chunk, VM& vm) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        std::cerr << "[CVMC] Error: Cannot open file for reading: " << path << "\n";
        return false;
    }

    // Validate header magic
    char magic[4];
    in.read(magic, 4);
    if (in.fail() || std::memcmp(magic, CVMC_MAGIC, 4) != 0) {
        std::cerr << "[CVMC] Error: Invalid .cvmc file (bad magic bytes). Did you mean to run a .cvm source file?\n";
        return false;
    }

    // Validate version
    int major = in.get();
    int minor = in.get();
    if (major != CVMC_VERSION_MAJOR) {
        std::cerr << "[CVMC] Error: Incompatible bytecode version " << major << "." << minor
                  << " (expected " << (int)CVMC_VERSION_MAJOR << "." << (int)CVMC_VERSION_MINOR << ")\n";
        return false;
    }

    // Read the top-level script chunk
    return readChunk(in, chunk, vm);
}

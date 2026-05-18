#ifndef cvm_serializer_h
#define cvm_serializer_h

#include "chunk.h"
#include "value.h"
#include <string>
#include <cstdint>

// CVM++ Bytecode Serializer & Deserializer
// File extension: .cvmc (CVM++ Compiled)
//
// Binary Format:
//   Header:   4 bytes magic ("CVMC") + 2 bytes version (major, minor)
//   Chunk:    Recursive chunk binary layout (see below)
//
// Chunk Layout:
//   [4] code_count        - number of bytecode instructions (uint32_t)
//   [code_count] bytes    - raw opcodes (uint8_t each)
//   [4] lines_count       - number of line entries (uint32_t, same as code_count)
//   [lines_count * 4]     - line numbers (int32_t each)
//   [4] strings_count     - number of interned strings (uint32_t)
//   for each string:
//       [4] length (uint32_t)
//       [length] bytes (utf-8 chars)
//   [4] constants_count   - number of constant values (uint32_t)
//   for each constant:
//       [1] tag byte:
//           0x01 = NUMBER    -> [8] double (little-endian)
//           0x02 = BOOLEAN   -> [1] uint8_t (0=false, 1=true)
//           0x03 = STRING    -> [4] uint32_t length + [length] bytes
//           0x04 = FUNCTION  -> recursive nested Chunk serialization:
//               [4] arity (uint32_t)
//               [4] upvalueCount (uint32_t)
//               [4] name_len (uint32_t) + [name_len] bytes
//               then full Chunk layout recursively

static const char CVMC_MAGIC[4] = {'C', 'V', 'M', 'C'};
static const uint8_t CVMC_VERSION_MAJOR = 1;
static const uint8_t CVMC_VERSION_MINOR = 0;

static const uint8_t CVMC_TAG_NUMBER   = 0x01;
static const uint8_t CVMC_TAG_BOOLEAN  = 0x02;
static const uint8_t CVMC_TAG_STRING   = 0x03;
static const uint8_t CVMC_TAG_FUNCTION = 0x04;

class VM;

class Serializer {
public:
    // Serialize a compiled Chunk to a .cvmc binary file
    static bool writeFile(const std::string& path, const Chunk& chunk);

    // Deserialize a .cvmc binary file into a Chunk. Returns true on success.
    static bool readFile(const std::string& path, Chunk& chunk, VM& vm);

private:
    static void writeChunk(std::ostream& out, const Chunk& chunk);
    static void writeValue(std::ostream& out, const Value& value);
    static void writeUint32(std::ostream& out, uint32_t v);
    static void writeInt32(std::ostream& out, int32_t v);
    static void writeDouble(std::ostream& out, double d);
    static void writeString(std::ostream& out, const std::string& s);

    static bool readChunk(std::istream& in, Chunk& chunk, VM& vm);
    static bool readValue(std::istream& in, Value& value, VM& vm);
    static uint32_t readUint32(std::istream& in);
    static int32_t readInt32(std::istream& in);
    static double readDouble(std::istream& in);
    static std::string readString(std::istream& in);
};

#endif

#include "common.h"
#include "chunk.h"
#include "lexer.h"
#include "parser.h"
#include "compiler.h"
#include "vm.h"
#include "serializer.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// ── Global script args (set before VM starts) ────────────────────────────────
static std::vector<std::string> g_scriptArgs;

// ── Compile source text → Chunk ───────────────────────────────────────────────
static bool compileSource(const std::string& source, Chunk& chunk, VM& vm) {
    Lexer lexer(source.c_str());
    Parser parser(lexer);
    std::vector<std::unique_ptr<Stmt>> statements = parser.parse();
    if (parser.hasError()) return false;
    Compiler compiler(vm);
    compiler.compile(statements, chunk);
    return true;
}

static bool runChunk(Chunk& chunk, VM& vm) {
    InterpretResult result = vm.interpret(&chunk);
    chunk.freeChunk();
    return result == INTERPRET_OK;
}

// Register args() and exit() natives using the script args
static void registerRuntimeNatives(VM& vm) {
    // args() → returns script arguments as an ObjArray
    vm.defineNative("args", [](int, Value*) -> Value {
        // This is a bit tricky: we cannot easily capture `vm` in a function pointer.
        // But wait! NativeFn is a function pointer. A lambda without captures decays to a function pointer.
        // If we capture `vm`, it becomes a closure, which cannot decay to NativeFn!
        // We must fetch the VM from somewhere, or allocate the array manually and let it leak, or add VM* to NativeFn.
        // Since args() is called once, leaking an ObjArray is fine, or we can use a global VM pointer.
        // Wait, CVM's NativeFn signature is `Value (*)(int, Value*)`.
        // Let's just create it with `new ObjArray()` and let it leak if we don't have VM context, OR we can add VM* to NativeFn.
        // We will just `new ObjArray()` here. It won't be swept because it's not in the VM's `objects` list.
        // Wait, if it's returned to the VM, it will be pushed to the stack. If a GC happens, the VM won't trace it because it's not in `objects`? No, if it's on the stack, it WILL trace it, but then it will try to set `isMarked`. That's fine. But it won't be sweeped! So it will just leak, which is safe for args().
        // Wait, actually, let's just make it properly.
        auto arr = new ObjArray();
        for (const auto& s : g_scriptArgs) {
            arr->elements.push_back(Value(s));
        }
        return Value(arr);
    });
    // exit(code) → exits the process
    vm.defineNative("exit", [](int argc, Value* args) -> Value {
        int code = (argc > 0 && args[0].isNumber()) ? (int)args[0].asNumber() : 0;
        std::exit(code);
        return Value(0.0);
    });
}

// ── Phase 29: Bytecode Disassembler ──────────────────────────────────────────
static const char* opName(uint8_t op) {
    switch ((OpCode)op) {
        case OP_CONSTANT:       return "OP_CONSTANT";
        case OP_DEFINE_GLOBAL:  return "OP_DEFINE_GLOBAL";
        case OP_GET_GLOBAL:     return "OP_GET_GLOBAL";
        case OP_SET_GLOBAL:     return "OP_SET_GLOBAL";
        case OP_GET_LOCAL:      return "OP_GET_LOCAL";
        case OP_SET_LOCAL:      return "OP_SET_LOCAL";
        case OP_GET_UPVALUE:    return "OP_GET_UPVALUE";
        case OP_SET_UPVALUE:    return "OP_SET_UPVALUE";
        case OP_ADD:            return "OP_ADD";
        case OP_SUBTRACT:       return "OP_SUBTRACT";
        case OP_MULTIPLY:       return "OP_MULTIPLY";
        case OP_DIVIDE:         return "OP_DIVIDE";
        case OP_NEGATE:         return "OP_NEGATE";
        case OP_NOT:            return "OP_NOT";
        case OP_EQUAL:          return "OP_EQUAL";
        case OP_GREATER:        return "OP_GREATER";
        case OP_LESS:           return "OP_LESS";
        case OP_PRINT:          return "OP_PRINT";
        case OP_POP:            return "OP_POP";
        case OP_JUMP:           return "OP_JUMP";
        case OP_JUMP_IF_FALSE:  return "OP_JUMP_IF_FALSE";
        case OP_LOOP:           return "OP_LOOP";
        case OP_CALL:           return "OP_CALL";
        case OP_RETURN:         return "OP_RETURN";
        case OP_CLOSURE:        return "OP_CLOSURE";
        case OP_CLOSE_UPVALUE:  return "OP_CLOSE_UPVALUE";
        case OP_CLASS:          return "OP_CLASS";
        case OP_GET_PROPERTY:   return "OP_GET_PROPERTY";
        case OP_SET_PROPERTY:   return "OP_SET_PROPERTY";
        case OP_METHOD:         return "OP_METHOD";
        case OP_INHERIT:        return "OP_INHERIT";
        case OP_GET_SUPER:      return "OP_GET_SUPER";
        case OP_BUILD_ARRAY:    return "OP_BUILD_ARRAY";
        case OP_INDEX_GET:      return "OP_INDEX_GET";
        case OP_INDEX_SET:      return "OP_INDEX_SET";
        case OP_BUILD_MAP:      return "OP_BUILD_MAP";
        case OP_TRY:            return "OP_TRY";
        case OP_END_TRY:        return "OP_END_TRY";
        case OP_THROW:          return "OP_THROW";
        case OP_IMPORT:         return "OP_IMPORT";
        case OP_COLLECTION_SIZE:return "OP_COLLECTION_SIZE";
        case OP_ITER_GET:       return "OP_ITER_GET";
        case OP_JUMP_BREAK:     return "OP_JUMP_BREAK";
        case OP_JUMP_CONTINUE:  return "OP_JUMP_CONTINUE";
        default:                return "OP_???";
    }
}

static void disassembleChunk(const Chunk& chunk, const std::string& name, int indent = 0) {
    std::string pad(indent * 2, ' ');
    std::cout << pad << "== " << name << " ==\n";
    const auto& code = chunk.code;
    const auto& lines = chunk.lines;
    size_t i = 0;
    while (i < code.size()) {
        uint8_t op = code[i];
        int line = (i < lines.size()) ? lines[i] : -1;
        printf("%s%04zu  [line %3d]  %-22s", pad.c_str(), i, line, opName(op));
        i++;
        // Print operands based on opcode
        switch ((OpCode)(op)) {
            case OP_CONSTANT: {
                uint8_t idx = code[i++];
                std::ostringstream ss; ss << chunk.constants.values[idx];
                printf("  #%-3u  ; %s", idx, ss.str().c_str());
                break;
            }
            case OP_DEFINE_GLOBAL: case OP_GET_GLOBAL: case OP_SET_GLOBAL:
            case OP_CLASS: case OP_METHOD: case OP_GET_PROPERTY:
            case OP_SET_PROPERTY: case OP_GET_SUPER: case OP_IMPORT: {
                uint8_t idx = code[i++];
                if (idx < chunk.strings.size())
                    printf("  \"%s\"", chunk.strings[idx].c_str());
                else printf("  #%u", idx);
                break;
            }
            case OP_GET_LOCAL: case OP_SET_LOCAL: case OP_GET_UPVALUE:
            case OP_SET_UPVALUE: case OP_CALL: case OP_BUILD_ARRAY:
            case OP_BUILD_MAP: case OP_COLLECTION_SIZE: {
                printf("  slot=%u", code[i++]);
                break;
            }
            case OP_ITER_GET: {
                uint8_t c = code[i++]; uint8_t idx = code[i++];
                printf("  coll=%u idx=%u", c, idx);
                break;
            }
            case OP_JUMP: case OP_JUMP_IF_FALSE: {
                uint16_t offset = (uint16_t)((code[i] << 8) | code[i+1]);
                i += 2;
                printf("  -> %zu", i + offset);
                break;
            }
            case OP_LOOP: {
                uint16_t offset = (uint16_t)((code[i] << 8) | code[i+1]);
                i += 2;
                printf("  -> %zu", i - offset);
                break;
            }
            case OP_CLOSURE: {
                uint8_t idx = code[i++];
                auto fn = chunk.constants.values[idx].asFunction();
                printf("  #%-3u  ; <fn %s>", idx, fn->name.c_str());
                int uvCount = fn->upvalueCount;
                for (int u = 0; u < uvCount; u++) {
                    uint8_t isLocal = code[i++];
                    uint8_t uvIdx   = code[i++];
                    printf("\n%s             upvalue[%d]: %s %d",
                        pad.c_str(), u, isLocal ? "local" : "upvalue", uvIdx);
                }
                // Recurse to disassemble nested function
                printf("\n");
                disassembleChunk(*fn->chunk, "<fn " + fn->name + ">", indent + 1);
                printf("\n"); continue;
            }
            case OP_TRY: {
                uint16_t offset = (uint16_t)((code[i] << 8) | code[i+1]);
                i += 2;
                uint8_t slot = code[i++];
                printf("  catch@%zu slot=%u", i + offset, slot);
                break;
            }
            default: break;
        }
        printf("\n");
    }
}

static void disasmFile(const char* path) {
    Chunk chunk;
    std::string spath(path);
    if (spath.size() >= 5 && spath.substr(spath.size()-5) == ".cvmc") {
        VM dummyVm;
        if (!Serializer::readFile(path, chunk, dummyVm)) {
            std::cerr << "Failed to load: " << path << "\n"; return;
        }
    } else {
        std::ifstream f(path);
        if (!f) { std::cerr << "Cannot open: " << path << "\n"; return; }
        std::stringstream ss; ss << f.rdbuf();
        VM dummyVm;
        if (!compileSource(ss.str(), chunk, dummyVm)) {
            std::cerr << "Compilation failed.\n"; return;
        }
    }
    disassembleChunk(chunk, path);
}

// ── Run source / compiled file ────────────────────────────────────────────────
static void runFile(const char* path) {
    std::ifstream file(path);
    if (!file.is_open()) { std::cerr << "Could not open file \"" << path << "\".\n"; exit(74); }
    std::stringstream buffer; buffer << file.rdbuf();

    std::string dir;
    std::string spath(path);
    auto slash = spath.find_last_of("/\\");
    if (slash != std::string::npos) dir = spath.substr(0, slash + 1);

    VM vm;
    vm.currentDir = dir;
    registerRuntimeNatives(vm);

    Chunk chunk;
    if (!compileSource(buffer.str(), chunk, vm)) exit(65);

    if (!runChunk(chunk, vm)) exit(65);
}

static void compileFile(const char* sourcePath) {
    std::ifstream file(sourcePath);
    if (!file.is_open()) { std::cerr << "Could not open source file \"" << sourcePath << "\".\n"; exit(74); }
    std::stringstream buffer; buffer << file.rdbuf();

    VM dummyVm; // for allocations during compilation
    Chunk chunk;
    if (!compileSource(buffer.str(), chunk, dummyVm)) {
        std::cerr << "[CVMC] Compilation failed. No output written.\n"; exit(65);
    }

    std::string outPath(sourcePath);
    std::string ext(".cvm");
    auto pos = outPath.rfind(ext);
    if (pos != std::string::npos && pos == outPath.size() - ext.size())
        outPath = outPath.substr(0, pos) + ".cvmc";
    else outPath += ".cvmc";

    if (!Serializer::writeFile(outPath, chunk)) {
        std::cerr << "[CVMC] Failed to write bytecode to \"" << outPath << "\".\n"; exit(74);
    }
    std::cout << "[CVMC] Compiled \"" << sourcePath << "\" → \"" << outPath << "\"\n";
    chunk.freeChunk();
}

static void runCompiledFile(const char* path) {
    std::string dir;
    std::string spath(path);
    auto slash = spath.find_last_of("/\\");
    if (slash != std::string::npos) dir = spath.substr(0, slash + 1);

    VM vm;
    vm.currentDir = dir;
    registerRuntimeNatives(vm);

    Chunk chunk;
    if (!Serializer::readFile(path, chunk, vm)) {
        std::cerr << "[CVMC] Failed to load bytecode from \"" << path << "\".\n"; exit(74);
    }
    if (!runChunk(chunk, vm)) exit(65);
}

void repl() {
    VM vm;
    registerRuntimeNatives(vm);
    std::string line;
    std::cout << "CVM++ REPL v1.0 (type 'exit' to quit)\n";
    for (;;) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) { std::cout << std::endl; break; }
        if (line == "exit") break;
        Chunk chunk;
        if (!compileSource(line, chunk, vm)) continue;
        InterpretResult result = vm.interpret(&chunk);
        chunk.freeChunk();
        (void)result;
    }
}

static bool hasExtension(const std::string& path, const std::string& ext) {
    if (path.size() < ext.size()) return false;
    std::string suffix = path.substr(path.size() - ext.size());
    for (char& c : suffix) c = (char)tolower(c);
    return suffix == ext;
}

int main(int argc, const char* argv[]) {
    try {
        if (argc == 1) {
            repl();
        } else if (argc == 2) {
            std::string path(argv[1]);
            if (hasExtension(path, ".cvmc")) {
                runCompiledFile(argv[1]);
            } else {
                runFile(argv[1]);
            }
        } else if (argc >= 3 && std::string(argv[1]) == "--compile") {
            compileFile(argv[2]);
        } else if (argc >= 3 && std::string(argv[1]) == "--disasm") {
            disasmFile(argv[2]);
        } else {
            // Treat argv[2..] as script arguments
            std::string path(argv[1]);
            for (int i = 2; i < argc; i++) g_scriptArgs.push_back(argv[i]);
            if (hasExtension(path, ".cvmc")) {
                runCompiledFile(argv[1]);
            } else {
                runFile(argv[1]);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Caught exception in main: " << e.what() << "\n";
    }
    return 0;
}

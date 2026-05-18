#ifndef cvm_vm_h
#define cvm_vm_h

#include "chunk.h"
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>

enum InterpretResult {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
};

struct CallFrame {
    ObjClosure* closure;
    uint8_t* ip;
    int slots;
};

struct ExceptionHandler {
    uint8_t* catchIp;
    int stackDepth;
    size_t frameDepth;
};

class VM {
public:
    static thread_local VM* currentVM;

    VM();
    ~VM();
    
    InterpretResult interpret(Chunk* chunk);

    std::string currentDir; // directory of the script being run (public for main.cpp)

    void defineNative(const std::string& name, NativeFn function);
    void runtimeError(const char* format, ...);
    
    bool hasException;
    Value exceptionValue;
    void throwException(Value value);

private:
    InterpretResult run();
    
    void push(Value value);
    Value pop();
    Value peek(int distance);
    ObjUpvalue* captureUpvalue(int stackIndex);
    void closeUpvalues(int lastSlotIndex);
    void printStackTrace();

    bool importModule(const std::string& path);

    // GC infrastructure
    Obj* objects;
    std::vector<Obj*> grayStack;
    size_t bytesAllocated;
    size_t nextGC;

    void collectGarbage();
    void markValue(Value value);
    void markObject(Obj* object);
    void markRoots();
    void traceReferences();
    void sweep();

    std::vector<CallFrame> frames;
    ObjUpvalue* openUpvalues;
    
    std::vector<Value> stack;
    std::unordered_map<std::string, Value> globals;
    std::vector<ExceptionHandler> exceptionHandlers;
    std::unordered_set<std::string> importedModules;

public:
    template <typename T, typename... Args>
    T* allocateObject(Args&&... args) {
        // Since we aren't tracking precise C++ sizeof correctly in this simple GC, 
        // we approximate size or just count object instances. Let's count instances * ~64 bytes.
        bytesAllocated += sizeof(T);
        if (bytesAllocated > nextGC) {
            collectGarbage();
        }

        T* object = new T(std::forward<Args>(args)...);
        object->next = objects;
        objects = object;
        return object;
    }
};

#endif

# CVM++ 🚀

**CVM++** is a high-performance, fully-featured, dynamic programming language compiler and virtual machine runtime built in native **C++17** from the ground up.

Featuring a custom **Mark-and-Sweep Garbage Collector**, **binary bytecode serialization (`.cvmc`)**, **try-catch exception handling with stack-unwinding backtraces**, and standard libraries for **JSON Parsing/Serialization** and **TCP Sockets Networking**, CVM++ represents the absolute pinnacle of custom language vm engines.

---

## 🌟 Key Features

*   **Recursive-Descent AST Compiler:** Translates raw source scripts directly into a dense, optimized bytecode chunk format.
*   **Mark-and-Sweep GC:** A fully custom manual allocation-tracked garbage collector with precise stack, global, and call frame root scanning to eliminate memory leaks and circular reference issues.
*   **Standard Library API:** 
    *   **TCP Sockets:** Real-time socket creation, binding, listening, connections, transmission, and shutdown (`ws2_32` linked on Windows).
    *   **Relaxed JSON Engine:** Parses single/double-quoted JSON payloads dynamically and serializes complex data structures with recursive cycle-detection exceptions.
*   **Modular Import System:** Imports reusable `.cvm` modules dynamically, supporting relative directories and circular import caches.
*   **Exception Unwinding:** Complete VM unwinding of CallFrames, capturing of closure upvalues, and mapping of C++ native errors directly to CVM++ script `catch` handlers.

---

## 🛠️ Complete 31-Phase Roadmap

1.  **Phase 1–5:** Setup, AST node definitions, recursive-descent compiler, and stack-based virtual machine.
2.  **Phase 6–7:** CLI runner, interactive REPL, and suite runner.
3.  **Phases 8–10:** Blocks, control flow loops, jump bytecode instructions, lexical locals/globals, and native standard functions.
4.  **Phase 11:** Heap-allocated `ObjArray` arrays and indexing (`[index]` get/set).
5.  **Phase 12:** Lexical closures and upvalue tracking.
6.  **Phases 13–15:** Full OOP (Classes, instances, field getters/setters, bound methods, single inheritance, and `super` resolution).
7.  **Phase 16:** Expanded helper methods on strings (e.g., `slice`, `indexOf`, `substring`, `contains`) and core File I/O (`read_file`, `write_file`).
8.  **Phase 17:** Heap-allocated `ObjMap` dictionaries with bracket indexing (`map[key]`) and methods like `.keys()`, `.values()`, `.has()`, `.remove()`.
9.  **Phase 18:** First-class CVM++ `try-catch-throw` exception handling with accurate lexical-line backtraces.
10. **Phase 19:** Binary bytecode compilation, writing/reading `.cvmc` serialized bytecode chunks.
11. **Phase 20:** Module `import "path";` system with duplicate import prevention cache.
12. **Phase 21:** For-each iterator loops (`for (x in collection)`) backing maps and arrays.
13. **Phase 22:** Loop control with `break` and `continue`.
14. **Phase 23:** Inline ternary operator (`a ? b : c`).
15. **Phase 24:** Variadic functions accepting dynamic number of arguments.
16. **Phase 25:** Native `typeof()` reflection operator.
17. **Phase 26:** Universal standard library math utilities (`floor`, `ceil`, `round`, `min`, `max`, etc.).
18. **Phase 27:** Built-in string methods.
19. **Phase 28:** Rich string interpolation (`"Hello ${name}"`).
20. **Phase 29:** Bytecode disassembler for full VM introspection.
21. **Phase 30 (Core VM Redesign):** Replaced `std::shared_ptr` entirely with a custom **Mark-and-Sweep Garbage Collector**.
22. **Phase 31 (Networking & JSON):** Relaxed JSON Engine, TCP Sockets Networking API, and C++ to CVM++ Exception Mapping.

---

## 🚀 Getting Started

### Prerequisites
*   A C++17 compatible compiler (e.g., GCC/MinGW, Clang, or MSVC)
*   CMake (optional)

### 1. Build the Production Binary
Compile with maximum optimizations (`-O3`) to get the absolute best performance out of the virtual machine:

```powershell
g++ -O3 -std=c++17 -Iinclude src/main.cpp src/chunk.cpp src/value.cpp src/lexer.cpp src/ast.cpp src/parser.cpp src/compiler.cpp src/vm.cpp src/serializer.cpp -o cvm++.exe -lws2_32
```

### 2. Run Scripts
Run any CVM++ script directly from the command line:
```powershell
.\cvm++.exe script.cvm
```

To run interactive REPL:
```powershell
.\cvm++.exe
```

### 3. Compile Scripts to Bytecode
Compile a source file into compact, high-speed binary bytecode (`.cvmc`):
```powershell
.\cvm++.exe --compile script.cvm
# Produces script.cvmc
```

Run compiled bytecode directly:
```powershell
.\cvm++.exe script.cvmc
```

To disassemble bytecode for inspection:
```powershell
.\cvm++.exe --disasm script.cvmc
```

---

## 🧪 Verification & Testing
To run the automated verification suite across all standard libraries and compiler features:
```powershell
# Run the PowerShell verifier script
powershell -ExecutionPolicy Bypass -File verify_all.ps1
```

---

## 📄 License
This project is open source and available under the [MIT License](LICENSE).

#ifndef cvm_compiler_h
#define cvm_compiler_h

#include "ast.h"
#include "chunk.h"
#include <map>
#include <string>

class VM;

class Compiler : public Visitor {
public:
    Compiler(VM& vm);
    void compile(const std::vector<std::unique_ptr<Stmt>>& statements, Chunk& chunk);

    void visitBinaryExpr(BinaryExpr* expr) override;
    void visitLogicalExpr(LogicalExpr* expr) override;
    void visitLiteralExpr(LiteralExpr* expr) override;
    void visitVariableExpr(VariableExpr* expr) override;
    void visitAssignExpr(AssignExpr* expr) override;
    void visitUnaryExpr(UnaryExpr* expr) override;
    void visitCallExpr(CallExpr* expr) override;
    void visitArrayExpr(ArrayExpr* expr) override;
    void visitMapExpr(MapExpr* expr) override;
    void visitIndexExpr(IndexExpr* expr) override;
    void visitIndexAssignExpr(IndexAssignExpr* expr) override;
    
    void visitExpressionStmt(ExpressionStmt* stmt) override;
    void visitPrintStmt(PrintStmt* stmt) override;
    void visitVarStmt(VarStmt* stmt) override;
    void visitBlockStmt(BlockStmt* stmt) override;
    void visitIfStmt(IfStmt* stmt) override;
    void visitWhileStmt(WhileStmt* stmt) override;
    void visitFunctionStmt(FunctionStmt* stmt) override;
    void visitReturnStmt(ReturnStmt* stmt) override;
    void visitClassStmt(ClassStmt* stmt) override;
    void visitTryStmt(TryStmt* stmt) override;
    void visitThrowStmt(ThrowStmt* stmt) override;
    void visitImportStmt(ImportStmt* stmt) override;
    void visitForEachStmt(ForEachStmt* stmt) override;
    void visitBreakStmt(BreakStmt* stmt) override;
    void visitContinueStmt(ContinueStmt* stmt) override;
    void visitTernaryExpr(TernaryExpr* expr) override;
    void visitGetExpr(GetExpr* expr) override;
    void visitSetExpr(SetExpr* expr) override;
    void visitThisExpr(ThisExpr* expr) override;
    void visitSuperExpr(SuperExpr* expr) override;

private:
    struct Local {
        Token name;
        int depth;
        bool isCaptured;
    };

    struct Upvalue {
        uint8_t index;
        bool isLocal;
    };

    struct CompilerState {
        ObjFunction* function;
        Chunk* chunk;
        std::vector<Local> locals;
        std::vector<Upvalue> upvalues;
        int scopeDepth;
    };

    std::vector<CompilerState> states;
    
    ObjFunction* currentFunction;
    Chunk* currentChunk;
    std::vector<Local> locals;
    std::vector<Upvalue> upvalues;
    int scopeDepth;
    struct LoopContext {
        int loopStart;              // for continue
        std::vector<int> breakJumps; // to be patched to exit
        int localDepthAtEntry;       // locals.size() at loop entry, for pop-before-jump
    };

    std::vector<LoopContext> loopStack;
    int currentLine;
    
    void pushState(std::string name, int arity, bool isMethod = false);
    ObjFunction* popState();
    
    void beginScope();
    void endScope();
    void addLocal(Token name);
    int resolveLocal(Token name);
    int resolveLocalInState(const std::vector<Local>& stateLocals, Token name);
    int resolveUpvalue(int stateIndex, Token name);
    int addUpvalue(int stateIndex, int index, bool isLocal);

    void emitByte(uint8_t byte);
    void emitBytes(uint8_t byte1, uint8_t byte2);
    void emitConstant(Value value);
    
    int emitJump(uint8_t instruction);
    void patchJump(int offset);
    void emitLoop(int loopStart);

private:
    VM& vm;
};

#endif

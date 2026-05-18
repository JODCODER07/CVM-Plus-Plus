#ifndef cvm_ast_h
#define cvm_ast_h

#include "lexer.h"
#include "value.h"
#include <vector>
#include <memory>
#include <string>

// Forward declarations
class Visitor;
class MapExpr;

class Expr {
public:
    virtual ~Expr() = default;
    virtual void accept(Visitor& visitor) = 0;
};

class Stmt {
public:
    virtual ~Stmt() = default;
    virtual void accept(Visitor& visitor) = 0;
};

class BinaryExpr : public Expr {
public:
    BinaryExpr(std::unique_ptr<Expr> left, Token op, std::unique_ptr<Expr> right)
        : left(std::move(left)), op(op), right(std::move(right)) {}
    void accept(Visitor& visitor) override;
    std::unique_ptr<Expr> left;
    Token op;
    std::unique_ptr<Expr> right;
};

class LogicalExpr : public Expr {
public:
    LogicalExpr(std::unique_ptr<Expr> left, Token op, std::unique_ptr<Expr> right)
        : left(std::move(left)), op(op), right(std::move(right)) {}
    void accept(Visitor& visitor) override;
    std::unique_ptr<Expr> left;
    Token op;
    std::unique_ptr<Expr> right;
};

class LiteralExpr : public Expr {
public:
    LiteralExpr(Value value) : value(value) {}
    void accept(Visitor& visitor) override;
    Value value;
};

class VariableExpr : public Expr {
public:
    VariableExpr(Token name) : name(name) {}
    void accept(Visitor& visitor) override;
    Token name;
};

class AssignExpr : public Expr {
public:
    AssignExpr(Token name, std::unique_ptr<Expr> value)
        : name(name), value(std::move(value)) {}
    void accept(Visitor& visitor) override;
    Token name;
    std::unique_ptr<Expr> value;
};

class UnaryExpr : public Expr {
public:
    UnaryExpr(Token op, std::unique_ptr<Expr> right)
        : op(op), right(std::move(right)) {}
    void accept(Visitor& visitor) override;
    Token op;
    std::unique_ptr<Expr> right;
};

class CallExpr : public Expr {
public:
    CallExpr(std::unique_ptr<Expr> callee, Token paren, std::vector<std::unique_ptr<Expr>> arguments)
        : callee(std::move(callee)), paren(paren), arguments(std::move(arguments)) {}
    void accept(Visitor& visitor) override;
    std::unique_ptr<Expr> callee;
    Token paren;
    std::vector<std::unique_ptr<Expr>> arguments;
};

class ExpressionStmt : public Stmt {
public:
    ExpressionStmt(std::unique_ptr<Expr> expression)
        : expression(std::move(expression)) {}
    void accept(Visitor& visitor) override;
    std::unique_ptr<Expr> expression;
};

class FunctionStmt : public Stmt {
public:
    FunctionStmt(Token name, std::vector<Token> params, std::vector<std::unique_ptr<Stmt>> body, bool isVariadic = false)
        : name(name), params(std::move(params)), body(std::move(body)), isVariadic(isVariadic) {}
    void accept(Visitor& visitor) override;
    Token name;
    std::vector<Token> params;
    std::vector<std::unique_ptr<Stmt>> body;
    bool isVariadic;
};

class PrintStmt : public Stmt {
public:
    PrintStmt(std::unique_ptr<Expr> expression)
        : expression(std::move(expression)) {}
    void accept(Visitor& visitor) override;
    std::unique_ptr<Expr> expression;
};

class VarStmt : public Stmt {
public:
    VarStmt(Token name, std::unique_ptr<Expr> initializer)
        : name(name), initializer(std::move(initializer)) {}
    void accept(Visitor& visitor) override;
    Token name;
    std::unique_ptr<Expr> initializer;
};

class BlockStmt : public Stmt {
public:
    BlockStmt(std::vector<std::unique_ptr<Stmt>> statements)
        : statements(std::move(statements)) {}
    void accept(Visitor& visitor) override;
    std::vector<std::unique_ptr<Stmt>> statements;
};

class IfStmt : public Stmt {
public:
    IfStmt(std::unique_ptr<Expr> condition, std::unique_ptr<Stmt> thenBranch, std::unique_ptr<Stmt> elseBranch)
        : condition(std::move(condition)), thenBranch(std::move(thenBranch)), elseBranch(std::move(elseBranch)) {}
    void accept(Visitor& visitor) override;
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Stmt> thenBranch;
    std::unique_ptr<Stmt> elseBranch;
};

class ReturnStmt : public Stmt {
public:
    ReturnStmt(Token keyword, std::unique_ptr<Expr> value)
        : keyword(keyword), value(std::move(value)) {}
    void accept(Visitor& visitor) override;
    Token keyword;
    std::unique_ptr<Expr> value;
};

class WhileStmt : public Stmt {
public:
    WhileStmt(std::unique_ptr<Expr> condition, std::unique_ptr<Stmt> body)
        : condition(std::move(condition)), body(std::move(body)) {}
    void accept(Visitor& visitor) override;
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Stmt> body;
};

class TernaryExpr : public Expr {
public:
    TernaryExpr(std::unique_ptr<Expr> condition, std::unique_ptr<Expr> thenExpr, std::unique_ptr<Expr> elseExpr)
        : condition(std::move(condition)), thenExpr(std::move(thenExpr)), elseExpr(std::move(elseExpr)) {}
    void accept(Visitor& visitor) override;
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Expr> thenExpr;
    std::unique_ptr<Expr> elseExpr;
};

class ArrayExpr : public Expr {
public:
    ArrayExpr(std::vector<std::unique_ptr<Expr>> elements)
        : elements(std::move(elements)) {}
    void accept(Visitor& visitor) override;
    std::vector<std::unique_ptr<Expr>> elements;
};

class MapExpr : public Expr {
public:
    MapExpr(std::vector<std::pair<std::unique_ptr<Expr>, std::unique_ptr<Expr>>> entries)
        : entries(std::move(entries)) {}
    void accept(Visitor& visitor) override;
    std::vector<std::pair<std::unique_ptr<Expr>, std::unique_ptr<Expr>>> entries;
};

class IndexExpr : public Expr {
public:
    IndexExpr(std::unique_ptr<Expr> object, Token bracket, std::unique_ptr<Expr> index)
        : object(std::move(object)), bracket(bracket), index(std::move(index)) {}
    void accept(Visitor& visitor) override;
    std::unique_ptr<Expr> object;
    Token bracket;
    std::unique_ptr<Expr> index;
};

class IndexAssignExpr : public Expr {
public:
    IndexAssignExpr(std::unique_ptr<Expr> object, Token bracket, std::unique_ptr<Expr> index, std::unique_ptr<Expr> value)
        : object(std::move(object)), bracket(bracket), index(std::move(index)), value(std::move(value)) {}
    void accept(Visitor& visitor) override;
    std::unique_ptr<Expr> object;
    Token bracket;
    std::unique_ptr<Expr> index;
    std::unique_ptr<Expr> value;
};

class GetExpr : public Expr {
public:
    GetExpr(std::unique_ptr<Expr> object, Token name)
        : object(std::move(object)), name(name) {}
    void accept(Visitor& visitor) override;
    std::unique_ptr<Expr> object;
    Token name;
};

class SetExpr : public Expr {
public:
    SetExpr(std::unique_ptr<Expr> object, Token name, std::unique_ptr<Expr> value)
        : object(std::move(object)), name(name), value(std::move(value)) {}
    void accept(Visitor& visitor) override;
    std::unique_ptr<Expr> object;
    Token name;
    std::unique_ptr<Expr> value;
};

class ThisExpr : public Expr {
public:
    ThisExpr(Token keyword) : keyword(keyword) {}
    void accept(Visitor& visitor) override;
    Token keyword;
};

class SuperExpr : public Expr {
public:
    SuperExpr(Token keyword, Token method)
        : keyword(keyword), method(method) {}
    void accept(Visitor& visitor) override;
    Token keyword;
    Token method;
};

class ClassStmt : public Stmt {
public:
    ClassStmt(Token name, std::unique_ptr<VariableExpr> superclass, std::vector<std::unique_ptr<FunctionStmt>> methods)
        : name(name), superclass(std::move(superclass)), methods(std::move(methods)) {}
    void accept(Visitor& visitor) override;
    Token name;
    std::unique_ptr<VariableExpr> superclass;
    std::vector<std::unique_ptr<FunctionStmt>> methods;
};

class TryStmt : public Stmt {
public:
    TryStmt(std::vector<std::unique_ptr<Stmt>> tryBlock, Token exceptionVar, std::vector<std::unique_ptr<Stmt>> catchBlock)
        : tryBlock(std::move(tryBlock)), exceptionVar(exceptionVar), catchBlock(std::move(catchBlock)) {}
    void accept(Visitor& visitor) override;
    std::vector<std::unique_ptr<Stmt>> tryBlock;
    Token exceptionVar;
    std::vector<std::unique_ptr<Stmt>> catchBlock;
};

class ThrowStmt : public Stmt {
public:
    ThrowStmt(std::unique_ptr<Expr> expression)
        : expression(std::move(expression)) {}
    void accept(Visitor& visitor) override;
    std::unique_ptr<Expr> expression;
};

class ImportStmt : public Stmt {
public:
    ImportStmt(Token keyword, std::string path)
        : keyword(keyword), path(std::move(path)) {}
    void accept(Visitor& visitor) override;
    Token keyword;
    std::string path; // resolved string literal content
};

class ForEachStmt : public Stmt {
public:
    ForEachStmt(Token name, std::unique_ptr<Expr> iterable, std::vector<std::unique_ptr<Stmt>> body)
        : name(name), iterable(std::move(iterable)), body(std::move(body)) {}
    void accept(Visitor& visitor) override;
    Token name;   // loop variable
    std::unique_ptr<Expr> iterable;
    std::vector<std::unique_ptr<Stmt>> body;
};

class BreakStmt : public Stmt {
public:
    BreakStmt(Token keyword) : keyword(keyword) {}
    void accept(Visitor& visitor) override;
    Token keyword;
};

class ContinueStmt : public Stmt {
public:
    ContinueStmt(Token keyword) : keyword(keyword) {}
    void accept(Visitor& visitor) override;
    Token keyword;
};

class Visitor {
public:
    virtual void visitBinaryExpr(BinaryExpr* expr) = 0;
    virtual void visitLogicalExpr(LogicalExpr* expr) = 0;
    virtual void visitLiteralExpr(LiteralExpr* expr) = 0;
    virtual void visitVariableExpr(VariableExpr* expr) = 0;
    virtual void visitAssignExpr(AssignExpr* expr) = 0;
    virtual void visitUnaryExpr(UnaryExpr* expr) = 0;
    virtual void visitCallExpr(CallExpr* expr) = 0;
    virtual void visitArrayExpr(ArrayExpr* expr) = 0;
    virtual void visitMapExpr(MapExpr* expr) = 0;
    virtual void visitIndexExpr(IndexExpr* expr) = 0;
    virtual void visitIndexAssignExpr(IndexAssignExpr* expr) = 0;
    virtual void visitGetExpr(GetExpr* expr) = 0;
    virtual void visitSetExpr(SetExpr* expr) = 0;
    virtual void visitThisExpr(ThisExpr* expr) = 0;
    virtual void visitSuperExpr(SuperExpr* expr) = 0;
    
    virtual void visitExpressionStmt(ExpressionStmt* stmt) = 0;
    virtual void visitPrintStmt(PrintStmt* stmt) = 0;
    virtual void visitVarStmt(VarStmt* stmt) = 0;
    virtual void visitBlockStmt(BlockStmt* stmt) = 0;
    virtual void visitIfStmt(IfStmt* stmt) = 0;
    virtual void visitWhileStmt(WhileStmt* stmt) = 0;
    virtual void visitFunctionStmt(FunctionStmt* stmt) = 0;
    virtual void visitReturnStmt(ReturnStmt* stmt) = 0;
    virtual void visitClassStmt(ClassStmt* stmt) = 0;
    virtual void visitTryStmt(TryStmt* stmt) = 0;
    virtual void visitThrowStmt(ThrowStmt* stmt) = 0;
    virtual void visitImportStmt(ImportStmt* stmt) = 0;
    virtual void visitForEachStmt(ForEachStmt* stmt) = 0;
    virtual void visitBreakStmt(BreakStmt* stmt) = 0;
    virtual void visitContinueStmt(ContinueStmt* stmt) = 0;
    virtual void visitTernaryExpr(TernaryExpr* expr) = 0;
};

#endif

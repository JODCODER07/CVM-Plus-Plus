#ifndef cvm_parser_h
#define cvm_parser_h

#include "lexer.h"
#include "ast.h"
#include <vector>
#include <list>
#include <memory>
#include <stdexcept>

class ParseError : public std::runtime_error {
public:
    ParseError(const char* message) : std::runtime_error(message) {}
};

class Parser {
public:
    Parser(Lexer& lexer);
    std::vector<std::unique_ptr<Stmt>> parse();
    bool hasError() const { return hadError; }
    std::unique_ptr<Expr> parseOneExpr(); // for sub-parsing interpolated string expressions

private:
    std::unique_ptr<Stmt> declaration();
    std::unique_ptr<Stmt> classDeclaration();
    std::unique_ptr<Stmt> varDeclaration();
    std::unique_ptr<Stmt> function(const char* kind);
    std::unique_ptr<Stmt> statement();
    std::unique_ptr<Stmt> ifStatement();
    std::unique_ptr<Stmt> whileStatement();
    std::unique_ptr<Stmt> printStatement();
    std::unique_ptr<Stmt> returnStatement();
    std::unique_ptr<Stmt> tryStatement();
    std::unique_ptr<Stmt> throwStatement();
    std::unique_ptr<Stmt> importStatement();
    std::unique_ptr<Stmt> forEachStatement();
    std::unique_ptr<Stmt> breakStatement();
    std::unique_ptr<Stmt> continueStatement();
    std::vector<std::unique_ptr<Stmt>> block();
    std::unique_ptr<Stmt> expressionStatement();
    
    std::unique_ptr<Expr> expression();
    std::unique_ptr<Expr> assignment();
    std::unique_ptr<Expr> ternary();  // new level: a ? b : c
    std::unique_ptr<Expr> logic_or();
    std::unique_ptr<Expr> logic_and();
    std::unique_ptr<Expr> equality();
    std::unique_ptr<Expr> comparison();
    std::unique_ptr<Expr> term();
    std::unique_ptr<Expr> factor();
    std::unique_ptr<Expr> unary();
    std::unique_ptr<Expr> call();
    std::unique_ptr<Expr> finishCall(std::unique_ptr<Expr> callee);
    std::unique_ptr<Expr> finishIndex(std::unique_ptr<Expr> object);
    std::unique_ptr<Expr> primary();

    void advance();
    bool check(TokenType type);
    bool match(TokenType type);
    void consume(TokenType type, const char* message);
    void synchronize();

    Lexer& lexer;
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
    
    void errorAtCurrent(const char* message);
    void error(const char* message);
    void errorAt(Token* token, const char* message);

    // String interpolation helpers
    std::unique_ptr<Expr> buildInterpolation(const std::string& raw, int line);
    std::list<std::string> interpSources; // keeps sub-expr source strings alive for their Lexers, list prevents reallocation moves
};

#endif
